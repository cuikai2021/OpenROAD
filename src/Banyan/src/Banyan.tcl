proc banyan { args } {
  sta::check_argc_eq0 "banyan" $args
  drt::banyan_cmd
}
