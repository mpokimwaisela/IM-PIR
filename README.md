# IM-PIR: In-Memory Private Information Retrieval
This repository provides the benchmarks for our IM-PIR paper, leveraging UPMEM PIM to accelerate the XOR-scanning operation in multi-server PIR.

The repository includes:
- **`src/`** — source code
- **`plot/`** — scripts for running benchmarks and generating plots

## Quick Start

1. Install dependencies:
   ```bash
   sudo apt-get install cmake make gnuplot python3 python3-pip
   pip3 install pandas
   ```
2. Build:
   Requires [UPMEM PIM toolchain](https://sdk.upmem.com/stable/index.html) installed and configured.
   
   ```bash
   cd src/build && cmake .. && make
   ```

3. Simple functionality test
   ```bash
   ./test
   ```

3. Benchmark & plot:
   ```bash
   cd ../../plots
   bash run.sh
   ```
   Results: see `plots/data/` (CSV) and `plots/` (PDF).

Note: Paper benchmarks were executed on a 2048-DPU UPMEM cloud server. 
Performance may differ with varying DPU counts. 
The benchmarks in this repository run on 256 DPUs from a 512-DPU system (509 operational) for easier data management.