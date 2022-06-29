# set_case_analysis 0 [get_ports {mode}]

create_clock -name clk -period 2.0 [get_ports {clk}]

# set_input_delay -clock clk 0.1 in2
# set_output_delay -clock clk -0.3 out
set_propagated_clock clk
