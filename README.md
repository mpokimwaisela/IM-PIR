# IM-PIR: In-Memory Private Information Retrieval
This repository contains the benchmarks for our IM-PIR paper.
We use UPMEM PIM to demonstrate PIM, focusing on the XOR-scanning operation central to multi-server PIR.
The repo includes the PIR source code (src/) and scripts (plot/) for running benchmarks and generating the paper’s plots.

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

Note: Paper benchmarks were run on a 2048-DPU UPMEM cloud server. 
Development/testing used our 512-DPU server (509 functional). 
For consistency and simpler data handling, experiments were limited to 256 DPUs with evenly sized databases.