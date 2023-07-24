###############################################################################
##
## BSD 3-Clause License
##
## Copyright (c) 2019, The Regents of the University of California
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##
## * Redistributions of source code must retain the above copyright notice, this
##   list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright notice,
##   this list of conditions and the following disclaimer in the documentation
##   and#or other materials provided with the distribution.
##
## * Neither the name of the copyright holder nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.
##
###############################################################################

sta::define_cmd_args "configure_cts_characterization" {[-max_cap cap] \
                                                       [-max_slew slew] \
                                                       [-slew_inter slewvalue] \
                                                       [-cap_inter capvalue] \
                                                      }

proc configure_cts_characterization { args } {
  sta::parse_key_args "configure_cts_characterization" args \
    keys {-max_cap -max_slew -slew_inter -cap_inter} flags {}

  sta::check_argc_eq0 "configure_cts_characterization" $args

  if { [info exists keys(-max_cap)] } {
    set max_cap_value $keys(-max_cap)
    cts::set_max_char_cap $max_cap_value
  }

  if { [info exists keys(-max_slew)] } {
    set max_slew_value $keys(-max_slew)
    cts::set_max_char_slew $max_slew_value
  }

  if { [info exists keys(-slew_inter)] } {
    set slew $keys(-slew_inter)
    cts::set_slew_inter $slew
  }

  if { [info exists keys(-cap_inter)] } {
    set cap $keys(-cap_inter)
    cts::set_cap_inter $cap
  }
}

sta::define_cmd_args "clock_tree_synthesis" {[-wire_unit unit]
                                             [-buf_list buflist] \
                                             [-root_buf buf] \
                                             [-clk_nets nets] \
                                             [-out_path path] \
                                             [-tree_buf buf] \
                                             [-post_cts_disable] \
                                             [-distance_between_buffers] \
                                             [-branching_point_buffers_distance] \
                                             [-clustering_exponent] \
                                             [-clustering_unbalance_ratio] \
                                             [-sink_clustering_size] \
                                             [-sink_clustering_max_diameter] \
                                             [-sink_clustering_enable] \
                                             [-balance_levels] \
                                             [-sink_clustering_levels levels] \
                                             [-num_static_layers] \
                                             [-sink_clustering_buffer] \
                                             [-nnc_builder [-nnc_topo_pb pb_file] [-nnc_banyan]]
                                            }

proc clock_tree_synthesis { args } {
  sta::parse_key_args "clock_tree_synthesis" args \
    keys {-root_buf -buf_list -wire_unit -clk_nets -out_path -sink_clustering_size -num_static_layers\
          -sink_clustering_buffer -distance_between_buffers -branching_point_buffers_distance -clustering_exponent\
          -clustering_unbalance_ratio -sink_clustering_max_diameter -sink_clustering_levels -tree_buf\
          -slew_degradation_bound -merge_delay_threshold -buffer_test_in_slew -max_fanout -balance_delay_threshold\
          -nnc_topo_pb}\
    flags {-post_cts_disable -sink_clustering_enable -balance_levels -nnc_builder -nnc_banyan}

  sta::check_argc_eq0 "clock_tree_synthesis" $args

  cts::set_disable_post_cts [info exists flags(-post_cts_disable)]

  cts::set_sink_clustering [info exists flags(-sink_clustering_enable)]

  cts::set_enable_nnc [info exists flags(-nnc_builder)]

  if { [info exists keys(-nnc_topo_pb)] } {
    if { [info exists flags(-nnc_builder)] } {
      cts::set_ncc_topo_pbfile $keys(-nnc_topo_pb)
    } else {
      utl::warn CTS 53 "Given -nnc_topo_pb flag in the absence of -nnc_builder flag, ignore it."
    }
  }

  if { [info exists keys(-nnc_banyan)] } {
    if { [info exists flags(-nnc_builder)] } {
      cts::set_nnc_banyan true
    } else {
      utl::warn CTS 54 "Given -nnc_banyan flag in the absence of -nnc_builder flag, ignore it."
    }
  }

  if { [info exists keys(-sink_clustering_size)] } {
    set size $keys(-sink_clustering_size)
    cts::set_sink_clustering_size $size
  }

  if { [info exists keys(-sink_clustering_max_diameter)] } {
    set distance $keys(-sink_clustering_max_diameter)
    cts::set_clustering_diameter $distance
  }

  cts::set_balance_levels [info exists flags(-balance_levels)]

  if { [info exists keys(-sink_clustering_levels)] } {
    set levels $keys(-sink_clustering_levels)
    cts::set_sink_clustering_levels $levels
  }

  if { [info exists keys(-num_static_layers)] } {
    set num $keys(-num_static_layers)
    cts::set_num_static_layers $num
  }

  if { [info exists keys(-distance_between_buffers)] } {
    set distance $keys(-distance_between_buffers)
    cts::set_distance_between_buffers $distance
  }

  if { [info exists keys(-branching_point_buffers_distance)] } {
    set distance $keys(-branching_point_buffers_distance)
    cts::set_branching_point_buffers_distance $distance
  }

  if { [info exists keys(-clustering_exponent)] } {
    set exponent $keys(-clustering_exponent)
    cts::set_clustering_exponent $exponent
  }

  if { [info exists keys(-clustering_unbalance_ratio)] } {
    set unbalance $keys(-clustering_unbalance_ratio)
    cts::set_clustering_unbalance_ratio $unbalance
  }

  if { [info exists keys(-buf_list)] } {
    set buf_list $keys(-buf_list)
    cts::set_buffer_list $buf_list
  } else {
    #User must input the buffer list.
    utl::error CTS 55 "Missing argument -buf_list"
  }

  if { [info exists keys(-wire_unit)] } {
    set wire_unit $keys(-wire_unit)
    cts::set_wire_segment_distance_unit $wire_unit
  }

  if { [info exists keys(-clk_nets)] } {
    set clk_nets $keys(-clk_nets)
    set fail [cts::set_clock_nets $clk_nets]
    if {$fail} {
      utl::error CTS 56 "Error when finding -clk_nets in DB."
    }
  }

  if { [info exists keys(-tree_buf)] } {
    set buf $keys(-tree_buf)
    cts::set_tree_buf $buf
  }

  if { [info exists keys(-root_buf)] } {
    set root_buf $keys(-root_buf)
    if { [llength $root_buf] > 1} {
      set root_buf [lindex $root_buf 0]
    }
    cts::set_root_buffer $root_buf
  } else {
    if { [info exists keys(-buf_list)] } {
      #If using -buf_list, the first buffer can become the root buffer.
      set root_buf [lindex $buf_list 0]
      cts::set_root_buffer $root_buf
    } else {
      utl::error CTS 57 "Missing argument, user must enter at least one of -root_buf or -buf_list."
    }
  }

  if { [info exists keys(-sink_clustering_buffer)] } {
    set sink_buf $keys(-sink_clustering_buffer)
    if { [llength $sink_buf] > 1} {
      set sink_buf [lindex $sink_buf 0]
    }
    cts::set_sink_buffer $sink_buf
  } else {
    cts::set_sink_buffer $root_buf
  }

  if { [info exists keys(-out_path)] && (![info exists keys(-lut_file)] || ![info exists keys(-sol_list)]) } {
    set out_path $keys(-out_path)
    cts::set_out_path $out_path
  }

  if { [info exists keys(-slew_degradation_bound)] } {
    set slew_bound $keys(-slew_degradation_bound)
    cts::set_slew_degradation_bound $slew_bound
  }

  if { [info exists keys(-merge_delay_threshold)] } {
    set delay_threshold $keys(-merge_delay_threshold)
    cts::set_merge_delay_threshold $delay_threshold
  }

  if { [info exists keys(-buffer_test_in_slew)] } {
    set test_in_slew $keys(-buffer_test_in_slew)
    cts::set_buffer_test_in_slew $test_in_slew
  }

  if { [info exists keys(-balance_delay_threshold)] } {
    set balance_delay_threshold $keys(-balance_delay_threshold)
    cts::set_balance_delay_threshold $balance_delay_threshold
  }

  if { [info exists keys(-max_fanout)] } {
    set fanout $keys(-max_fanout)
    cts::set_max_fanout $fanout
  }

  if { [ord::get_db_block] == "NULL" } {
    utl::error CTS 103 "No design block found."
  }
  cts::run_triton_cts
}

sta::define_cmd_args "report_cts" {[-out_file file] \
                                  }
proc report_cts { args } {
  sta::parse_key_args "report_cts" args \
    keys {-out_file} flags {-json_format}

  sta::check_argc_eq0 "report_cts" $args

  if { [info exists keys(-out_file)] } {
    set outFile $keys(-out_file)
    cts::set_metric_output $outFile
  }

  cts::set_json_metric_format [info exists flags(-json_format)]

  cts::report_cts_metrics
}

sta::define_cmd_args "set_clock_net_ndr" { {-all ndr} | {-non_leaf ndr} | {[-top ndr] [-trunk ndr] [-trunk_leaf ndr] [-leaf ndr]} }

proc set_clock_net_ndr { args } {
  sta::parse_key_args "set_clock_net_ndr" args \
    keys  {-all -non_leaf -top -trunk -trunk_leaf -leaf} \
    flags {}

  if { [info exists keys(-all)] } {
    set_clock_net_ndr   -non_leaf $keys(-all)
    set_clock_net_ndr       -leaf $keys(-all)
  }

  if { [info exists keys(-non_leaf)] } {
    set_clock_net_ndr -top        $keys(-non_leaf)
    set_clock_net_ndr -trunk      $keys(-non_leaf)
    set_clock_net_ndr -trunk_leaf $keys(-non_leaf)
  }

  if { [info exists keys(-top)] } {
    cts::set_clock_net_ndr_cmd "top"        $keys(-top)
  }
  if { [info exists keys(-trunk)] } {
    cts::set_clock_net_ndr_cmd "trunk"      $keys(-trunk)
  }
  if { [info exists keys(-trunk_leaf)] } {
    cts::set_clock_net_ndr_cmd "trunk_leaf" $keys(-trunk_leaf)
  }
  if { [info exists keys(-leaf)] } {
    cts::set_clock_net_ndr_cmd "leaf"       $keys(-leaf)
  }
}
