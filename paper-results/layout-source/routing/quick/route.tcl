set_voltage 0.8 -corners tt
set_ignored_layers -min_routing_layer M2 -max_routing_layer M8
foreach opt {route.global.timing_driven route.global.crosstalk_driven route.track.timing_driven route.track.crosstalk_driven route.detail.timing_driven} {
  set_app_options -name $opt -value false
}
set b [open before.tsv w]
foreach_in_collection n $selected {
  puts $b [join [list [get_object_name $n] [sizeof_collection [get_shapes -quiet -of_objects $n]] [sizeof_collection [get_vias -quiet -of_objects $n]]] "\t"]
}
close $b
puts "QUICK_ROUTE_BEGIN [clock format [clock seconds]] [sizeof_collection $selected]"
set routed [route_group -nets $selected -max_detail_route_iterations 1]
puts "QUICK_ROUTE_RETURN $routed [clock format [clock seconds]]"
source collect.tcl
save_block -compress
save_lib
close [open ROUTE_SAVED w]
