#include "common.h"
#include <alloc.h>
#include <barrier.h>
#include <defs.h>
#include <inttypes.h>
#include <iso646.h>
#include <mram.h>
#include <seqread.h>
#include <stdint.h>

#ifndef SIZE
#define SIZE 4                               
#endif

typedef struct { int64_t w[SIZE]; } uint256_t;

#ifndef BLOCK_LOG2
#define BLOCK_LOG2 12                      
#endif
#define BLOCK       (1U << BLOCK_LOG2)
#define GROUP_SIZE  (8 * sizeof(uint256_t))  // 1 index byte per 8 records

#ifndef MAX_BATCH
#define MAX_BATCH 32
#endif

// Tile batches. 4 or 8 is a good trade-off.
#ifndef B_TILE
#define B_TILE 4
#endif

static inline void xor256(uint256_t *d, const uint256_t *s) {
    for (int i = 0; i < SIZE; i++) d->w[i] ^= s->w[i];
}

BARRIER_INIT(my_barrier, NR_TASKLETS);

__host dpu_args_t args;                      
__host uint256_t out[MAX_BATCH];                    

static uint256_t shared[MAX_BATCH][NR_TASKLETS];

int main(void)
{
    const uint8_t tid = me();

    mem_reset();
    barrier_wait(&my_barrier);

    const uint32_t  db_size     = args.database_size_bytes;
    uint32_t        num_batches = args.num_batches;
    if (num_batches == 0) num_batches = 1;
    if (num_batches > MAX_BATCH) num_batches = MAX_BATCH;

    const uintptr_t data_base   = (uintptr_t)DPU_MRAM_HEAP_POINTER;
    const uintptr_t index_base  = data_base + db_size;

    const uint32_t rec_size      = sizeof(uint256_t);
    const uint32_t total_records = db_size / rec_size;           
    const uint32_t index_stride  = (total_records + 7) >> 3;    

    // One streaming cache for database records
    seqreader_buffer_t data_cache = seqread_alloc();
    seqreader_t        dr;

    // Per-tile bitmap readers (tiny footprint)
    seqreader_buffer_t idx_cache[B_TILE];
    seqreader_t        ir[B_TILE];
    for (uint32_t tb = 0; tb < B_TILE; ++tb)
        idx_cache[tb] = seqread_alloc();

    // Local accumulators for a tile of batches
    uint256_t acc[B_TILE];

    // Process batches in tiles to limit inner-loop footprint
    for (uint32_t base_b = 0; base_b < num_batches; base_b += B_TILE) {
        const uint32_t tile_cnt = (base_b + B_TILE <= num_batches) ? B_TILE
                                                                   : (num_batches - base_b);

        // zero the tile accumulators
        for (uint32_t tb = 0; tb < tile_cnt; ++tb)
            acc[tb] = (uint256_t){{0}};

        // Stripe the database across tasklets
        for (uintptr_t off = ((uintptr_t)tid << BLOCK_LOG2);
             off < db_size;
             off += (uintptr_t)BLOCK * NR_TASKLETS)
        {
            const uint32_t bytes   = (off + BLOCK <= db_size) ? BLOCK : (db_size - off);
            const uint32_t rec_cnt = bytes / rec_size;
            if (!rec_cnt) break;

            // stream records
            uint256_t *recp = (uint256_t *)seqread_init(
                data_cache, (__mram_ptr void *)(data_base + off), &dr);

            // stream index bytes for this block for each batch in the tile
            const uintptr_t idx_off = (off / GROUP_SIZE);
            uint8_t *idxp[B_TILE];
            for (uint32_t tb = 0; tb < tile_cnt; ++tb) {
                const uint32_t b = base_b + tb;
                idxp[tb] = (uint8_t *)seqread_init(
                    idx_cache[tb],
                    (__mram_ptr void *)(index_base + (uintptr_t)b * index_stride + idx_off),
                    &ir[tb]);
            }

            // Iterate this block in groups of up to 8 records (one index byte)
            uint32_t i = 0;
            while (i < rec_cnt) {
                // fetch one index byte per batch in the tile
                uint8_t idx_byte[B_TILE];
                for (uint32_t tb = 0; tb < tile_cnt; ++tb) {
                    idx_byte[tb] = *idxp[tb];
                }
                // advance bitmap streams for next group if more records remain
                if (i + 8 < rec_cnt) {
                    for (uint32_t tb = 0; tb < tile_cnt; ++tb)
                        idxp[tb] = (uint8_t *)seqread_get(idxp[tb], 1, &ir[tb]);
                }

                // up to 8 records under these bits
                for (int k = 0; k < 8 && i < rec_cnt; ++k, ++i) {
                    // load record words once
                    int64_t w0 = recp->w[0];
                    int64_t w1 = recp->w[1];
                    int64_t w2 = recp->w[2];
                    int64_t w3 = recp->w[3];

                    // apply to each batch in the tile
                    for (uint32_t tb = 0; tb < tile_cnt; ++tb) {
                        const uint64_t umask = 0U - (uint64_t)((idx_byte[tb] >> k) & 1U);
                        const int64_t  mask  = (int64_t)umask;
                        acc[tb].w[0] ^= (w0 & mask);
                        acc[tb].w[1] ^= (w1 & mask);
                        acc[tb].w[2] ^= (w2 & mask);
                        acc[tb].w[3] ^= (w3 & mask);
                    }

                    // advance record stream
                    if (i + 1 <= rec_cnt)
                        recp = (uint256_t *)seqread_get(recp, sizeof(*recp), &dr);
                }
            }
        }

        // Write this tile's partials to shared and reduce across tasklets
        for (uint32_t tb = 0; tb < tile_cnt; ++tb) {
            const uint32_t b = base_b + tb;
            shared[b][tid] = acc[tb];
        }
        barrier_wait(&my_barrier);

        for (int step = NR_TASKLETS >> 1; step > 0; step >>= 1) {
            if (tid < step) {
                for (uint32_t b = base_b; b < base_b + tile_cnt; ++b)
                    xor256(&shared[b][tid], &shared[b][tid + step]);
            }
            barrier_wait(&my_barrier);
        }

        if (tid == 0) {
            for (uint32_t b = base_b; b < base_b + tile_cnt; ++b)
                out[b] = shared[b][0];
        }

        barrier_wait(&my_barrier); // keep tiles ordered
    }

    return 0;
}
