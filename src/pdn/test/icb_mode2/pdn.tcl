read_lef icb_mode2/PRTF_EDI_N65_9M_6X1Z1U_RDL_M9.15_.24a.tlef
read_lef icb_mode2/tcbn65lp_std.lef
read_lef icb_mode2/ts1n65lpa4096x32m8_140a_5m.lef
read_lef icb_mode2/tphn65lpnv2od3_sl_9lm.lef
read_lef icb_mode2/tpbn65v_9lm.lef
read_def icb_mode2/fp.def

pdngen icb_mode2/pdn.cfg -verbose

write_def icb_mode2/pdn.def


  
