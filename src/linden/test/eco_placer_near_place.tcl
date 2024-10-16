source "helpers.tcl"
read_lef Nangate45.lef
read_def gcd_replace.def

set eps [linden::make_eco_placer]
puts "ECO Placer initialized."

linden::eco_placer_init_from_odb $eps
puts "Initialization from odb complete."

set buf_x2 "BUF_X2"
set point "135900 142955"

# find 1 
set pi [linden::eco_placer_find_nearest_place $eps $buf_x2 $point --xmin 135900 --ymin 142955]
for {set i 0} {$i < 4} {incr i} {
    set place [linden::eco_placer_next_place $pi]
    puts "Place $i: $place"
}
puts ""

# find 2
set pi [linden::eco_placer_find_nearest_place $eps $buf_x2 $point --xmin 130000 --ymin 130000 --xmax 133000 --ymax 135000]
for {set i 0} {$i < 5} {incr i} {
    set place [linden::eco_placer_next_place $pi]
    puts "Place $i: $place"
}
