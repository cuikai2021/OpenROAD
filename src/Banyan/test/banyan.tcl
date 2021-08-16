set tapout_flow_dir $env(TAPOUT_FLOW_DIR)

read_lef ~/newtestcase2/scc40nll_10lm_2tm.SPACING.lef
read_lef ~/newtestcase2/scc40nll_vhsc40_rvt_ant.lef

read_def ~/newtestcase2/test_1.pl.def
banyan_route
# banyan_save_txt
write_def test_1_rt.def



