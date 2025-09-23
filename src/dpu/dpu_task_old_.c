#include <stdio.h>
#include <defs.h>
#include <mram.h>
#include <barrier.h>
#include <alloc.h>
#include <stdint.h>
#include "common.h"

BARRIER_INIT(my_barrier, NR_TASKLETS);

// 256-bit type: 4 x 64-bit
typedef struct {
    uint64_t w[4];
} uintX_t;

__host dpu_args_t args;   // from the host: includes database_size_bytes, input_indexing_size_bytes, etc.
__host uintX_t out;       // final 256-bit result on host side

__dma_aligned uintX_t shared_cache[NR_TASKLETS]; // partial accumulators

// -----------------------------------------------------------------------------
// Some helpers for 256-bit operations
static inline uintX_t xor_uintX_t(uintX_t a, uintX_t b)
{
    uintX_t r;
    r.w[0] = a.w[0] ^ b.w[0];
    r.w[1] = a.w[1] ^ b.w[1];
    r.w[2] = a.w[2] ^ b.w[2];
    r.w[3] = a.w[3] ^ b.w[3];
    return r;
}

static inline uintX_t set1_uintX_t_epi64x(uint64_t val)
{
    uintX_t r;
    r.w[0] = r.w[1] = r.w[2] = r.w[3] = val;
    return r;
}

// AND a 256-bit value with a 64-bit mask repeated over all 4 words
static inline uintX_t and_uintX_t_64mask(const uintX_t *in, uint64_t mask)
{
    uintX_t r;
    r.w[0] = in->w[0] & mask;
    r.w[1] = in->w[1] & mask;
    r.w[2] = in->w[2] & mask;
    r.w[3] = in->w[3] & mask;
    return r;
}

// -----------------------------------------------------------------------------
// bitwise_selection_xor:
//   For count_indexing indexing bytes, we have count_indexing * 8 data words (each 256 bits).
//   Each indexing byte => 8 bits => selects among 8 data words.
static void bitwise_selection_xor(
    uintX_t       *data,       // [count_indexing * 8] 256-bit words
    const uint8_t *indexing,   // [count_indexing] bytes
    uint32_t       count_indexing,
    uint32_t       tasklet_id
) {
    uintX_t accum = set1_uintX_t_epi64x(0ULL);

    for (uint32_t i = 0; i < count_indexing; i++) {
        uint8_t tmp = indexing[i];
        // For each of 8 bits in tmp
        for (int b = 0; b < 8; b++) {
            // mask = 0xFFFFFFFFFFFFFFFF if bit=1, else 0
            uint64_t mask = -((tmp >> b) & 1ULL);

            // data offset = i*8 + b
            uintX_t masked = and_uintX_t_64mask(&data[i*8 + b], mask);
            accum = xor_uintX_t(accum, masked);
        }
    }

    // XOR into shared_cache
    shared_cache[tasklet_id] = xor_uintX_t(shared_cache[tasklet_id], accum);
}

// -----------------------------------------------------------------------------
// main DPU program
int main()
{
    uint32_t tasklet_id = me();
    if (tasklet_id == 0) {
        mem_reset();
    }
    barrier_wait(&my_barrier);

    // Initialize partial accumulators
    shared_cache[tasklet_id] = set1_uintX_t_epi64x(0ULL);

    // The host sets these:
    //   args.database_size_bytes => size of the data region
    //   args.input_indexing_size_bytes => size of the indexing region
    // We assume data is laid out at [0..database_size-1], indexing at [database_size..database_size + indexing_size -1]
    uint32_t database_size       = args.database_size_bytes;
    uint32_t input_indexing_size = args.input_indexing_size_bytes;

    // MRAM base addresses
    uint32_t mram_data_base     = (uint32_t)DPU_MRAM_HEAP_POINTER;
    uint32_t mram_indexing_base = mram_data_base + database_size;

    // We can allocate up to 2048 bytes. We need:
    //   indexing_chunk_bytes + (indexing_chunk_bytes * 256) <= 2048
    // => indexing_chunk_bytes*(256 + 1) <= 2048 => indexing_chunk_bytes <= floor(2048/257) => 7
    #define INDEXING_CHUNK_BYTES  7
    #define DATA_CHUNK_BYTES      (INDEXING_CHUNK_BYTES * 256)  // 7*256=1792
    // total = 7 + 1792 = 1799 < 2048

    uint8_t *cache_indexing = mem_alloc(INDEXING_CHUNK_BYTES); // up to 7 bytes
    uintX_t *cache_data     = mem_alloc(DATA_CHUNK_BYTES);     // up to 1792 bytes

    // We'll iterate over all indexing bytes in increments of INDEXING_CHUNK_BYTES * NR_TASKLETS
    for (uint32_t idx_offset = tasklet_id * INDEXING_CHUNK_BYTES;
         idx_offset < input_indexing_size;
         idx_offset += INDEXING_CHUNK_BYTES * NR_TASKLETS)
    {
        // how many indexing bytes remain for this chunk
        uint32_t chunk_this_time = (idx_offset + INDEXING_CHUNK_BYTES > input_indexing_size)
                                     ? (input_indexing_size - idx_offset)
                                     : INDEXING_CHUNK_BYTES;

        if (chunk_this_time == 0)
            break; // no more indexing to read

        // Read 'chunk_this_time' indexing bytes from MRAM
        mram_read(
            (__mram_ptr void const *)(mram_indexing_base + idx_offset),
            cache_indexing,
            chunk_this_time
        );

        // For data: each indexing byte => 256 data bytes
        uint32_t data_bytes_to_read = chunk_this_time * 256;
        // The data offset is idx_offset * 256
        //   if idx_offset is in bytes of indexing
        mram_read(
            (__mram_ptr void const *)(mram_data_base + ((uint64_t)idx_offset * 256)),
            cache_data,
            data_bytes_to_read
        );

        // chunk_this_time indexing bytes => chunk_this_time*8 data words
        bitwise_selection_xor(cache_data, cache_indexing, chunk_this_time, tasklet_id);
    }

    barrier_wait(&my_barrier);

    // Reduce partial results
    if (tasklet_id == 0) {
        out = set1_uintX_t_epi64x(0ULL);
        for (int t = 0; t < NR_TASKLETS; t++) {
            out = xor_uintX_t(out, shared_cache[t]);
        }
        // 'out' now contains the final 256-bit result
    }
    // printf("DPU %d done\n", tasklet_id);
    return 0;
}