#include "datastore.h"

#include <cassert>
#include <cstdio>

datastore::db_record
datastore::answer_pir(const std::vector<uint8_t> &indexing) const {
  db_record result = _mm256_set_epi64x(0, 0, 0, 0);
  db_record results[8] = {
      {result}, {result}, {result}, {result},
      {result}, {result}, {result}, {result},
  };
  assert(data_.size() % 8 == 0);

  for (size_t i = 0; i < data_.size(); i += 8) {

    uint64_t tmp = indexing[i / 8];
    results[0] = _mm256_xor_si256(results[0],_mm256_and_si256(data_[i + 0], _mm256_set1_epi64x(-((tmp >> 0) & 1))));
    results[1] = _mm256_xor_si256(results[1],_mm256_and_si256(data_[i + 1], _mm256_set1_epi64x(-((tmp >> 1) & 1))));
    results[2] = _mm256_xor_si256(results[2],_mm256_and_si256(data_[i + 2], _mm256_set1_epi64x(-((tmp >> 2) & 1))));
    results[3] = _mm256_xor_si256(results[3],_mm256_and_si256(data_[i + 3], _mm256_set1_epi64x(-((tmp >> 3) & 1))));
    results[4] = _mm256_xor_si256(results[4],_mm256_and_si256(data_[i + 4], _mm256_set1_epi64x(-((tmp >> 4) & 1))));
    results[5] = _mm256_xor_si256(results[5],_mm256_and_si256(data_[i + 5], _mm256_set1_epi64x(-((tmp >> 5) & 1))));
    results[6] = _mm256_xor_si256(results[6],_mm256_and_si256(data_[i + 6], _mm256_set1_epi64x(-((tmp >> 6) & 1))));
    results[7] = _mm256_xor_si256(results[7],_mm256_and_si256(data_[i + 7], _mm256_set1_epi64x(-((tmp >> 7) & 1))));

  }

  result = _mm256_xor_si256(results[0], results[1]);
  result = _mm256_xor_si256(result, results[2]);
  result = _mm256_xor_si256(result, results[3]);
  result = _mm256_xor_si256(result, results[4]);
  result = _mm256_xor_si256(result, results[5]);
  result = _mm256_xor_si256(result, results[6]);
  result = _mm256_xor_si256(result, results[7]);
  return result;
}