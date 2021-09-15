  read_lef icb_mode/NangateOpenCellLibrary.tech.lef
  read_lef icb_mode/NangateOpenCellLibrary.macro.mod.lef
  read_lef icb_mode/pad.lef

  read_liberty icb_mode/NangateOpenCellLibrary_typical.lib

  read_def icb_mode/floorplan.def
  read_sdc icb_mode/constraints.sdc

  pdngen icb_mode/pdn.cfg -verbose

  write_def icb_mode/pdn.def
  write_verilog icb_mode/pdn.v


  
