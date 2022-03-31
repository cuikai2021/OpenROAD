set_case_analysis 1 [get_ports {mode0}]

create_clock -name clk -period 2 [get_ports {clk}]

# set_output_delay -clock clk -0.3 out
set_propagated_clock clk
