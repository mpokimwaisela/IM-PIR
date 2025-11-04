#!/bin/bash

# Build benchmark binaries, generate performance data CSVs, and create plots
# 1. Compiles the C++ PIR implementation 
# 2. Runs plot.py to execute benchmarks and generate CSV data
# 3. Generates plots from data using gnuplot scripts

echo "Compiling code in ../src/build..."
mkdir -p ../src/build
cd ../src/build || exit 1
cmake .. > /dev/null 2>&1 && make > /dev/null 2>&1
compile_status=$?
cd ../../plots || exit 1

if [ $compile_status -ne 0 ]; then
	echo "Compilation failed. Running with verbose output to show errors..."
	cd ../src/build || exit 1
	cmake .. && make
	cd ../../plots || exit 1
	exit 1
fi

echo "Running plot.py..."
python3 plot.py
plot_status=$?
if [ $plot_status -ne 0 ]; then
	echo "plot.py failed. Exiting."
	exit 1
fi

echo "Running gnuplot scripts..."
for script in scripts/*.plot; do
	echo "Running gnuplot $script"
	gnuplot "$script"
	if [ $? -ne 0 ]; then
		echo "gnuplot $script failed. Exiting."
		exit 1
	fi
done

echo "All steps completed successfully."
