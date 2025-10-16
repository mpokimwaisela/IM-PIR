# About
- This artifact contains the code and instructions required to reproduce the key results in our Middleware 2025 paper — *IM-PIR: In-Memory Private Information Retrieval*.
- The artifact demonstrates how UPMEM Processing-In-Memory (PIM) accelerates the XOR-scanning operation in multi-server PIR.
- The repository provides the following folders:
   - **`src/`** — source code for building and running the IM-PIR system.
   - **`plot/`** — scripts for executing benchmarks and generating result plots.

## For Middleware’25 Artifact Reviewers
- Because setting up a UPMEM-based environment to run the benchmarks can be complex, we have a pre-configured server that reviewers can access remotely to run the benchmarks. To use the pre-configured server, reviewers should: Send their SSH public key to the email: **`mpoki.mwaisela@unine.ch`** with the subject line: **`Middleware’25 artifact reviewer`** public key
- Once your key is received, we will configure the server to allow remote SSH access for running the benchmarks. After configuration, you will receive a confirmation email with detailed connection instructions on how to access the server remotely.
- To ensure a smooth evaluation process, we encourage reviewers to coordinate time slots among themselves to avoid running experiments concurrently on the same server.
- For reviewers using our pre-configured server via SSH, you may skip directly to the section “Running the Benchmarks” to begin the evaluation. Otherwise, please follow the setup instructions in the next section to configure your own system.
- Estimated total run time: approximately 4–6 hours for all benchmarks (excluding initial setup time).


## Setup
- The following steps describe how to set up the environment required to build and run the IM-PIR benchmarks.
- The setup assumes a Linux-based system (tested on Ubuntu 20.04 and 22.04) with UPMEM Processing-In-Memory (PIM) hardware or access to our pre-configured UPMEM server (see For Middleware’25 Artifact Reviewers above).

1. Install dependencies:
Install the required software packages for building and running IM-PIR:

```bash
   sudo apt-get install cmake make gnuplot python3 python3-pip
   pip3 install pandas
```

2. Install the UPMEM Toolchain
The IM-PIR benchmarks require the UPMEM SDK and toolchain. If you are using our pre-configured server, this step has already been completed.
Otherwise, follow [the official installation guide provided by UPMEM](https://sdk.upmem.com/stable/index.html):

3. Build IM-PIR   
Navigate to the **`src`** folder and build the project:

```bash
   cd src
   mkdir build && cd build
   cmake ..
   make
```

4. Verify Installation
Run the basic functionality test to ensure the setup works correctly:

```bash
   ./test
```

## Running the benchmarks
This section describes how to execute the IM-PIR benchmarks and reproduce the key results presented in our Middleware 2025 paper.

Benchmarks are run from the plots/ directory and automatically generate both raw data and plots.

1. Navigate to the Benchmark Directory

```bash
cd plots
```

2. Run the benchmark script
Executte the main benchmark script

```bash 
bash run.sh
```
This script compiles and executes the full benchmark suite and automatically generates the plots corresponding to the main results presented in the paper.

3. Interpreting the results
After the executiong, the results will be generated in the following locations:
- Raw data: `plots/data/`
- Generated plots: `plots/`

Each plot corresponds to the figure or experiment discussed in the paper: 
- `figure3.pdf` corresponds to **Figure 3** in the paper. It illustrates the execution times of the individual algorithms that make up the PIR construction based on DPFs. The key observation is that the **dpXOR** operation dominates the total execution time, highlighting it as the primary target for acceleration. 

- `figure9.pdf` corresponds to figure 9 in the paper. It compares query throughput and latency between IM-PIR and a CPU-based PIR design across different database and batch sizes. Each record is 32 bytes, and the CPU baseline uses AES-NI and AVX optimizations with one thread per query.

- `figure10.pdf` corresponds to figure 10 in the paper. It shows the latency breakdown of server-side phases in IM-PIR and CPU-PIR. While CPU-PIR is bottlenecked by memory-bound dpXOR operations, IM-PIR’s in-memory processing shifts the bottleneck to CPU-side DPF evaluation. This figure also provides the data used to compute the percentages in Table 1 in the paper.

Note: Our server is equipped with 512 DPUs, of which 509 are functional. For convenience and consistency with power-of-two database sizes, we configured the benchmarks to run on 256 DPUs. The main results presented in the paper were obtained on a larger system with 2,546 DPUs, utilizing 2,048 DPUs for the same power-of-two convenience. Consequently, the benchmark results in this artifact differ from those in the paper due to the smaller number of DPUs (256 vs. 2,048). Using a non–power-of-two configuration would require adding padding or bogus data during database and hot vector partitioning, leading to unnecessary overhead and data imbalance.