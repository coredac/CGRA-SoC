proc synthesize {} {
  source job.tcl
  file mkdir out
  define_design_lib WORK -path work
  set logic /opt2/synopsys/14mm/stdcell_rvt/db_nldm/saed14rvt_tt0p8v25c.db
  set_app_var target_library [list $logic]
  set_app_var link_library [list * $logic]
  set_app_var sh_continue_on_error false
  set_host_options -max_cores 2
  if {![analyze -format sverilog -define SYNTHESIS $source_list]} { error "RTL analysis failed" }
  elaborate $top
  current_design $top
  if {![link]} { error "Design link failed" }
  create_clock -name core -period 10 [get_ports clk]
  set_input_delay 0.1 -clock core [remove_from_collection [all_inputs] [get_ports clk]]
  set_output_delay 0.1 -clock core [all_outputs]
  set_fix_multiple_port_nets -all -buffer_constants [current_design]
  redirect -file out/check_before.rpt { check_design }
  compile -map_effort medium -area_effort none
  set leaves [get_cells -hierarchical -filter {is_hierarchical == false}]
  foreach ref [lsort -unique [get_attribute $leaves ref_name]] {
    if {[string match GTECH* $ref] || [sizeof_collection [get_lib_cells -quiet */$ref]] == 0} {
      error "Unmapped or black-box leaf: $ref"
    }
  }
  redirect -file out/check_after.rpt { check_design }
  redirect -file out/area.rpt { report_area -hierarchy }
  redirect -file out/timing.rpt { report_timing -max_paths 10 }
  redirect -file out/qor.rpt { report_qor }
  redirect -file out/units.rpt { report_units }
  redirect -file out/references.rpt { report_reference -hierarchy }
  write -format ddc -hierarchy -output out/mapped.ddc
  write -format verilog -hierarchy -output out/mapped.v
  write_sdc out/mapped.sdc
  redirect -variable area_text { report_area }
  if {![regexp {Total cell area:\s+([0-9.eE+-]+)} $area_text match area]} { error "Missing mapped area" }
  set handle [open out/result.tsv w]
  puts $handle "top\tcell_area_library_units\tleaf_cells\tclock_period_ns"
  puts $handle "$top\t$area\t[sizeof_collection $leaves]\t10"
  close $handle
  set handle [open out/SUCCESS w]
  puts $handle "Mapped $top with no unmapped leaves. Timing closure must be read separately."
  close $handle
}
if {[catch {synthesize} message]} {
  puts stderr "ASIC_SYNTH_FAILED: $message"
  puts stderr $::errorInfo
  exit 1
}
exit 0
