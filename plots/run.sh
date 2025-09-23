#!/bin/bash

echo "Compiling code in ../src/build..."
cd ../src/build || exit 1
cmake .. && make
compile_status=$?
cd ../../plots || exit 1

if [ $compile_status -ne 0 ]; then
	echo "Compilation failed. Exiting."
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
