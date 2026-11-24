proc run {} {
  set_host_options -max_cores 16
  open_lib stage.ndm
  open_block ChipTop
  set_voltage 0.8 -corners tt
  set_ignored_layers -min_routing_layer M2 -max_routing_layer M8
  route_global
  save_block -compress
  save_lib
  close_lib
}
if {[catch run message]} {puts stderr $message; puts stderr $::errorInfo; exit 1}
exit 0
