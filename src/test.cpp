#include "./dpf/dpf.h"
#include "datastore.h"
#include "dpu/common.h"
#include <cstddef>
#include <chrono>
#include <iostream>
#include <cstring>


int testCPU() {
  size_t N = 20;
  datastore store;
  store.reserve(1ULL << N);
  for (size_t i = 0; i < (1ULL << N); i++) {
    store.push_back(_mm256_set_epi64x(i, i, i, i));
  }

  auto keys = DPF::Gen(123456, N);
  auto a = keys.first;
  auto b = keys.second;
  std::vector<uint8_t> aaaa = DPF::EvalFull8(a, N);
  std::vector<uint8_t> bbbb = DPF::EvalFull8(b, N);
  datastore::db_record answerA = store.answer_pir(aaaa);
  datastore::db_record answerB = store.answer_pir(bbbb);
  datastore::db_record answer = _mm256_xor_si256(answerA, answerB);
  if (_mm256_extract_epi64(answer, 0) == 123456) {
    return 0;
  } else {
    std::cout << "PIR answer wrong\n";
    return -1;
  }
}

#ifdef ENABLE_PIM
#include <dpu>
using namespace dpu;
#define BINARY_NAME "dpu_task"
void setup(size_t NUM_ELEM,size_t NUM_DPUS, dpu::DpuSet **dpu_set, std::vector<dpu_args_t> &args) {
  try {

    *dpu_set = new dpu::DpuSet(dpu::DpuSet::allocate(NUM_DPUS));
    (*dpu_set)->load(BINARY_NAME);

    size_t data_per_dpu = (NUM_ELEM + NUM_DPUS - 1) / NUM_DPUS;
    size_t database_size_per_dpu_bytes = data_per_dpu * sizeof(datastore::db_record);
    args[0].database_size_bytes = database_size_per_dpu_bytes;
    args[0].num_batches = 1;

    std::vector<std::vector<uint64_t>> dpu_store(NUM_DPUS);

    for (size_t i = 0; i < NUM_DPUS; i++) {
      size_t start = i * data_per_dpu;
      size_t end = std::min(start + data_per_dpu, NUM_ELEM);
      for (size_t j = start; j < end; j++) {
        dpu_store[i].push_back(j);
        dpu_store[i].push_back(j);
        dpu_store[i].push_back(j);
        dpu_store[i].push_back(j);
      }
    }
      (*dpu_set)->copy(DPU_MRAM_HEAP_POINTER_NAME, 0, dpu_store);

  } catch (const DpuError &e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }
}


datastore::db_record execution_pim(size_t N, std::vector<uint8_t> aaaa, size_t NUM_DPUS, dpu::DpuSet *dpu_set, std::vector<dpu_args_t> &args) {
  std::vector<std::vector<uint8_t>> output_vectors(NUM_DPUS, std::vector<uint8_t>(32, 0));

  size_t elements_per_dpu = (aaaa.size() + NUM_DPUS - 1) / NUM_DPUS;
  std::vector<std::vector<uint8_t>> dpu_input_vectors(NUM_DPUS);

#pragma omp parallel for
  for (size_t i = 0; i < NUM_DPUS; i++) {
    auto start = aaaa.begin() + std::min(i * elements_per_dpu, aaaa.size());
    auto end = (i == NUM_DPUS - 1) ? aaaa.end() : aaaa.begin() + std::min((i + 1) * elements_per_dpu, aaaa.size());
    dpu_input_vectors[i] = std::vector<uint8_t>(start, end);
  }
  args[0].input_indexing_size_bytes = elements_per_dpu;

  dpu_set->copy("args", args);
  dpu_set->copy(DPU_MRAM_HEAP_POINTER_NAME, args[0].database_size_bytes, dpu_input_vectors);
  dpu_set->exec();
  dpu_set->copy(output_vectors, "out");

  datastore::db_record dpu_result = _mm256_setzero_si256();

  for (const auto &dpu_output : output_vectors) {
    datastore::db_record partial;
    std::memcpy(&partial, dpu_output.data(), sizeof(datastore::db_record));
    dpu_result = _mm256_xor_si256(dpu_result, partial);
  }

  return dpu_result;
}


int testPIM() {
  size_t N = 20;
  size_t num_dpus = 256;
  dpu::DpuSet *dpu_set = nullptr;
  std::vector<dpu_args_t> args(1);
  setup(1ULL << N, num_dpus, &dpu_set, args);

  auto keys = DPF::Gen(5, N);
  auto a = keys.first;
  auto b = keys.second;
  std::vector<uint8_t> aaaa = DPF::EvalFull8(a, N);
  std::vector<uint8_t> bbbb = DPF::EvalFull8(b, N);

  datastore::db_record answerA = execution_pim(N, aaaa, num_dpus, dpu_set, args);
  datastore::db_record answerB = execution_pim(N, bbbb, num_dpus, dpu_set, args);
  datastore::db_record answer = _mm256_xor_si256(answerA, answerB);
  if (_mm256_extract_epi64(answer, 0) == 5) {
    return 0;
  } else {
    std::cout << "PIR answer wrong\n";
    return -1;
  }
}

#endif

int main(int argc, char **argv) {
  int res = 0;
  res |= testCPU();
#ifdef ENABLE_PIM
  res |= testPIM();
#endif
  return res;
}