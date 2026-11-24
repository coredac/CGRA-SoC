// Dedicated units share only the external test port, not arithmetic hardware.
module GptAsic (
  input logic clk, reset, start, fused,
  input logic [1:0] unit,
  input logic [2:0] phase,
  input logic row,
  input logic [1:0] operation,
  input logic [10:0] count,
  input logic [31:0] divisor,
  input logic write,
  input logic [1:0] bank,
  input logic [9:0] address,
  input logic signed [31:0] wdata,
  output logic signed [31:0] rdata,
  output logic busy, done
);
  logic [31:0] data [3];
  logic [2:0] active, complete;
  GptNorm norm(clk, reset, start && unit == 0, fused, phase, write && unit == 0, bank, address, wdata, data[0], active[0], complete[0]);
  GptSoftmax softmax(clk, reset, start && unit == 1, fused, phase, row, write && unit == 1, address, wdata, data[1], active[1], complete[1]);
  GptVector vector_unit(clk, reset, start && unit == 2, operation, count, divisor, write && unit == 2, bank[0], address, wdata, data[2], active[2], complete[2]);
  assign rdata = data[unit];
  assign busy = active[unit];
  assign done = complete[unit];
endmodule
