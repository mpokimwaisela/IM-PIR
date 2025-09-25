#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include <x86intrin.h>

#include "util/alignment_allocator.h"

class datastore {
public:
  typedef __m256i db_record;

  datastore() = default;

  void reserve(size_t n) { data_.reserve(n); }
  void resize(size_t n) { data_.resize(n); }
  void push_back(const db_record &data) { data_.push_back(data); }
  void push_back(db_record &&data) { data_.push_back(data); }

  void dummy(size_t n) { data_.resize(n, _mm256_set_epi64x(1, 2, 3, 4)); }

  size_t size() const { return data_.size(); }

  db_record answer_pir(const std::vector<uint8_t> &indexing) const;

private:
  std::vector<db_record, AlignmentAllocator<db_record, sizeof(db_record)>>
      data_;
};
