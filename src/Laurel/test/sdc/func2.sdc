set_case_analysis 0 [get_ports {mode0}]
set_case_analysis 1 [get_ports {mode1}]

create_clock -name clk -period 2 [get_ports {clk}]

# set_input_delay -clock clk 0.1 in2
# set_output_delay -clock clk -0.3 out
set_propagated_clock clk
