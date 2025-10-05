#include "./dpf/dpf.h"
#include "dpu/common.h"
#include "datastore.h"
#include "util/concurentqueue.h"
#include "util/profiler.h"
#include "util/queue.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dpu>
#include <iostream>
#include <map>
#include <omp.h>
#include <random>
#include <vector>

using namespace std;
using namespace dpu;

#define SIZE_GB 1024 * 1024 * 1024

#define BINARY_NAME "dpu_task"

static std::vector<dpu::DpuSet *> dpu_clusters;

static size_t NUM_DPUS = 128;

Profiler profiler;
std::vector<dpu_args_t> args(1);

void setup_database(datastore &store, size_t num_elements, size_t cluster);
void execution_pim(size_t N, std::vector<uint8_t> aaaa);
void pim_batch_execution(size_t N, datastore &store, size_t batch_size,
                         size_t reps);

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

void run_single_query_pim(datastore &store, size_t N, size_t reps) {

  // 1. setup the database
  // 2. generate keys
  // 3. evaluate keys
  // 4. perform the dot product or xor operation for the CPU variant.
  //
  for (size_t r = 0; r < reps; r++) {

    profiler.start("DPF.KeyGen");
    auto keys = DPF::Gen(5, N);
    profiler.accumulate("DPF.KeyGen");

    auto a = keys.first;
    // auto b = keys.second; // Not used in this example only for one server
    std::vector<uint8_t> aaaa;

    profiler.start("DPF.Eval");
    aaaa = DPF::EvalFull8(a, N);
    profiler.accumulate("DPF.Eval");

    execution_pim(N, aaaa);
  }

  profiler.printAllTimes(true); // Print the median time
  profiler.reset();
  printf("\n");
}

void run_batch_query_pim(datastore &store, size_t N, size_t batch_size,
                         size_t cluster, size_t reps) {
  pim_batch_execution(N, store, batch_size, reps);
}

int main(int argc, char **argv) {

  auto args = parse_args(argc, argv);

  if (!args.count("mode") || !args.count("logN")) {
    cerr << "Usage:\n"
         << "  ./pim_bench num_dpus=256 mode=single logN=20 reps=10\n"
         << "  ./pim_bench num_dpus=256 mode=batch logN=20 batch=64 cluster=1 "
            "reps=10\n";
    return 1;
  }

  string mode = args["mode"];
  size_t N = stoul(args["logN"]);
  size_t reps = args.count("reps") ? stoul(args["reps"]) : 10;
  size_t batch_size = args.count("batch") ? stoul(args["batch"]) : 1;
  size_t cluster = args.count("cluster") ? stoul(args["cluster"]) : 1;
  size_t num_dpus = args.count("num_dpus") ? stoul(args["num_dpus"]) : 128;
  NUM_DPUS = num_dpus;

  size_t num_elements = 1ULL << N;
  double DB_size =
      static_cast<double>(num_elements * sizeof(datastore::db_record)) /
      static_cast<double>(SIZE_GB);
  datastore store;
  store.reserve(num_elements);
  cout << "Database Size: " << DB_size << " GB" << endl;
  setup_database(store, num_elements, cluster);

  if (mode == "single") {
    run_single_query_pim(store, N, reps);
  } else if (mode == "batch") {
    run_batch_query_pim(store, N, batch_size, cluster, reps);
  } else {
    cerr << "Unknown mode: " << mode << endl;
    return 1;
  }
  return 0;
}

void setup_database(datastore &store, size_t num_elements,
                    size_t cluster = 1) {
  try {
    assert(cluster > 0);
    assert(cluster <= NUM_DPUS);
    assert(NUM_DPUS % cluster == 0);

    size_t DPUS_PER_CLUSTER = NUM_DPUS / cluster;

    if (dpu_clusters.size() <= 0) {
      for (size_t i = 0; i < cluster; i++) {
        dpu_clusters.push_back(
            new dpu::DpuSet(DpuSet::allocate(DPUS_PER_CLUSTER)));
        dpu_clusters[i]->load(BINARY_NAME);
      }
      DPUS_PER_CLUSTER = dpu_clusters[0]->dpus().size();
      printf("Clusters: %zu\n", cluster);
      printf("DPUs per cluster: %zu\n", DPUS_PER_CLUSTER);
    }

    size_t data_per_dpu =
        (num_elements + DPUS_PER_CLUSTER - 1) / DPUS_PER_CLUSTER;
    size_t database_size_per_dpu_bytes =
        data_per_dpu * sizeof(datastore::db_record);
    args[0].database_size_bytes = database_size_per_dpu_bytes;
    args[0].num_batches = 1;

    std::vector<std::vector<uint64_t>> dpu_store(DPUS_PER_CLUSTER);

#pragma omp parallel for
    for (size_t i = 0; i < DPUS_PER_CLUSTER; i++) {
      size_t start = i * data_per_dpu;
      size_t end = std::min(start + data_per_dpu, num_elements);
      for (size_t j = start; j < end; j++) {
        dpu_store[i].push_back(j);
        dpu_store[i].push_back(j);
        dpu_store[i].push_back(j);
        dpu_store[i].push_back(j);
      }
    }

    for (size_t i = 0; i < cluster; i++) {
      dpu_clusters[i]->copy(DPU_MRAM_HEAP_POINTER_NAME, 0, dpu_store);
    }

  } catch (const DpuError &e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }
}

// This execution is mainly for single query execution
void execution_pim(size_t N, std::vector<uint8_t> aaaa) {
  std::vector<std::vector<uint8_t>> output_vectors(NUM_DPUS,
                                                   std::vector<uint8_t>(32, 0));

  size_t elements_per_dpu = (aaaa.size() + NUM_DPUS - 1) / NUM_DPUS;
  std::vector<std::vector<uint8_t>> dpu_input_vectors(NUM_DPUS);

#pragma omp parallel for
  for (size_t i = 0; i < NUM_DPUS; i++) {
    auto start = aaaa.begin() + std::min(i * elements_per_dpu, aaaa.size());
    auto end =
        (i == NUM_DPUS - 1)
            ? aaaa.end()
            : aaaa.begin() + std::min((i + 1) * elements_per_dpu, aaaa.size());
    dpu_input_vectors[i] = std::vector<uint8_t>(start, end);
  }
  args[0].input_indexing_size_bytes = elements_per_dpu;

  profiler.start("PIR.PIM_Total");
  profiler.start("COPY.CPU->PIM");
  dpu_clusters[0]->copy("args", args);
  dpu_clusters[0]->copy(DPU_MRAM_HEAP_POINTER_NAME, args[0].database_size_bytes,
                        dpu_input_vectors);

  profiler.accumulate("COPY.CPU->PIM");

  profiler.start("PIR.PIMexec");
  dpu_clusters[0]->exec();
  profiler.accumulate("PIR.PIMexec");

  profiler.start("COPY.PIM->CPU");
  // dpu_set->copy(output_vectors, "out");
  dpu_clusters[0]->copy(output_vectors, "out");
  profiler.accumulate("COPY.PIM->CPU");

  profiler.start("PIR.Aggregate");
  datastore::db_record dpu_result = _mm256_setzero_si256();

  for (const auto &dpu_output : output_vectors) {
    datastore::db_record partial;
    std::memcpy(&partial, dpu_output.data(), sizeof(datastore::db_record));
    dpu_result = _mm256_xor_si256(dpu_result, partial);
  }

  profiler.accumulate("PIR.Aggregate");
  profiler.accumulate("PIR.PIM_Total");
}


void pim_batch_execution(size_t N, datastore &store, size_t batch_size,
                         size_t reps) {
  // ---------------------------------------------
  // 1. Key generation 
  // ---------------------------------------------
  std::vector<std::vector<uint8_t>> keys(batch_size);
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<size_t> dist(0, N - 1);
  for (size_t i = 0; i < batch_size; ++i) {
    auto kp = DPF::Gen(dist(rng), N);
    keys[i] = kp.first;
  }

  const size_t num_dpus = NUM_DPUS / dpu_clusters.size();
  const std::string event_name = "Batch = " + std::to_string(batch_size);
  // using db_record = datastore::db_record;

  moodycamel::ConcurrentQueue<BatchData> queue;
  std::atomic<bool> producers_done{false};

  // ---------------------------------------------
  // 3. CPU producer threads (enqueue)
  // ---------------------------------------------
  auto cpu_worker = [&](size_t start, size_t end) {
    moodycamel::ProducerToken ptoken(queue);
    for (size_t b = start; b < end; ++b) {
      BatchData data;

      // Perform DPF evaluation
      data.query = DPF::EvalFull8(keys[b], N); 

      // Split the input query into num_dpus parts
      size_t elems = (data.query.size() + num_dpus - 1) / num_dpus;
      data.dpu_input_vectors.resize(num_dpus);
      for (size_t j = 0; j < num_dpus; ++j) {
        auto s = data.query.begin() + std::min(j * elems, data.query.size());
        auto e = (j + 1 == num_dpus)
                     ? data.query.end()
                     : data.query.begin() +
                           std::min((j + 1) * elems, data.query.size());
        data.dpu_input_vectors[j] = std::vector<uint8_t>(s, e);
      }

      // Enqueue the batch data
      queue.enqueue(ptoken, std::move(data));
    }
  };

  // ---------------------------------------------
  // 4. DPU submitter threads (bulk‑dequeue)
  // ---------------------------------------------
  // 
      auto dpu_submitter = [&](dpu::DpuSet *set) {
        moodycamel::ConsumerToken ctoken(queue);
        // reserve a small buffer for bulk pop
        std::vector<BatchData> buf(32);
        std::vector<dpu_args_t> arguments(1);

        while (true) {
            size_t got = queue.try_dequeue_bulk(ctoken, buf.data(), buf.size());
            if (got == 0) {
                if (producers_done.load(std::memory_order_acquire) && queue.size_approx() == 0) break;  // all work finished
                std::this_thread::yield();
                continue;
            }

            // This sends one by one batch to the DPU need to be optimized to batch
            // multiple batches together
            // for (size_t idx = 0; idx < got; ++idx) {
            //     BatchData &data = buf[idx];
            //     std::vector<std::vector<uint8_t>> dpu_out(num_dpus, std::vector<uint8_t>(32));
            //     if (!data.dpu_input_vectors.empty()){
            //         arguments[0].input_indexing_size_bytes = data.dpu_input_vectors[0].size();
            //         arguments[0].num_batches = got;
            //         arguments[0].database_size_bytes = args[0].database_size_bytes;
            //       } 
            //     auto h = set->async();
            //     h.copy("args", arguments);
            //     h.copy(DPU_MRAM_HEAP_POINTER_NAME, args[0].database_size_bytes, data.dpu_input_vectors);
            //     h.exec();
            //     h.copy(dpu_out, "out");
            //     h.sync();

            //     datastore::db_record agg = _mm256_setzero_si256();
            //     for (auto &vec : dpu_out) {
            //         datastore::db_record part;
            //         memcpy(&part, vec.data(), sizeof(part));
            //         agg = _mm256_xor_si256(agg, part);
            //     }
            // }
            
            size_t output_size_per_dpu = sizeof(datastore::db_record)*got;
            std::vector<std::vector<uint8_t>> batched_inputs(num_dpus);
            std::vector<std::vector<uint8_t>> dpu_out(num_dpus, std::vector<uint8_t>(output_size_per_dpu)); 

            for (size_t idx = 0; idx < got; ++idx) {
                BatchData &data = buf[idx];
                for(size_t dpu_id = 0; dpu_id < num_dpus; ++dpu_id){
                    if (!data.dpu_input_vectors[dpu_id].empty()){
                        batched_inputs[dpu_id].insert(batched_inputs[dpu_id].end(), data.dpu_input_vectors[dpu_id].begin(), data.dpu_input_vectors[dpu_id].end());
                    }
                }
            }
            arguments[0].input_indexing_size_bytes = batched_inputs[0].size()/got;
            arguments[0].num_batches = got;
            arguments[0].database_size_bytes = args[0].database_size_bytes;

            set->copy("args", arguments);
            set->copy(DPU_MRAM_HEAP_POINTER_NAME, args[0].database_size_bytes, batched_inputs);
            set->exec();
            // set->copy(dpu_out, output_size_per_dpu, DPU_MRAM_HEAP_POINTER_NAME, args[0].database_size_bytes);
            set->copy(dpu_out, "out");

            datastore::aligned_vector agg(got, _mm256_setzero_si256());

            for (size_t dpu_id = 0; dpu_id < num_dpus; ++dpu_id) {
                for (size_t idx = 0; idx < got; ++idx) {
                        datastore::db_record part;
                        memcpy(&part, dpu_out[dpu_id].data() + (idx * sizeof(datastore::db_record)), sizeof(part));
                        agg[idx] = _mm256_xor_si256(agg[idx], part);
                }
            }
          }
    };

  profiler.start(event_name);
  for (size_t r = 0; r < reps; ++r) {
    // ---- launch producers ----
    const size_t num_workers =
        std::min<size_t>(16, std::max<size_t>(1, batch_size / 4));
    std::vector<std::thread> producers;
    size_t per = batch_size / num_workers;
    size_t rem = batch_size % num_workers;
    for (size_t w = 0, off = 0; w < num_workers; ++w) {
      size_t cnt = per + (w < rem);
      producers.emplace_back(cpu_worker, off, off + cnt);
      off += cnt;
    }

    // ---- launch DPU submitters ----
    std::vector<std::thread> submitters;
    for (auto *set : dpu_clusters)
      submitters.emplace_back(dpu_submitter, set);

    // wait producers
    for (auto &t : producers)
      t.join();
    producers_done.store(true, std::memory_order_release);

    // wait consumers
    for (auto &t : submitters)
      t.join();

    profiler.accumulate(event_name);
    producers_done.store(false, std::memory_order_relaxed);
  }

  profiler.printAllTimes(true);
  double tp = (batch_size * 1000.0) / profiler.getAverageTime(event_name);
  std::cout << "Throughput: " << tp << " q/s" << std::endl;
}