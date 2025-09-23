#include "common.h"
#include <alloc.h>
#include <barrier.h>
#include <defs.h>
#include <inttypes.h>
#include <iso646.h>
#include <mram.h>
#include <seqread.h>
#include <stdint.h>

#ifndef BLOCK_LOG2
#define BLOCK_LOG2 8               
#endif
#define BLOCK       (1U << BLOCK_LOG2)
#define GROUP_SIZE  (8 * sizeof(uintX_t)) 

#ifndef SIZE
#define SIZE 4                       
#endif

typedef struct { int64_t w[SIZE]; } uintX_t;


BARRIER_INIT(my_barrier, NR_TASKLETS);
__host dpu_args_t args;
__host uintX_t    out[32];
static uintX_t    shared[NR_TASKLETS];


int main(void)
{
    const uint8_t tid = me();

    
    mem_reset();
    barrier_wait(&my_barrier);

    uintX_t acc = {{0}};                 

    
    const uint32_t  db_size    = args.database_size_bytes;
    const uintptr_t data_base  = (uintptr_t)DPU_MRAM_HEAP_POINTER;
    const uintptr_t index_base = data_base + db_size;

    
    seqreader_buffer_t data_cache  = seqread_alloc();
    seqreader_buffer_t index_cache = seqread_alloc();
    seqreader_t        dr, ir;

    
    for (uintptr_t off = ((uintptr_t)tid << BLOCK_LOG2);
         off < db_size;
         off += (uintptr_t)BLOCK * NR_TASKLETS)
    {
       
        const uint32_t bytes  = (off + BLOCK <= db_size) ? BLOCK
                                                        : (db_size - off);
        const uint32_t rec_cnt = bytes / sizeof(uintX_t);
        if (!rec_cnt) break;

        
        const uint32_t idx_cnt = (rec_cnt + 7) >> 3;

        
        uintX_t *recp = (uintX_t *)seqread_init(
            data_cache,
            (__mram_ptr void *)(data_base + off),
            &dr);

        uint8_t *idxp = (uint8_t *)seqread_init(
            index_cache,
            (__mram_ptr void *)(index_base + (off / GROUP_SIZE)),
            &ir);

        
        uint8_t curr_idx = *idxp;
        uint8_t bit_pos  = 0;
        uint32_t idx_used = 1;           

        
        for (uint32_t i = 0; i < rec_cnt; ++i)
        {
            
            if (bit_pos == 8) {
                idxp      = (uint8_t *)seqread_get(idxp, 1, &ir);
                curr_idx  = *idxp;
                bit_pos   = 0;
                ++idx_used;
            }

            const int64_t mask = -((int64_t)((curr_idx >> bit_pos) & 1));
            ++bit_pos;

            
            for (int j = 0; j < SIZE; ++j)
                acc.w[j] ^= recp->w[j] & mask;

            
            if (i + 1 < rec_cnt)
                recp = (uintX_t *)seqread_get(recp, sizeof(*recp), &dr);
        }

        // sanity: we should have consumed exactly idx_cnt bytes
        (void)idx_used; /* (could assert in debug) */
    }

    shared[tid] = acc;
    barrier_wait(&my_barrier);

    if (tid == 0) {
        out[0] = (uintX_t){{0}};
        for (int t = 0; t < NR_TASKLETS; ++t)
            for (int j = 0; j < SIZE; ++j)
                out[0].w[j] ^= shared[t].w[j];
    }
    return 0;
}