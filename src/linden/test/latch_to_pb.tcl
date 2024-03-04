read_liberty Nangate45/Nangate45_typ.lib
read_lef Nangate45/Nangate45.lef

read_verilog latch_to_pb.v
link_design top

read_sdc sdc/incr_tdf_data.sdc

set pb_file "results/latch_to_pb.pb"

rsz::write_tdp_data $pb_file

puts "-------test the dumped result-------\n"
rsz::read_tdp_data $pb_file
