module GcnVector (
  input logic clk,
  input logic reset,
  input logic load,
  input logic [1:0] bank,
  input logic [10:0] address,
  input logic [31:0] data,
  input logic start,
  input logic [15:0] nodes,
  output logic accepted,
  output logic [1:0] internal_starts,
  output logic busy,
  output logic aggregate_done,
  output logic done,
  output logic out_valid,
  input logic out_ready,
  output logic [31:0] out_data,
  output logic link_valid,
  output logic link_ready
);
  logic g_busy, v_busy;
  logic [31:0] link_data;
  assign busy = g_busy || v_busy;
  assign accepted = start && !busy;
  assign internal_starts = accepted ? 2'd2 : 2'd0;

  Gcn gcn (
    .clk(clk), .reset(reset), .load(load), .bank(bank),
    .address(address), .data(data), .start(accepted), .fused(1'b0),
    .nodes(nodes), .busy(g_busy), .done(aggregate_done), .out_valid(link_valid),
    .out_ready(link_ready), .out_data(link_data)
  );
  Vector vector (
    .clk(clk), .reset(reset), .start(accepted), .operation(2'd0),
    .length({9'd0, nodes, 7'd0}), .in_valid(link_valid),
    .in_ready(link_ready), .left(link_data), .right(32'd0),
    .out_valid(out_valid), .out_ready(out_ready), .out_data(out_data),
    .busy(v_busy), .done(done)
  );
endmodule
