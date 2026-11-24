create_clock -name core -period 2.0 [get_ports clock_uncore]
create_clock -name jtag -period 100.0 [get_ports jtag_TCK]
create_clock -name serial -period 10.0 [get_ports serial_tl_0_clock_in]
set_clock_groups -asynchronous -group core -group jtag -group serial
set clocks [get_ports {clock_uncore jtag_TCK serial_tl_0_clock_in}]
set resets [get_ports {reset_io jtag_reset}]
set core_inputs [remove_from_collection [all_inputs] [get_ports {clock_uncore jtag_TCK serial_tl_0_clock_in reset_io jtag_reset jtag_TMS jtag_TDI serial_tl_0_in_valid serial_tl_0_in_bits_phit* serial_tl_0_out_ready}]]
set core_outputs [remove_from_collection [all_outputs] [get_ports {axi4_mem_0_clock clock_tap jtag_TDO serial_tl_0_in_ready serial_tl_0_out_valid serial_tl_0_out_bits_phit*}]]
set_input_delay 0.1 -clock core $core_inputs
set_output_delay 0.1 -clock core $core_outputs
set_input_delay 0.1 -clock jtag [get_ports {jtag_TMS jtag_TDI}]
set_output_delay 0.1 -clock jtag [get_ports jtag_TDO]
set_input_delay 0.1 -clock serial [get_ports {serial_tl_0_in_valid serial_tl_0_in_bits_phit* serial_tl_0_out_ready}]
set_output_delay 0.1 -clock serial [get_ports {serial_tl_0_in_ready serial_tl_0_out_valid serial_tl_0_out_bits_phit*}]
set_input_transition 0.1 [remove_from_collection [all_inputs] $clocks]
set_load 1.0 [remove_from_collection [all_outputs] [get_ports {axi4_mem_0_clock clock_tap}]]
# Functional placement mode excludes external reset transitions; reset release needs a separate recovery/removal analysis.
set_false_path -from $resets
set_clock_uncertainty 0.05 [all_clocks]
