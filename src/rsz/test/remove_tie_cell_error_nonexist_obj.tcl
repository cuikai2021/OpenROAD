read_liberty Nangate45/Nangate45_typ.lib
read_lef Nangate45/Nangate45.lef

catch {remove_tie_cell -tie_low_cell  LOGIC0_X1           \
                       -tie_low_pin   Z                   \
                       -tie_high_cell NOT_EXIST_CELL_TIE1 \
                       -tie_high_pin  Z                   } error
puts $error

catch {remove_tie_cell -tie_low_cell  NOT_EXIST_CELL_TIE0 \
                       -tie_low_pin   Z                   \
                       -tie_high_cell LOGIC1_X1           \
                       -tie_high_pin  Z                   } error
puts $error

catch {remove_tie_cell -tie_low_cell  LOGIC0_X1          \
                       -tie_low_pin   NOT_EXIST_TIE0_PIN \
                       -tie_high_cell LOGIC1_X1          \
                       -tie_high_pin  Z                  } error
puts $error

catch {remove_tie_cell -tie_low_cell  LOGIC0_X1          \
                       -tie_low_pin   Z                  \
                       -tie_high_cell LOGIC1_X1          \
                       -tie_high_pin  NOT_EXIST_TIE1_PIN } error
puts $error
