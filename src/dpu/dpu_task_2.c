#include "common.h"
#include <alloc.h>
#include <assert.h>
#include <barrier.h>
#include <defs.h>
#include <inttypes.h>
#include <iso646.h>
#include <mram.h>
#include <mutex.h>
#include <stdint.h>
#include <stdio.h>

#ifndef BLOCK_LOG2
#define BLOCK_LOG2 10
#endif
#ifndef RECORD_SIZE
#define RECORD_SIZE 32
#endif

#define BLOCK (1 << BLOCK_LOG2)
#define GROUP 8
#define INDEX_BLOCK 8
#define BLOCK_PROCESS (GROUP * RECORD_SIZE)

BARRIER_INIT(my_barrier, NR_TASKLETS);

__host dpu_args_t args;
// I dont put the out host variable as it will be in the MRAM at the position
// database size + indexing array size Also I dont put the shared_cache as it
// will be in the MRAM at the position database size + indexing array size

int select_xor();
int main() {

  uint8_t idx = me();

  if (idx == 0)
    mem_reset();
  barrier_wait(&my_barrier);

  // Get the size of the database and the indexing array
  uint32_t db_size = args.database_size_bytes;
  uint32_t idv_size = args.input_indexing_size_bytes;

  // Get the address of the database and the indexing array
  uintptr_t mram_addr_db = (uintptr_t)DPU_MRAM_HEAP_POINTER;
  uintptr_t mram_base_index = (uintptr_t)(DPU_MRAM_HEAP_POINTER + db_size);
  uintptr_t mram_addr_out =
      (uintptr_t)(DPU_MRAM_HEAP_POINTER + db_size + idv_size);

  uint8_t *selectors = mem_alloc(INDEX_BLOCK);
  uint8_t *cache = mem_alloc(BLOCK);

  int tasklet = idx * BLOCK_PROCESS;

  for (int byte = tasklet; byte < db_size;
       byte += BLOCK_PROCESS * NR_TASKLETS) {

    uintptr_t db_addr = mram_addr_db + byte;

    select_xor(db_addr, selectors);
  }

  return 0;
}

int select_xor(uintptr_t db_addr, uint8_t *selectors, uint8_t *cache) {

  for (size_t i = 0; i < GROUP; i++) {
    uint64_t temp = selectors[i / 8];
    for (int j = 0; j < 8; j++) {
      uint64_t bit = (temp >> j) & 1;
      int64_t mask_val = (bit == 1) ? -1LL : 0LL;
    }
  }
  return 0;
}
