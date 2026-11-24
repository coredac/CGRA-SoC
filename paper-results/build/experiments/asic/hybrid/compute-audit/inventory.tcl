proc inventory {name checkpoint top} {
  remove_design -all
  read_ddc $checkpoint
  current_design $top
  if {![link]} { error "Cannot link $name" }
  set leaves [get_cells -hierarchical -filter {is_hierarchical == false}]
  set names [get_attribute $leaves full_name]
  set refs [get_attribute $leaves ref_name]
  set areas [get_attribute $leaves area]
  set sequential [get_attribute $leaves is_sequential]
  set count [sizeof_collection $leaves]
  foreach values [list $names $refs $areas $sequential] {
    if {[llength $values] != $count} { error "Missing leaf attributes in $name" }
  }
  set file [open "$name.tsv" w]
  puts $file "path\tref\tarea\tsequential"
  foreach path $names ref $refs area $areas seq $sequential {
    puts $file "$path\t$ref\t$area\t$seq"
  }
  close $file
  redirect -file "$name-area.rpt" { report_area }
  puts "INVENTORY $name $count"
}
set logic /opt2/synopsys/14mm/stdcell_rvt/db_nldm/saed14rvt_tt0p8v25c.db
set sram /opt2/synopsys/14mm/SAED14nm_EDK_SRAM_v_05072020/lib/sram/logic_synth/dual/saed14sram_tt0p8v25c.db
set_app_var target_library [list $logic]
set_app_var link_library [list * $logic $sram]
set_app_var sh_continue_on_error false
set_host_options -max_cores 2
if {[catch {
  inventory asic /tmp/asic-sram-20261113-v1/AsicAccelerator/out/mapped.ddc AsicAccelerator
  inventory cgra /tmp/cgra-cost-current-20261113-v002/jobs/275cf1ae994205fb1200126176af79a73c6b1d95924a8ceb4489d89c1ce7ed63/recovered/mapped.ddc R275cf1ae994205fb1200126176af79a73c6b1d95924a8ceb4489d89c1ce7ed63
} message]} {
  puts stderr "INVENTORY_FAILED: $message"
  puts stderr $::errorInfo
  exit 1
}
set file [open SUCCESS w]
puts $file "Read-only leaf inventory of original mapped checkpoints."
close $file
exit 0
