module BiasProof(input [31:0] product, output ok);
  wire [16:0] bias_low = {1'b0, product[15:0]} + 17'd4096;
  wire [15:0] upper = product[31:16] + {15'd0, bias_low[16]};
  wire [31:0] revised = {upper, bias_low[15:0]};
  wire [31:0] original = product + 32'd4096;
  assign ok = revised == original;
endmodule

module SquareSumProof(input [31:0] square_low, input [15:0] square_cross_a, square_cross_b, output ok);
  wire [15:0] square_upper = square_low[31:16] + square_cross_a;
  wire [15:0] upper = square_upper + square_cross_b;
  wire [31:0] revised = {upper, square_low[15:0]};
  wire [31:0] original = square_low + {square_cross_a, 16'd0} + {square_cross_b, 16'd0};
  assign ok = revised == original;
endmodule
