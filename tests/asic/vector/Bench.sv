module Bench (
  input logic clk, reset,
  input logic g_load,
  input logic [1:0] g_bank,
  input logic [10:0] g_address,
  input logic [31:0] g_data,
  input logic g_start, g_fused,
  input logic [15:0] g_nodes,
  output logic g_busy, g_done, g_valid,
  input logic g_ready,
  output logic [31:0] g_output,
  input logic v_start,
  input logic [1:0] v_operation,
  input logic [31:0] v_length,
  input logic v_valid,
  output logic v_ready,
  input logic [31:0] v_left, v_right,
  output logic v_out_valid,
  input logic v_out_ready,
  output logic [31:0] v_output,
  output logic v_busy, v_done,
  input logic c_load,
  input logic [1:0] c_bank,
  input logic [10:0] c_address,
  input logic [31:0] c_data,
  input logic c_start,
  input logic [15:0] c_nodes,
  output logic c_accepted,
  output logic [1:0] c_internal_starts,
  output logic c_busy, c_done, c_valid, c_aggregate_done,
  input logic c_ready,
  output logic [31:0] c_output,
  output logic c_link_valid, c_link_ready
);
  Gcn gcn (
    .clk(clk), .reset(reset), .load(g_load), .bank(g_bank),
    .address(g_address), .data(g_data), .start(g_start), .fused(g_fused),
    .nodes(g_nodes), .busy(g_busy), .done(g_done), .out_valid(g_valid),
    .out_ready(g_ready), .out_data(g_output)
  );
  Vector vector (
    .clk(clk), .reset(reset), .start(v_start), .operation(v_operation),
    .length(v_length), .in_valid(v_valid), .in_ready(v_ready),
    .left(v_left), .right(v_right), .out_valid(v_out_valid),
    .out_ready(v_out_ready), .out_data(v_output), .busy(v_busy), .done(v_done)
  );
  GcnVector chain (
    .clk(clk), .reset(reset), .load(c_load), .bank(c_bank),
    .address(c_address), .data(c_data), .start(c_start), .nodes(c_nodes),
    .accepted(c_accepted), .internal_starts(c_internal_starts),
    .busy(c_busy), .aggregate_done(c_aggregate_done), .done(c_done), .out_valid(c_valid), .out_ready(c_ready),
    .out_data(c_output), .link_valid(c_link_valid), .link_ready(c_link_ready)
  );
endmodule
