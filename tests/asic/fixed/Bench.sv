module FixedBench (
  input logic clk, reset,
  input logic select, write, read,
  input logic [1:0] start,
  input logic [15:0] count,
  input logic [14:0] src0, src1, dst, address,
  input logic [31:0] wdata,
  input logic [1:0] operation,
  output logic [31:0] rdata,
  output logic rvalid,
  output logic [1:0] busy, done, write_pending
);
  logic [31:0] data [2];
  logic [1:0] valid;
  assign rdata = data[select];
  assign rvalid = valid[select];
  for (genvar n = 0; n < 2; n++) begin : cores
    AsicFixed #(.OP(n == 0 ? 1 : 0)) core (
      .clk(clk), .reset(reset), .start(start[n]),
      .unit(3'd7), .operation(operation), .fused(1'b1), .phase(3'd7),
      .count(count), .rows(16'd0), .edges(16'd0), .divisor(32'd0),
      .src0(src0), .src1(src1), .src2(15'd0), .src3(15'd0), .dst(dst),
      .write(write && select == n), .read(read && select == n),
      .address(address), .wdata(wdata), .rdata(data[n]),
      .rvalid(valid[n]), .busy(busy[n]), .done(done[n]), .write_pending(write_pending[n])
    );
  end
endmodule
