#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>



typedef struct {
  uint32_t database_size_bytes;
  uint32_t input_indexing_size_bytes;
  uint32_t num_batches;
} dpu_args_t;

#endif // __COMMON_H__