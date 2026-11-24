proc synthesize {} {
  source job.tcl
  file mkdir out
  define_design_lib WORK -path work
  set logic /opt2/synopsys/14mm/stdcell_rvt/db_nldm/saed14rvt_tt0p8v25c.db
  set_app_var target_library [list $logic]
  set sram /opt2/synopsys/14mm/SAED14nm_EDK_SRAM_v_05072020/lib/sram/logic_synth/dual/saed14sram_tt0p8v25c.db
  set_app_var link_library [list * $logic $sram]
  set_app_var sh_continue_on_error false
  set_host_options -max_cores 4
  if {![analyze -format sverilog -define SYNTHESIS $source_list]} { error "RTL analysis failed" }
  elaborate $top
  if {[sizeof_collection [get_designs -quiet $top]] != 1} { error "Top elaboration failed: $top" }
  foreach child $dependencies { read_ddc ../$child/out/mapped.ddc }
  current_design $top
  if {[get_object_name [current_design]] != $top} { error "Wrong current design" }
  if {![link]} { error "Design link failed" }
  set_dont_touch [get_cells -hierarchical -filter {ref_name == SRAM2RW128x32}] true
  foreach child $dependencies { set_dont_touch [get_designs $child] true }
  create_clock -name core -period 10 [get_ports $clock_port]
  set_input_delay 0.1 -clock core [remove_from_collection [all_inputs] [get_ports $clock_port]]
  set_output_delay 0.1 -clock core [all_outputs]
  set_fix_multiple_port_nets -all -buffer_constants [current_design]
  redirect -file out/check_before.rpt { check_design }
  write -format ddc -hierarchy -output out/precompile.ddc
  write_sdc out/precompile.sdc
  if {![compile -map_effort low -area_effort none]} { error "Compile failed" }
  if {[get_object_name [current_design]] != $top} { error "Wrong compiled design" }
  write -format ddc -hierarchy -output out/mapped.ddc
  write -format verilog -hierarchy -output out/mapped.v
  write_sdc out/mapped.sdc
  redirect -file out/area.rpt { report_area -hierarchy }
  redirect -file out/timing.rpt { report_timing -delay_type max -max_paths 10 -significant_digits 4 }
  redirect -file out/hold.rpt { report_timing -delay_type min -max_paths 10 -significant_digits 4 }
  redirect -file out/constraints.rpt { report_constraint -all_violators -significant_digits 4 }
  redirect -file out/qor.rpt { report_qor }
  redirect -file out/units.rpt { report_units }
  redirect -file out/references.rpt { report_reference -hierarchy }
  redirect -file out/check_after.rpt { check_design }
  set leaves [get_cells -hierarchical -filter {is_hierarchical == false}]
  foreach ref [lsort -unique [get_attribute $leaves ref_name]] {
    if {[string match GTECH* $ref] || [sizeof_collection [get_lib_cells -quiet */$ref]] == 0} {
      error "Unmapped or black-box leaf: $ref"
    }
  }
  set macros [sizeof_collection [get_cells -hierarchical -filter {ref_name == SRAM2RW128x32}]]
  set expected_macros [expr {$top == "Sync128" ? 1 : 256}]
  if {$macros != $expected_macros} { error "Wrong SRAM macro count: $macros" }
  if {$top == "AsicCore" || $top == "AsicAccelerator"} {
    set bank_file [open out/banks.tsv w]
    foreach {bank expected} {Async128 24 Sync128 256} {
      set count [sizeof_collection [get_cells -hierarchical -filter "ref_name == $bank"]]
      puts $bank_file "$bank\t$count"
      if {$count != $expected} { error "Unexpected bank count for $bank: $count" }
    }
    close $bank_file
  }
  redirect -variable area_text { report_area }
  if {![regexp {Total cell area:\s+([0-9.eE+-]+)} $area_text match area]} { error "Missing mapped area" }
  set handle [open out/result.tsv w]
  puts $handle "top\tcell_area_library_units\tleaf_cells\tclock_period_ns"
  puts $handle "$top\t$area\t[sizeof_collection $leaves]\t10"
  close $handle
  set handle [open out/SUCCESS w]
  puts $handle "Mapped $top; no unmapped leaves. Timing closure must be read separately."
  close $handle
}
if {[catch {synthesize} message]} {
  puts stderr "ASIC_BANK_SYNTH_FAILED: $message"
  puts stderr $::errorInfo
  exit 1
}
exit 0
