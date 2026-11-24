// Fixed vector operations required by the synthetic GPT block.
module GptVectorNative (
  input logic clk, reset, start,
  input logic [1:0] operation,
  input logic [10:0] count,
  input logic [31:0] divisor,
  input logic write, bank,
  input logic [9:0] address,
  input logic signed [31:0] wdata,
  output logic signed [31:0] rdata,
  output logic busy, done,
  output logic div_start, div_signed,
  output logic [31:0] div_a, div_b,
  input logic div_busy, div_done,
  input logic [31:0] div_result
);
  typedef enum logic [4:0] {
    IDLE, LOAD, PREPARE, ABSOLUTE, DELTA, COEFFICIENT, SQUARE,
    CORRECTION, GATE, PRODUCT, BIAS, ACTIVATE, SIGN,
    ROUND_ABS, ROUND_ADD, ROUND_SHIFT, ROUND_SIGN,
    DIV_ABS, DIV_ADD, DIV_START, DIV_WAIT, CLAMP, STORE
  } state_t;
  state_t state;
  logic signed [31:0] source_data, other_data;
  logic [9:0] index;
  logic [10:0] size;
  logic [1:0] op;
  logic [31:0] scale, numerator, round_magnitude;
  logic signed [31:0] value, second, q, magnitude, delta;
  logic signed [31:0] coefficient, square, correction, gate, product, biased, activation;
  logic negative;

  assign div_start = state == DIV_START;
  assign div_signed = 1'b0;
  assign div_a = numerator;
  assign div_b = scale;

  GptVectorMemory source_store (
    .clk(clk), .we(!reset && state == IDLE && write && !bank),
    .waddr(address), .wdata(wdata), .raddr(index), .rdata(source_data)
  );
  GptVectorMemory other_store (
    .clk(clk), .we(!reset && state == IDLE && write && bank),
    .waddr(address), .wdata(wdata), .raddr(index), .rdata(other_data)
  );
  GptVectorMemory result_store (
    .clk(clk), .we(!reset && state == STORE),
    .waddr(index), .wdata(value), .raddr(address), .rdata(rdata)
  );

  // Each arithmetic register keeps the original 32-bit wraparound semantics.
  // Payload registers need no reset: every path writes them before use.
  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      busy <= 0;
      state <= IDLE;
    end else begin
      case (state)
        IDLE: begin
          if (start) begin
            busy <= 1;
            index <= 0;
            size <= count;
            op <= operation;
            scale <= divisor;
            state <= LOAD;
          end
        end
        LOAD: begin
          value <= source_data;
          second <= other_data;
          state <= PREPARE;
        end
        PREPARE: begin
          case (op)
            0: begin q <= value <<< 2; state <= ABSOLUTE; end
            1: begin negative <= value[31]; state <= DIV_ABS; end
            2: begin value <= value + second; state <= CLAMP; end
            default: state <= CLAMP;
          endcase
        end
        ABSOLUTE: begin magnitude <= q < 0 ? -q : q; state <= DELTA; end
        DELTA: begin delta <= (magnitude > 160 ? 160 : magnitude) - 160; state <= COEFFICIENT; end
        COEFFICIENT: begin coefficient <= 591 * delta; state <= SQUARE; end
        SQUARE: begin square <= coefficient * delta; state <= CORRECTION; end
        CORRECTION: begin correction <= square >>> 12; state <= GATE; end
        GATE: begin gate <= q < 0 ? correction : 8192 - correction; state <= PRODUCT; end
        PRODUCT: begin product <= magnitude * gate; state <= BIAS; end
        BIAS: begin biased <= product + 4096; state <= ACTIVATE; end
        ACTIVATE: begin activation <= biased >>> 13; state <= SIGN; end
        SIGN: begin value <= q < 0 ? -activation : activation; state <= ROUND_ABS; end
        ROUND_ABS: begin
          negative <= value[31];
          round_magnitude <= value < 0 ? 32'(-value) : 32'(value);
          state <= ROUND_ADD;
        end
        ROUND_ADD: begin round_magnitude <= round_magnitude + 2; state <= ROUND_SHIFT; end
        ROUND_SHIFT: begin round_magnitude <= round_magnitude >> 2; state <= ROUND_SIGN; end
        ROUND_SIGN: begin value <= negative ? -$signed(round_magnitude) : $signed(round_magnitude); state <= CLAMP; end
        DIV_ABS: begin numerator <= value < 0 ? 32'(-value) : 32'(value); state <= DIV_ADD; end
        DIV_ADD: begin numerator <= numerator + (scale >> 1); state <= DIV_START; end
        DIV_START: state <= DIV_WAIT;
        DIV_WAIT: begin
          if (div_done) begin
            value <= negative ? -$signed(div_result) : $signed(div_result);
            state <= CLAMP;
          end
        end
        CLAMP: begin value <= value > 127 ? 127 : value < -128 ? -128 : value; state <= STORE; end
        STORE: begin
          index <= index + 1'b1;
          state <= LOAD;
          if ({1'b0,index} + 11'd1 == size) begin
            busy <= 0;
            done <= 1;
            state <= IDLE;
          end
        end
        default: state <= IDLE;
      endcase
    end
  end
endmodule

// Preserve asynchronous local-array reads while bounding each bank's decode.
module GptVectorBank (
  input logic clk, we,
  input logic [6:0] waddr, raddr,
  input logic [31:0] wdata,
  output logic [31:0] rdata
);
  logic [31:0] words [128];
  assign rdata = words[raddr];
  always_ff @(posedge clk) if (we) words[waddr] <= wdata;
endmodule

module GptVectorMemory (
  input logic clk, we,
  input logic [9:0] waddr, raddr,
  input logic [31:0] wdata,
  output logic [31:0] rdata
);
  logic [31:0] values [8];
  for (genvar i = 0; i < 8; i = i + 1) begin : banks
    GptVectorBank memory (
      .clk(clk), .we(we && waddr[9:7] == 3'(i)),
      .waddr(waddr[6:0]), .wdata(wdata), .raddr(raddr[6:0]), .rdata(values[i])
    );
  end
  assign rdata = values[raddr[9:7]];
endmodule
