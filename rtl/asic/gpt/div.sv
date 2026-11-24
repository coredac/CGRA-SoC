// One restoring division step per clock; operands and signedness latch at start.
module GptDivider (
  input logic clk, reset, start, signed_mode,
  input logic [31:0] dividend, divisor,
  output logic [31:0] quotient,
  output logic busy, done
);
  logic [31:0] numerator, denominator, remainder;
  logic [30:0] partial;
  logic [32:0] shifted, difference;
  logic [31:0] next_partial;
  logic negative;
  logic [4:0] iteration;

  always_comb begin
    shifted = {remainder, numerator[31]};
    difference = shifted - {1'b0, denominator};
    next_partial = {partial, !difference[32]};
  end

  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      busy <= 0;
      quotient <= 0;
      numerator <= 0;
      denominator <= 0;
      partial <= 0;
      remainder <= 0;
      negative <= 0;
      iteration <= 0;
    end else if (start && !busy) begin
      busy <= 1;
      numerator <= signed_mode && dividend[31] ? -dividend : dividend;
      denominator <= signed_mode && divisor[31] ? -divisor : divisor;
      negative <= signed_mode && (dividend[31] != divisor[31]);
      partial <= 0;
      remainder <= 0;
      iteration <= 0;
    end else if (busy) begin
      numerator <= numerator << 1;
      partial <= next_partial[30:0];
      remainder <= difference[32] ? shifted[31:0] : difference[31:0];
      iteration <= iteration + 1'b1;
      if (iteration == 31) begin
        quotient <= negative ? -next_partial : next_partial;
        busy <= 0;
        done <= 1;
      end
    end
  end
endmodule
