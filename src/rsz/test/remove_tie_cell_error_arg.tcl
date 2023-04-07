catch {remove_tie_cell} error
puts $error

catch {remove_tie_cell -tie_low_cell  tie_low_cell_name            } error
puts $error

catch {remove_tie_cell -tie_low_cell  tie_low_cell_name            \
                       -tie_low_pin   tie_low_cell_output_pin_name } error
puts $error

catch {remove_tie_cell -tie_low_cell  tie_low_cell_name            \
                       -tie_low_pin   tie_low_cell_output_pin_name \
                       -tie_high_cell tie_high_cell_name           } error
puts $error


catch {remove_tie_cell -tie_low_cell  tie_low_cell_name            \
                       -tie_low_pin   tie_low_cell_output_pin_name \
                       -tie_high_cell tie_high_cell_name           \
                       -tie_high_pim  tie_high_cell_output_pin_name} error
puts $error
