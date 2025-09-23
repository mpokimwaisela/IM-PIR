### Define colors
RED    = "#EB5353"
BLUE   = "#187498"
GREEN  = "#36AE7C"
YELLOW = "#F9D923"
BLACK  = "#000000"

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

set terminal pdfcairo enhanced font 'Helvetica,16' size 7in,3.5in
set output 'figure3.pdf'

set multiplot layout 1,2 rowsfirst

####################
# (a) Execution Time
####################

set border 3
set ytics nomirror
set xtics nomirror
set grid ytics
set border 3 lw 2

set title "{/:Bold (a) Execution Time}"

set style data histogram
set style histogram rowstacked gap 0.5
set style fill solid 1 border rgb "black"
set boxwidth 0.2

set style line 1 lt 1 lw 3 lc rgb COLOR_1
set style line 2 lt 1 lw 3 lc rgb COLOR_2
set style line 3 lt 1 lw 3 lc rgb COLOR_17


set xlabel "Size (GB)"
set ylabel "Time (ms)"
set xrange [-0.35:2.15]
set key top center

plot '< awk -F, ''($2==1||$2==2||$2==4){printf "%d %s %s %s\n",$2,$5,$4,$3}'' ./data/cpu_single.csv' \
     using 2:xtic(1) title 'Gen' ls 2 fill pattern 5, \
     ''         using 3        title 'Eval'   ls 1 fill pattern 1,  \
     ''         using 4        title 'dpXOR'    ls 3 fill pattern 1


# Clean up for next plot
unset object 1
unset arrow
unset label
unset logscale
unset format
unset border
unset xtics
unset ytics
unset grid
unset xrange
unset yrange
unset xlabel
unset ylabel
unset title


####################
# (b) Roofline Model
####################

# Axes, scaling
set logscale x
set logscale y
set xrange [0.01:50]
set yrange [0.5:17.5]
set border 3 lw 2.5
set xtics ("0.01" 0.01, "0.1" 0.1, "1" 1, "10" 10, "50" 50)
set ytics ("0.5" 0.5, "1" 1, "2" 2, "4" 4, "8" 8, "16" 16)
unset mxtics
unset mytics
set format x "%.2f"
set format y "%.1f"

# Labels and grid
set title "{/:Bold (b) Roofline Model}"
set xlabel "Operational Intensity (OP/B)"
set ylabel "Performance (GFLOPS)"
set xtics nomirror
set ytics nomirror
set grid xtics
set grid ytics

# Constants
peak_bw = 48.89
peak_flops = 16.8
knee = peak_flops / peak_bw

# Transparent memory-bound area
set object 1 polygon from \
    0.01,0.5 to \
    knee,peak_bw*knee to \
    knee,0.5 to \
    0.01,0.5 \
    fc rgb "skyblue" fs transparent solid 0.3 noborder

# Style lines
set style line 1 lc rgb BLACK lt 1 lw 3.5
set style line 2 lc rgb COLOR_17 pt 7 ps 1.5
set style line 3 lc rgb COLOR_1 pt 7 ps 1.5

# Knee marker
set arrow from knee,0.5 to knee,peak_flops nohead dt 2 lw 2.5 lc rgb BLACK
set label "Peak compute" at 1,peak_flops*0.85 front

# Point labels
set label "dpXOR"  at 0.05,0.95 front
set label "Eval" at 20,6.5 front

set arrow 11 from  0.5,2.5 to 0.1,2.5
#set arrow 22 from 250,2.36 to 74,2.36
set label 11 "memory bound region" font ",15.5" at 0.55,2.55 left 
#set label 22 "training resumed" font ",15" at 260,2.36 left

# Plot roofline and points (no titles)
plot \
    "< awk 'BEGIN{for(x=0.01;x<=0.3435;x+=0.01) printf \"%.5f %.5f\\n\", x, 48.89*x}'" \
        using 1:2 with lines ls 1 notitle, \
    "< awk 'BEGIN{for(x=0.3435;x<=50;x+=0.01) print x, 16.8}'" \
        using 1:2 with lines ls 1 notitle, \
    "-" using 1:2 with points ls 2 notitle, \
    "-" using 1:2 with points ls 3 notitle
# PIR point
0.0625 0.725
e
# Eval point
30 4.85
e

unset multiplot
set output