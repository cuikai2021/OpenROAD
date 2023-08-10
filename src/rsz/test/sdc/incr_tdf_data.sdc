set_case_analysis 0 [get_ports {mode}]

create_clock -name clk -period 5.5 [get_ports {clk}]

set_input_delay -clock clk 0.1 in1
set_output_delay -clock clk 0.3 out
# set_propagated_clock clk