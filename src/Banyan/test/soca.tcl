set tapout_flow_dir $env(TAPOUT_FLOW_DIR)

read_lef $tapout_flow_dir/foundries/smic/40/lef/scc40nll_10lm_2tm.SPACING.lef
read_lef $tapout_flow_dir/foundries/smic/40/lef/scc40nll_vhsc40_rvt_ant.lef
read_lef $tapout_flow_dir/foundries/smic/40/lef/sram_sp_hde_x512y8d32_bw.lef
read_def $tapout_flow_dir/results/smic40vhs_RVT/pulpino_top_wrapper/4_cts.def
banyan
