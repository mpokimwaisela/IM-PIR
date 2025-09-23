import subprocess
import csv
import re
import os
import pandas as pd

BUILD_DIR = "../src/build"
PIM_BINARY_NAME = "pim_bench"
CPU_BINARY_NAME = "cpu_bench"
PIM_BINARY_PATH = os.path.join(BUILD_DIR, PIM_BINARY_NAME)
CPU_BINARY_PATH = os.path.join(BUILD_DIR, CPU_BINARY_NAME)
OUTPUT_DIR = "./data"

# Create the output directory if it doesn't exist
os.makedirs(OUTPUT_DIR, exist_ok=True)

logNs = [24, 25, 26, 27, 28]
batch_sizes = [4, 8,  32]
reps = 10
dpu_counts = [64, 128, 256]
max_dpus = dpu_counts[-1]
clusters = [1, 2, 4, 8]

csv_files = ["pim_single.csv", "pim_batch.csv", "pim_cluster.csv","cpu_single.csv", "cpu_batch.csv"]

for filename in csv_files:
    filepath = os.path.join(OUTPUT_DIR, filename)
    if os.path.exists(filepath):
        os.remove(filepath)

pim_single_fields = [
    "logN", "size_GB", "pir_total", "pim2cpu", "pir_exec", "cpu2pim", "dpf_eval", "pir_agg", "dpf_keygen"
]

cpu_single_fields = [
   "logN","size_GB","pir_total","dpf_eval","dpf_keygen"
]

batch_fields = [
    "logN", "size_GB", "BatchSize", "Latency_ms", "Throughput_qps"
]

cluster_fields = [
    "logN", "size_GB", "BatchSize", "Clusters", "Latency_ms", "Throughput_qps"
]

dpu_scaling_fields = [
    "num_dpus", "logN", "size_GB", "cpu2pim","pir_exec", "pim2cpu"
]

def run_command(command):
    """Run a shell command inside the build directory."""
    result = subprocess.run(
        command,
        cwd=BUILD_DIR,
        shell=True,
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"[ERROR OUTPUT]\n{result.stderr}")
        raise Exception(f"Command failed: {command}")
    return result.stdout

def parse_size(output):
    match = re.search(r"Database Size:\s*([\d.]+)\s*GB", output)
    if not match:
        return None
    value = float(match.group(1))
    return int(value) if value.is_integer() else value

def parse_pim_single_output(output):
    return {
        "pir_total": float(re.search(r"PIR\.PIM_Total\s*:\s*([\d.]+)", output).group(1)),
        "pim2cpu": float(re.search(r"COPY\.PIM->CPU\s*:\s*([\d.]+)", output).group(1)),
        "pir_exec": float(re.search(r"PIR\.PIMexec\s*:\s*([\d.]+)", output).group(1)),
        "cpu2pim": float(re.search(r"COPY\.CPU->PIM\s*:\s*([\d.]+)", output).group(1)),
        "dpf_eval": float(re.search(r"DPF\.Eval\s*:\s*([\d.]+)", output).group(1)),
        "pir_agg": float(re.search(r"PIR\.Aggregate\s*:\s*([\d.eE-]+)", output).group(1)),
        "dpf_keygen": float(re.search(r"DPF\.KeyGen\s*:\s*([\d.]+)", output).group(1)),
    }

def parse_pim_batch_output(output):
    return {
        "Latency_ms": float(re.search(r"Batch\s*=\s*\d+\s*:\s*([\d.]+)\s*ms", output).group(1)),
        "Throughput_qps": float(re.search(r"Throughput\s*:\s*([\d.]+)", output).group(1))
    }

def parse_cpu_single_output(output):
    return {
        "pir_total": float(re.search(r"PIR\.CPU\s*:\s*([\d.]+)", output).group(1)),
        "dpf_eval": float(re.search(r"DPF\.Eval\s*:\s*([\d.]+)", output).group(1)),
        "dpf_keygen": float(re.search(r"DPF\.KeyGen\s*:\s*([\d.]+)", output).group(1)),
    }

def parse_cpu_batch_output(output):
    return {
        "Latency_ms": float(re.search(r"Batch\s*=\s*\d+\s*:\s*([\d.]+)\s*ms", output).group(1)),
        "Throughput_qps": float(re.search(r"Throughput\s*:\s*([\d.]+)", output).group(1))
    }

def write_csv_row(filename, fieldnames, row):
    path = os.path.join(OUTPUT_DIR, filename)
    file_exists = os.path.isfile(path)
    with open(path, "a", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        if not file_exists:
            writer.writeheader()
        writer.writerow(row)

def write_dat(filename, header, rows):
    with open(os.path.join(OUTPUT_DIR, filename), "w") as f:
        f.write(header + "\n")
        for row in rows:
            f.write("\t".join(str(x) for x in row) + "\n")

### CPU BENCHMARKS

### SINGLE MODE
for logN in logNs:
    cmd = f"./{CPU_BINARY_NAME} logN={logN} mode=single8 reps={reps}"
    output = run_command(cmd)
    size_gb = parse_size(output)
    metrics = parse_cpu_single_output(output)
    row = {"logN": logN, "size_GB": size_gb, **metrics}
    write_csv_row("cpu_single.csv", cpu_single_fields, row)

### BATCH MODE
for logN in logNs:
    for batch in batch_sizes:
        if logN > 28 :
            continue
        cmd = f"./{CPU_BINARY_NAME} logN={logN} batch={batch} mode=batch8 reps={reps}"
        output = run_command(cmd)
        size_gb = parse_size(output)
        metrics = parse_cpu_batch_output(output)
        row = {"logN": logN, "size_GB": size_gb, "BatchSize": batch, **metrics}
        write_csv_row("cpu_batch.csv", batch_fields, row)

### PIM BENCHMARKS

# ## DPU SCALING MODE 
# logN_scaling = 25
# for dpus in dpu_counts:
#     cmd = f"./{PIM_BINARY_NAME} num_dpus={dpus} mode=single logN={logN_scaling} reps={reps}"
#     output = run_command(cmd)
#     size_gb = parse_size(output)
#     metrics = parse_pim_single_output(output)
#     row = {
#         "num_dpus": dpus,
#         "logN": logN_scaling,
#         "size_GB": size_gb,
#         "cpu2pim": metrics["cpu2pim"],
#         "pir_exec": metrics["pir_exec"],
#         "pim2cpu": metrics["pim2cpu"]
#     }
#     write_csv_row("dpu_scaling.csv", dpu_scaling_fields, row)

### SINGLE MODE
for logN in logNs:
    cmd = f"./{PIM_BINARY_NAME} num_dpus={max_dpus} mode=single logN={logN} reps={reps}"
    output = run_command(cmd)
    size_gb = parse_size(output)
    metrics = parse_pim_single_output(output)
    row = {"logN": logN, "size_GB": size_gb, **metrics}
    write_csv_row("pim_single.csv", pim_single_fields, row)

### BATCH MODE
for logN in logNs:
    for batch in batch_sizes:
        if logN > 28 :
            continue
        cmd = f"./{PIM_BINARY_NAME} num_dpus={max_dpus} mode=batch logN={logN} batch={batch} reps={reps}"
        output = run_command(cmd)
        size_gb = parse_size(output)
        metrics = parse_pim_batch_output(output)
        row = {"logN": logN, "size_GB": size_gb, "BatchSize": batch, **metrics}
        write_csv_row("pim_batch.csv", batch_fields, row)


# ### CLUSTER MODE 
# logN_cluster = 25
# for batch_cluster in batch_sizes:
#     for cluster in clusters:
#         cmd = f"./{PIM_BINARY_NAME} num_dpus={max_dpus} mode=batch logN={logN_cluster} batch={batch_cluster} cluster={cluster} reps={reps}"
#         output = run_command(cmd)
#         size_gb = parse_size(output)
#         metrics = parse_pim_batch_output(output)
#         row = {
#             "logN": logN_cluster,
#             "size_GB": size_gb,
#             "BatchSize": batch_cluster,
#             "Clusters": cluster,
#             **metrics
#         }
#         write_csv_row("pim_cluster.csv", cluster_fields, row)

# 1. batch_plot1_latency.dat: DB size, cpu latency, pim latency (fixed batch size)
cpu_batch = pd.read_csv(os.path.join(OUTPUT_DIR, "cpu_batch.csv"))
pim_batch = pd.read_csv(os.path.join(OUTPUT_DIR, "pim_batch.csv"))

# Fixed batch size: smallest
fixed_batch = min(cpu_batch["BatchSize"].unique())

# Fixed DB size: 1GB
fixed_db_size = 1.0

rows = []
for size in sorted(cpu_batch["size_GB"].unique()):
    cpu_row = cpu_batch[(cpu_batch["size_GB"] == size) & (cpu_batch["BatchSize"] == fixed_batch)]
    pim_row = pim_batch[(pim_batch["size_GB"] == size) & (pim_batch["BatchSize"] == fixed_batch)]
    if not cpu_row.empty and not pim_row.empty:
        rows.append([
            size,
            cpu_row.iloc[0]["Latency_ms"],
            pim_row.iloc[0]["Latency_ms"]
        ])
write_dat("batch_plot1_latency.dat", "DB_Size\tCPU_Latency\tPIM_Latency", rows)

# 2. batch_plot1_thoughput.dat: DB size, cpu throughput, pim throughput (fixed batch size)
rows = []
for size in sorted(cpu_batch["size_GB"].unique()):
    cpu_row = cpu_batch[(cpu_batch["size_GB"] == size) & (cpu_batch["BatchSize"] == fixed_batch)]
    pim_row = pim_batch[(pim_batch["size_GB"] == size) & (pim_batch["BatchSize"] == fixed_batch)]
    if not cpu_row.empty and not pim_row.empty:
        rows.append([
            size,
            cpu_row.iloc[0]["Throughput_qps"],
            pim_row.iloc[0]["Throughput_qps"]
        ])
write_dat("batch_plot1_thoughput.dat", "DB_Size\tCPU_Throughput\tPIM_Throughput", rows)

# 3. batch_plot2_latency.dat: batch size, cpu latency, pim latency (fixed DB size = 1GB)
rows = []
for batch in sorted(cpu_batch["BatchSize"].unique()):
    cpu_row = cpu_batch[(cpu_batch["size_GB"] == fixed_db_size) & (cpu_batch["BatchSize"] == batch)]
    pim_row = pim_batch[(pim_batch["size_GB"] == fixed_db_size) & (pim_batch["BatchSize"] == batch)]
    if not cpu_row.empty and not pim_row.empty:
        rows.append([
            batch,
            cpu_row.iloc[0]["Latency_ms"],
            pim_row.iloc[0]["Latency_ms"]
        ])
write_dat("batch_plot2_latency.dat", "BatchSize\tCPU_Latency\tPIM_Latency", rows)

# 4. batch_plot2_throughput.dat: batch size, cpu throughput, pim throughput (fixed DB size = 1GB)
rows = []
for batch in sorted(cpu_batch["BatchSize"].unique()):
    cpu_row = cpu_batch[(cpu_batch["size_GB"] == fixed_db_size) & (cpu_batch["BatchSize"] == batch)]
    pim_row = pim_batch[(pim_batch["size_GB"] == fixed_db_size) & (pim_batch["BatchSize"] == batch)]
    if not cpu_row.empty and not pim_row.empty:
        rows.append([
            batch,
            cpu_row.iloc[0]["Throughput_qps"],
            pim_row.iloc[0]["Throughput_qps"]
        ])
write_dat("batch_plot2_throughput.dat", "BatchSize\tCPU_Throughput\tPIM_Throughput", rows)

# # 5. pim_cluster_latency.dat: batchsize and latencies for cluster1 cluster2 cluster4 cluster8
# pim_cluster = pd.read_csv(os.path.join(OUTPUT_DIR, "pim_cluster.csv"))
# batch_sizes = sorted(pim_cluster["BatchSize"].unique())
# header = "BatchSize\tC1\tC2\tC4\tC8"
# rows = []
# for batch in batch_sizes:
#     latencies = []
#     for c in [1,2,4,8]:
#         match = pim_cluster[(pim_cluster["BatchSize"] == batch) & (pim_cluster["Clusters"] == c)]
#         latencies.append(match["Latency_ms"].iloc[0] if not match.empty else "NA")
#     rows.append([batch] + latencies)
# write_dat("pim_cluster_latency.dat", header, rows)

# # 6. pim_cluster.dat: batchsize and throughput for cluster1 cluster2 cluster4 cluster8
# header = "BatchSize\tC1\tC2\tC4\tC8"
# rows = []
# for batch in batch_sizes:
#     throughputs = []
#     for c in [1,2,4,8]:
#         match = pim_cluster[(pim_cluster["BatchSize"] == batch) & (pim_cluster["Clusters"] == c)]
#         throughputs.append(match["Throughput_qps"].iloc[0] if not match.empty else "NA")
#     rows.append([batch] + throughputs)
# write_dat("pim_cluster.dat", header, rows)

