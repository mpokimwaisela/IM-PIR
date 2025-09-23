

# IM-PIR
Benchmarking In-memory processing for Private Information Retrieval (PIR).

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
   cd ../../plots
   ```
3. Benchmark & plot:
   ```bash
   bash run.sh
   ```
   Results: see `plots/data/` (CSV) and `plots/` (PDF).
