`default_nettype none

module gcd6 (
  input wire clk,
  input wire reset,
  input wire start,
  input wire [5:0] a,
  input wire [5:0] b,
  output reg done,
  output reg [5:0] result
);
  reg busy;
  reg [5:0] x;
  reg [5:0] y;

  always @(posedge clk) begin
    if (reset) begin
      busy <= 1'b0;
      done <= 1'b0;
      result <= 6'd0;
      x <= 6'd0;
      y <= 6'd0;
    end else if (start && !busy) begin
      busy <= 1'b1;
      done <= 1'b0;
      result <= 6'd0;
      x <= a;
      y <= b;
    end else if (busy) begin
      if (y == 6'd0) begin
        result <= x;
        done <= 1'b1;
        busy <= 1'b0;
      end else if (x >= y) begin
        x <= x - y;
      end else begin
        x <= y;
        y <= x;
      end
    end
  end
endmodule

`default_nettype wire
