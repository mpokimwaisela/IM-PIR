set datafile separator ","

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


set terminal pdfcairo enhanced font "Helvetica,16" size 7.5in,3.5in
set output 'figure10.pdf'

set multiplot layout 1,2 rowsfirst

set border 3 lw 2
set ytics nomirror
set xtics nomirror
set grid
set xrange [-0.5:3.2]

set style data histogram
set style histogram rowstacked gap 1
set style fill solid 1 border rgb "black"
set boxwidth 0.25

set style line 1 lt 1 lw 2.5 lc rgb COLOR_1
set style line 2 lt 1 lw 2.5 lc rgb COLOR_17
set style line 3 lt 1 lw 2.5 lc rgb COLOR_2
set style line 4 lt 1 lw 2.5 lc rgb COLOR_4
set style line 5 lt 1 lw 2.5 lc rgb COLOR_3


###############################################################################
# (a) System Overheads
###############################################################################
set title "{/:Bold (a) Latency breakdown for IM-PIR phases}"
set xlabel "Size (GB)" font ",18"
set ylabel "Latency (ms)" font ",18"
set key top center vertical samplen 4 spacing 1.2 font ",16"

plot '< awk -F, ''$2 >= 1 && $2 <= 32'' ./data/pim_single.csv' \
        using 7:xtic(2) title "Eval"  ls 1 fill pattern 1, \
     '' using 6 title "copy(cpu→pim)"      ls 5 fill pattern 14, \
     '' using 5 title "dpXOR"              ls 2 fill pattern 1, \
     '' using 4 title "copy(pim→cpu)"      ls 3 fill pattern 15, \
     '' using 8 title "aggregation"        ls 4 fill pattern 2

###############################################################################
# (b) Algorithmic Bottlenecks
###############################################################################
set title "{/:Bold (b) Latency breakdown for CPU-PIR phases}"
plot '< awk -F, ''$2 >= 1 && $2 <= 32'' ./data/cpu_single.csv' using 4:xtic(2) title "Eval" ls 1 fill pattern 1, \
     '' using 3 title "dpXOR" ls 2 fill pattern 1

unset multiplot
set output
