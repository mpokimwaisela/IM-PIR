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
#define BLOCK_LOG2 11
#endif
#define BLOCK (1 << BLOCK_LOG2)
#define NUM_ELEMENTS (BLOCK / sizeof(uintX_t)) // This is 256/32 = 8
#define GROUP_SIZE    (8 * sizeof(uintX_t))
#define BLOCK_INDEX   ((BLOCK + GROUP_SIZE - 1) / GROUP_SIZE)

#ifndef SIZE
#define SIZE 4
#endif

BARRIER_INIT(my_barrier, NR_TASKLETS);

typedef struct {
  int64_t w[SIZE];
} uintX_t;

__host dpu_args_t args;
__host uintX_t out;

uintX_t shared[NR_TASKLETS];

static inline uintX_t and_uintX_t(uintX_t op1, const uintX_t op2);
static inline uintX_t xor_uintX_t(uintX_t op1, const uintX_t op2);
static inline uintX_t set1_uintX_t_epi64x(int64_t value);
static inline void select_xor(uintX_t *data, uint8_t *index, uint32_t size);
static inline void test_and_uintX_t();
static inline void test_xor_uintX_t();

int main() {

  uint8_t tasklet_id = me();

  if (tasklet_id == 0) {
#if TEST_XOR_AND
    test_and_uintX_t();
    test_xor_uintX_t();
#endif
    // printf("tasklet %u: stack = %u\n", me(), check_stack());

    mem_reset();
  }
  barrier_wait(&my_barrier);

  shared[tasklet_id] = set1_uintX_t_epi64x(0);

  uint32_t db_size = args.database_size_bytes;
  uint32_t input_indexing_size = args.input_indexing_size_bytes;

  uintptr_t mram_addr_db = (uintptr_t)DPU_MRAM_HEAP_POINTER;
  uintptr_t mram_base_index = (uintptr_t)(DPU_MRAM_HEAP_POINTER + db_size);

  uintX_t *cache_data = mem_alloc(BLOCK);
  uint8_t *cache_index = mem_alloc(BLOCK_INDEX);

  uint32_t tasklet = tasklet_id << BLOCK_LOG2;

  for (int idx = tasklet; idx < db_size; idx += BLOCK * NR_TASKLETS) {

    uint32_t bytes = (idx + BLOCK >= db_size) ? (db_size - idx) : BLOCK;

    mram_read((__mram_ptr void const *)(mram_addr_db + idx), cache_data, bytes);

    uint32_t idx_o = idx / 256;
    uint32_t idx_l = (bytes / 256);

    if (idx_l > 0) {

      uint32_t desired_addr = mram_base_index + idx_o;

      if (idx_l < 8) {
        uint32_t aligned_addr = desired_addr & ~7UL; // Clear the lower 3 bits
        uint32_t offset_in_word = desired_addr - aligned_addr;
        uint64_t word;

        mram_read((__mram_ptr void const *)aligned_addr, &word, 8);
        uint8_t index_byte = (word >> (offset_in_word * 8)) & 0xFF;
        cache_index[0] = index_byte;

      } else {
        mram_read((__mram_ptr void const *)desired_addr, cache_index, idx_l);
      }
    } else
      break;

    select_xor(cache_data, cache_index, bytes / sizeof(uintX_t));
  }

  barrier_wait(&my_barrier);

  if (tasklet_id == 0) {
    out = set1_uintX_t_epi64x(0);
    for (int t = 0; t < NR_TASKLETS; t++) {
      out = xor_uintX_t(out, shared[t]);
    }
  }
}

static inline void select_xor(uintX_t *data, uint8_t *index, uint32_t size) {
  uint8_t idx = me();
  uintX_t results[8];

  for (int i = 0; i < 8; i++) {
    results[i] = set1_uintX_t_epi64x(0);
  }
  for (uint32_t i = 0; i < size; i += 8) {
    uint64_t tmp = index[i / 8];
    for (uint32_t k = 0; k < 8; k++) {
      uint64_t bit = (tmp >> k) & 1;
      int64_t mask_val = (bit == 1) ? -1LL : 0LL;
      results[k] = xor_uintX_t(
          results[k], and_uintX_t(data[i + k], set1_uintX_t_epi64x(mask_val)));
    }
  }

  uintX_t final;
  final = xor_uintX_t(results[0], results[1]);
  final = xor_uintX_t(final, results[2]);
  final = xor_uintX_t(final, results[3]);
  final = xor_uintX_t(final, results[4]);
  final = xor_uintX_t(final, results[5]);
  final = xor_uintX_t(final, results[6]);
  final = xor_uintX_t(final, results[7]);

  shared[idx] = xor_uintX_t(shared[idx], final);
}

static inline uintX_t xor_uintX_t(uintX_t op1, const uintX_t op2) {
  uintX_t res;

  for (int i = 0; i < SIZE; i++)
    res.w[i] = op1.w[i] ^ op2.w[i];

  return res;
}

static inline uintX_t and_uintX_t(uintX_t op1, const uintX_t op2) {
  uintX_t res;

  for (int i = 0; i < SIZE; i++)
    res.w[i] = op1.w[i] & op2.w[i];

  return res;
}

static inline uintX_t set1_uintX_t_epi64x(int64_t value) {
  uintX_t res;
  for (int i = 0; i < SIZE; i++)
    res.w[i] = value;

  return res;
}

static inline void test_xor_uintX_t() {
  uintX_t op1 = {{1, 2, 3, 4}};
  uintX_t op2 = {{4, 3, 2, 1}};
  uintX_t expected = {{5, 1, 1, 5}};
  uintX_t result = xor_uintX_t(op1, op2);

  assert(result.w[0] == expected.w[0]);
  assert(result.w[1] == expected.w[1]);
  assert(result.w[2] == expected.w[2]);
  assert(result.w[3] == expected.w[3]);

  printf("test_xor_uintX_t passed\n");
}

static inline void test_and_uintX_t() {
  uintX_t op1 = {{1, 2, 3, 4}};
  uintX_t op2 = {{4, 3, 2, 1}};
  uintX_t expected = {{0, 2, 2, 0}};
  uintX_t result = and_uintX_t(op1, op2);

  assert(result.w[0] == expected.w[0]);
  assert(result.w[1] == expected.w[1]);
  assert(result.w[2] == expected.w[2]);
  assert(result.w[3] == expected.w[3]);

  printf("test_and_uintX_t passed\n");
}