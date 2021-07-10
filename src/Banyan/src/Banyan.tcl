proc banyan_route { args } {
  sta::check_argc_eq0 "banyan_route" $args
  drt::banyan_route 
}

proc banyan_save_txt { args } {
  sta::check_argc_eq0 "banyan_save_txt" $args
  drt::banyan_save_txt 
}
