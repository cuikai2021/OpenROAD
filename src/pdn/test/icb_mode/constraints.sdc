
create_clock -period 20  -waveform {0 10} 	-name clk_10m    [get_ports {clk_pad}]


set_max_transition 0.5 [all_clocks] -data_path
set_max_transition 0.3 [all_clocks] -clock_path


set_max_fanout 24 [current_design]

set_clock_uncertainty 0.500 [all_clocks]



set_input_delay 3 pad_pad_1 		-clock [get_clocks clk_10m]

set_output_delay 3 pad_pad_2		-clock [get_clocks clk_10m]
set_output_delay 3 pad_pad_3		-clock [get_clocks clk_10m]
