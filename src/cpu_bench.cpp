#include "./dpf/dpf.h"
#include "datastore.h"
#include "util/profiler.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <omp.h>
#include <random>
#include <string>
#include <vector>

#define SIZE_GB (1024 * 1024 * 1024)

using namespace std;

Profiler profiler;

std::map<std::string, std::string> parse_args(int argc, char **argv) {
  std::map<std::string, std::string> args;
  for (int i = 1; i < argc; ++i) {
    std::string token(argv[i]);
    auto pos = token.find('=');
    if (pos != std::string::npos)
      args[token.substr(0, pos)] = token.substr(pos + 1);
  }
  return args;
}

void setup_database(datastore &store, size_t num_elements) {
  for (size_t i = 0; i < num_elements; i++) {
    store.push_back(_mm256_set_epi64x(i, i, i, i));
  }
}

void run_single_query_vectorized(datastore &store, size_t N, size_t reps) {
    profiler.start("DPF.KeyGen");
    auto keys = DPF::Gen(5, N);
    profiler.accumulate("DPF.KeyGen");

    auto key = keys.first;

    profiler.start("DPF.Eval");
    auto query = DPF::EvalFull8(key, N);
    profiler.accumulate("DPF.Eval");

    profiler.start("PIR.CPU");
    store.answer_pir(query);
    profiler.accumulate("PIR.CPU");

  profiler.printAllTimes();
  profiler.reset();
}

void run_single_query_scalar(datastore &store, size_t N, size_t reps) {
  for (size_t i = 0; i < reps; ++i) {
    profiler.start("DPF.KeyGen");
    auto keys = DPF::Gen(5, N);
    profiler.accumulate("DPF.KeyGen");

    auto key = keys.first;

    profiler.start("DPF.Eval");
    auto query = DPF::EvalFull(key, N);
    profiler.accumulate("DPF.Eval");

    profiler.start("PIR.CPU");
    store.answer_pir(query);
    profiler.accumulate("PIR.CPU");
  }

  profiler.printAllTimes(true);
  profiler.reset();
}

// void batch_operation_cpu(size_t N, datastore &store, vector<uint8_t>
// &key,
//                          size_t batch_size, datastore::db_record
//                          *answers) {
// #pragma omp parallel for
//   for (size_t i = 0; i < batch_size; i++) {
//     auto query = DPF::EvalFull(key, N);
//     answers[i] = store.answer_pir(query);
//   }
// }

// void run_batch_query(datastore &store, size_t N, size_t batch_size,
//                      size_t reps) {

//   auto keys = DPF::Gen(5, N);
//   auto key = keys.first;
//   auto base_query = DPF::EvalFull8(key, N);

//   string event_name = "Batch=" + to_string(batch_size);
//   datastore::db_record answers[batch_size];

//   profiler.start(event_name);
//   for (size_t r = 0; r < reps; ++r) {
//     batch_operation_cpu(N, store, base_query, batch_size, answers);
//     profiler.accumulate(event_name);

//   }

//   double throughput = batch_size * 1000.0 /
//   profiler.getAverageTime(event_name); cout << "Throughput (" << event_name
//   << "): " << throughput << " q/s" << endl; profiler.printAllTimes(true);
//   profiler.reset();
// }

void run_batch_query8(datastore &store, size_t N, size_t batch_size,
                      size_t reps) {
  using db_record = datastore::db_record;

  // Step 1: Generate a batch of unique DPF keys
  std::vector<std::vector<uint8_t>> keys(batch_size);
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<size_t> dist(0, N - 1);

  for (size_t i = 0; i < batch_size; ++i) {
    size_t index = dist(rng);
    auto key_pair = DPF::Gen(index, N);
    keys[i] = key_pair.first;
  }
  cout << "Batch size: " << batch_size << endl;

  // Step 2: Allocate answers buffer
  std::vector<db_record, AlignmentAllocator<db_record, sizeof(db_record)>>
      answers(batch_size);

  // Step 3: Run the benchmark
  std::string event_name = "Batch = " + std::to_string(batch_size);
  profiler.start(event_name);

  for (size_t r = 0; r < reps; ++r) {
#pragma omp parallel for
    for (size_t i = 0; i < batch_size; ++i) {
      auto query = DPF::EvalFull8(keys[i], N);
      answers[i] = store.answer_pir(query);
    }
    profiler.accumulate(event_name);
  }

  double avg_ms = profiler.getAverageTime(event_name);
  double throughput = (batch_size * 1000.0) / avg_ms;

  std::cout << "Throughput : " << throughput
            << " DPFs/sec" << std::endl;

  profiler.printAllTimes(true);
  profiler.reset();
}

void run_batch_query(datastore &store, size_t N, size_t batch_size,
                     size_t reps) {
  using db_record = datastore::db_record;

  // Step 1: Generate a batch of unique DPF keys
  std::vector<std::vector<uint8_t>> keys(batch_size);
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<size_t> dist(0, N - 1);

  for (size_t i = 0; i < batch_size; ++i) {
    size_t index = dist(rng);
    auto key_pair = DPF::Gen(index, N);
    keys[i] = key_pair.first;
  }
  cout << "Batch size: " << batch_size << endl;

  // Step 2: Allocate answers buffer
  std::vector<db_record, AlignmentAllocator<db_record, sizeof(db_record)>>
      answers(batch_size);

  // Step 3: Run the benchmark
  std::string event_name = "Batch = " + std::to_string(batch_size);
  profiler.start(event_name);

  for (size_t r = 0; r < reps; ++r) {
#pragma omp parallel for
    for (size_t i = 0; i < batch_size; ++i) {
      auto query = DPF::EvalFull(keys[i], N);
      answers[i] = store.answer_pir(query);
    }
    profiler.accumulate(event_name);
  }

  double avg_ms = profiler.getAverageTime(event_name);
  double throughput = (batch_size * 1000.0) / avg_ms;

  std::cout << "Throughput (" << event_name << "): " << throughput
            << " DPFs/sec" << std::endl;

  profiler.printAllTimes(true);
  profiler.reset();
}

int main(int argc, char **argv) {
  auto args = parse_args(argc, argv);

  if (!args.count("mode") || !args.count("logN")) {
    cerr << "Usage:\n"
         << "  ./cpu_bench mode=single8 logN=24 reps=5\n"
         << "  ./cpu_bench mode=single logN=24 reps=5\n"
         << "  ./cpu_bench mode=batch8 logN=25 batch=64 reps=10\n"
         << "  ./cpu_bench mode=batch logN=25 batch=64 reps=10\n";
    return 1;
  }

  string mode = args["mode"];
  size_t N = stoul(args["logN"]);
  size_t reps = args.count("reps") ? stoul(args["reps"]) : 10;
  size_t num_elements = 1ULL << N;

  double DB_size =
      static_cast<double>(num_elements * sizeof(datastore::db_record)) /
      SIZE_GB;
  cout << "Database Size: " << DB_size << " GB" << endl;

  datastore store;
  store.reserve(num_elements);
  setup_database(store, num_elements);

  if (mode == "single8") {
    run_single_query_vectorized(store, N, reps);
  } else if (mode == "single") {
    run_single_query_scalar(store, N, reps);
  } else if (mode == "batch") {
    if (!args.count("batch")) {
      cerr << "Missing 'batch' parameter for batch mode.\n";
      return 1;
    }
    size_t batch_size = stoul(args["batch"]);
    run_batch_query(store, N, batch_size, reps);
  } else if (mode == "batch8") {
    if (!args.count("batch")) {
      cerr << "Missing 'batch' parameter for batch mode.\n";
      return 1;
    }
    size_t batch_size = stoul(args["batch"]);
    run_batch_query8(store, N, batch_size, reps);
  } else {
    cerr << "Unknown mode: " << mode << endl;
    return 1;
  }

  return 0;
}
