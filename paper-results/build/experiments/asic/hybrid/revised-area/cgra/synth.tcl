proc run {} {
  define_design_lib WORK -path work
  set logic /opt2/synopsys/14mm/stdcell_rvt/db_nldm/saed14rvt_tt0p8v25c.db
  set_app_var target_library [list $logic]
  set_app_var link_library [list * $logic]
  set_app_var sh_continue_on_error false
  set_host_options -max_cores 2
  if {![analyze -format verilog CgraCompute.v]} {error "analyze failed"}
  elaborate CgraCompute
  current_design CgraCompute
  if {![link]} {error "link failed"}
  create_clock -name core -period 2 [get_ports clk]
  set_input_delay 0.1 -clock core [remove_from_collection [all_inputs] [get_ports clk]]
  set_output_delay 0.1 -clock core [all_outputs]
  set_input_transition 0.1 [remove_from_collection [all_inputs] [get_ports clk]]
  set_load 1.0 [all_outputs]
  set_max_transition 0.2 [current_design]
  set_max_fanout 16 [current_design]
  set_wire_load_mode enclosed
  set_fix_multiple_port_nets -all -buffer_constants [get_designs *]
  if {![compile -map_effort medium -area_effort high]} {error "compile failed"}
  redirect -file area.rpt {report_area -hierarchy}
  redirect -file units.rpt {report_units}
  redirect -file setup.rpt {report_timing -delay_type max -max_paths 10 -nets -input_pins -significant_digits 6}
  redirect -file constraints.rpt {report_constraint -all_violators -significant_digits 6}
  redirect -file qor.rpt {report_qor}
  write -format ddc -hierarchy -output mapped.ddc
  redirect -file check.rpt {check_design}
  redirect -file timing_check.rpt {check_timing -include {unconstrained_endpoints no_input_delay partial_input_delay}}
  redirect -file design.rpt {report_design}
  set leaves [get_cells -hierarchical -filter {is_hierarchical == false}]
  foreach ref [lsort -unique [get_attribute $leaves ref_name]] {
    if {[string match GTECH* $ref] || [sizeof_collection [get_lib_cells -quiet */$ref]] == 0} {error "unmapped $ref"}
  }
  set names [get_attribute $leaves full_name]
  set refs [get_attribute $leaves ref_name]
  set areas [get_attribute $leaves area]
  set seqs [get_attribute $leaves is_sequential]
  set f [open leaves.tsv w]
  puts $f "path\tref\tarea\tsequential"
  foreach name $names ref $refs area $areas seq $seqs {puts $f "$name\t$ref\t$area\t$seq"}
  close $f
  set f [open SUCCESS w]
  puts $f complete
  close $f
}
if {[catch run message]} {puts stderr $message; puts stderr $::errorInfo; exit 1}
exit 0
