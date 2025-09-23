set terminal pdfcairo enhanced font 'Helvetica,16' size 7.5in,6.5in
set output 'figure9.pdf'

COLOR_1  = "#287D8E"   # Deep Teal
COLOR_2  = "#4C5B61"   # Slate Gray
COLOR_3  = "#A52F2F"   # Bordeaux Red
COLOR_4  = "#556B2F"   # Olive Green
COLOR_5  = "#4B0082"   # Indigo
COLOR_6  = "#8B5A2B"   # Chestnut Brown
COLOR_7  = "#D55E00"   # Burnt Orange
COLOR_8  = "#002147"   # Oxford Blue
COLOR_9  = "#36454F"   # Charcoal Gray
COLOR_10 = "#3C6E71"   # Muted Navy
COLOR_11 = "#C27BA0"   # Dusty Rose
COLOR_12 = "#8DA7BE"   # Pewter Blue
COLOR_13 = "#D2B48C"   # Warm Taupe
COLOR_14 = "#228B22"   # Forest Green
COLOR_15 = "#990000"   # Crimson
COLOR_16 = "#301934"   # Midnight Purple
COLOR_17 = "#CD7F32"   # Amber

SCALE = 1000

set multiplot layout 2,2 rowsfirst
set border 3 lw 2
set ytics nomirror
set xtics nomirror
set grid

set format y "10^{%L}"
# Style lines
set style line 2 lc rgb COLOR_1    pt 3 ps 0.6 lw 3 dt 4  
set style line 3 lc rgb COLOR_17  pt 2 ps 0.6 lw 3 dt 5

set key top center vertical samplen 4 spacing 1.5 font ",16"

set logscale y 10
#set ytics ("1" 0, "10" 10, "100" 100, "1000" 1000)

# ---------- Plot 1 ----------
set title "{/:Bold (a) Throughput vs DB Size (Batch = 32)}"
set xlabel "DB Size (GB)"
set ylabel "Throughput (QPS)"

plot \
    './data/batch_plot1_throughput.dat' using 1:2 with linespoints ls 2 title 'CPU-PIR', \
    './data/batch_plot1_throughput.dat' using 1:3 with linespoints ls 3 title 'IM-PIR'

# ---------- Plot 2 ----------
set title "{/:Bold (b) Throughput vs Batch Size (DB = 1 GiB)}"
set xlabel "Batch Size"
set ylabel "Throughput (QPS)"
set logscale x 2
set xrange [2.5:512]
set xtics ("4" 4, "8" 8, "16" 16, "32" 32, "64" 64, "128" 128, "256" 256, "512" 512)


plot \
    './data/batch_plot2_throughput.dat' using 1:2 with linespoints ls 2 title 'CPU-PIR', \
    './data/batch_plot2_throughput.dat' using 1:3 with linespoints ls 3 title 'IM-PIR'

unset xrange
unset yrange
unset logscale x
# ---------- Plot 3 ----------
set title "{/:Bold (c) Latency vs DB Size (Batch = 32)}"
set xlabel "DB Size (GB)"
set ylabel "Latency (s)"
set xtics auto
set key top center vertical


plot \
    './data/batch_plot1_latency.dat' using 1:($2/SCALE) with linespoints ls 2 title 'CPU-PIR', \
    './data/batch_plot1_latency.dat' using 1:($3/SCALE) with linespoints ls 3 title 'IM-PIR'

# ---------- Plot 4 ----------
set title "{/:Bold (d) Latency vs Batch Size (DB = 1 GiB)}"
set xlabel "Batch Size"
set ylabel "Latency (s)"
set logscale x 2
set xrange [2.5:512]
set xtics ("4" 4, "8" 8, "16" 16, "32" 32, "64" 64, "128" 128, "256" 256, "512" 512)


plot \
    './data/batch_plot2_latency.dat' using 1:($2/SCALE) with linespoints ls 2 title 'CPU-PIR', \
    './data/batch_plot2_latency.dat' using 1:($3/SCALE) with linespoints ls 3 title 'IM-PIR'

unset multiplot
