  read_lef NangateOpenCellLibrary.tech.lef
  read_lef NangateOpenCellLibrary.macro.mod.lef
  read_lef pad.lef

  read_liberty NangateOpenCellLibrary_typical.lib

  read_def floorplan.def
  read_sdc constraints.sdc

  pdngen pdn.cfg -verbose

  write_def pdn.def
  write_verilog pdn.v


  
