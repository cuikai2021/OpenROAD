set eps [linden::make_eco_placer]
puts "ECO Placer initialized."

linden::eco_placer_add_lef $eps "Nangate45.lef"
puts "LEF file added."

linden::eco_placer_add_def $eps "gcd_replace.def"
puts "DEF file added."

linden::eco_placer_init $eps
puts "Initialization complete."

set buf_x2 "BUF_X2"
set p0 "135900 142955"
set p1 "135900 142955"
set x_delta 2000
set y_delta 2000

# find
puts "Starting to find vacant places..."
set places [linden::eco_placer_find_vacant_places $eps $buf_x2 $p0 $p1 $x_delta $y_delta]
puts "Number of vacant places found: [llength $places]"
set idx 0
foreach place $places {
    puts "Place $idx: $place"
    incr idx
}
puts " "

# insert + find
set cell_name "cell_0"
set cell_place [lindex $places 0]
puts "Inserting cell '$cell_name' at place: $cell_place"
linden::eco_placer_insert_cell $eps $cell_name $buf_x2 $cell_place
puts "Cell inserted."
puts "Finding vacant places after cell insertion..."
set places [linden::eco_placer_find_vacant_places $eps $buf_x2 $p0 $p1 $x_delta $y_delta]
puts "Number of vacant places found after insertion: [llength $places]"
set idx 0
foreach place $places {
    puts "Place $idx: $place"
    incr idx
}
puts " "

# remove + find
puts "Removing cell '$cell_name'."
linden::eco_placer_remove_cell $eps $cell_name
puts "Cell removed."
puts "Finding vacant places after cell removal..."
set places [linden::eco_placer_find_vacant_places $eps $buf_x2 $p0 $p1 $x_delta $y_delta]
puts "Number of vacant places found after removal: [llength $places]"
set idx 0
foreach place $places {
    puts "Place $idx: $place"
    incr idx
}
puts " "

# Finish ECO Placer
linden::eco_placer_finish $eps
puts "ECO Placer operations completed."