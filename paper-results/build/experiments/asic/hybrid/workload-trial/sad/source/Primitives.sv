// Temporary expanded arithmetic service. Native callers share lane zero's divider.
module Restoring (
  input logic clk, reset, start, signed_mode,
  input logic [1:0] mode,
  input logic [31:0] a, b,
  output logic busy, done,
  output logic [31:0] result
);
  logic [31:0] quotient, divisor;
  logic [32:0] remainder, trial, subtract;
  logic [31:0] next_quotient;
  logic [32:0] next_remainder;
  logic [5:0] count;
  logic square_root, take_remainder, negative;
  always_comb begin
    trial = {remainder[31:0], quotient[31]};
    subtract = {1'b0, divisor};
    if (square_root) begin
      trial = (remainder << 2) | {31'd0, divisor[31:30]};
      subtract = ({1'b0, quotient} << 2) | 33'd1;
    end
    next_remainder = trial >= subtract ? trial - subtract : trial;
    next_quotient = {quotient[30:0], trial >= subtract};
  end
  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      busy <= 0;
      result <= 0;
      quotient <= 0;
      divisor <= 0;
      remainder <= 0;
      count <= 0;
      square_root <= 0;
      take_remainder <= 0;
      negative <= 0;
    end else if (start && !busy) begin
      busy <= 1;
      square_root <= mode == 2;
      take_remainder <= mode == 1;
      negative <= signed_mode && (a[31] != b[31]);
      quotient <= mode == 2 ? 32'd0 : signed_mode && a[31] ? -a : a;
      divisor <= mode == 2 ? a : signed_mode && b[31] ? -b : b;
      remainder <= 0;
      count <= mode == 2 ? 6'd16 : 6'd32;
    end else if (busy) begin
      quotient <= next_quotient;
      remainder <= next_remainder;
      if (square_root) divisor <= divisor << 2;
      count <= count - 1'b1;
      if (count == 1) begin
        busy <= 0;
        done <= 1;
        result <= take_remainder ? next_remainder[31:0] : negative ? -next_quotient : next_quotient;
      end
    end
  end
endmodule

module ArithmeticLane (
  input logic clk, reset, in_valid,
  output logic in_ready,
  input logic [4:0] operation,
  input logic [31:0] a, b, c, d,
  output logic out_valid,
  input logic out_ready,
  output logic [31:0] out_data,
  input logic native_start, native_signed,
  input logic [31:0] native_a, native_b,
  output logic native_busy, native_done,
  output logic [31:0] native_result
);
  localparam [4:0] ADD=0, SUB=1, MUL=2, DIV=3, REM=4, SQRT=5,
    AND_OP=6, OR_OP=7, XOR_OP=8, BIT_NOT=9, LOG_NOT=10,
    SHL=11, SHR=12, SAR=13, EQ=14, NE=15, LT=16, GE=17, GT=18,
    SELECT=19, RELU=20, PASS=21, INC=22, GEP2D=23;
  typedef enum logic [2:0] {IDLE, FAST, SLOW, GEP1, GEP2, OUTPUT} state_t;
  state_t state;
  logic [4:0] op;
  logic [31:0] x, y, z, stride, partial, fast_value, multiply;
  logic accept, iterative, slow_start;
  logic [1:0] slow_mode;
  assign in_ready = state == IDLE && !native_busy && !native_start;
  assign accept = in_valid && in_ready;
  assign iterative = operation == DIV || operation == REM || operation == SQRT;
  assign slow_start = native_start || (accept && iterative);
  assign slow_mode = native_start ? 2'd0 : operation == SQRT ? 2'd2 : operation == REM ? 2'd1 : 2'd0;
  Restoring divider (
    .clk(clk), .reset(reset), .start(slow_start),
    .signed_mode(native_start && native_signed), .mode(slow_mode),
    .a(native_start ? native_a : a), .b(native_start ? native_b : b),
    .busy(native_busy), .done(native_done), .result(native_result)
  );
  assign out_valid = state == OUTPUT;
  assign multiply = y * (op == GEP2D ? stride : x);
  always_comb begin
    fast_value = 0;
    case (op)
      ADD: fast_value = x + y;
      SUB: fast_value = x - y;
      MUL: fast_value = multiply;
      AND_OP: fast_value = x & y;
      OR_OP: fast_value = x | y;
      XOR_OP: fast_value = x ^ y;
      BIT_NOT: fast_value = ~x;
      LOG_NOT: fast_value = {31'd0, x == 0};
      SHL: fast_value = x << y;
      SHR: fast_value = x >> y;
      SAR: fast_value = $signed(x) >>> y;
      EQ: fast_value = {31'd0, x == y};
      NE: fast_value = {31'd0, x != y};
      LT: fast_value = {31'd0, $signed(x) < $signed(y)};
      GE: fast_value = {31'd0, $signed(x) >= $signed(y)};
      GT: fast_value = {31'd0, $signed(x) > $signed(y)};
      SELECT: fast_value = z == 32'd1 ? x : y;
      RELU: fast_value = x[31] ? 32'd0 : x;
      PASS: fast_value = x;
      INC: fast_value = x + 1'b1;
      default: begin end
    endcase
  end
  always_ff @(posedge clk) begin
    if (reset) begin
      state <= IDLE;
      op <= 0;
      x <= 0; y <= 0; z <= 0; stride <= 0;
      partial <= 0;
      out_data <= 0;
    end else begin
      case (state)
        IDLE: if (accept) begin
          op <= operation;
          x <= a; y <= b; z <= c; stride <= d;
          state <= iterative ? SLOW : FAST;
        end
        FAST: begin
          if (op == GEP2D) begin partial <= multiply; state <= GEP1; end
          else begin out_data <= fast_value; state <= OUTPUT; end
        end
        GEP1: begin partial <= partial + x; state <= GEP2; end
        GEP2: begin out_data <= partial + z; state <= OUTPUT; end
        SLOW: if (native_done) begin out_data <= native_result; state <= OUTPUT; end
        OUTPUT: if (out_ready) state <= IDLE;
        default: state <= IDLE;
      endcase
    end
  end
endmodule

module PrimitiveBank (
  input logic clk, reset, cmd_valid,
  output logic cmd_ready,
  input logic [15:0] mask,
  input logic [79:0] operation,
  input logic [511:0] a, b, c, d,
  output logic rsp_valid,
  input logic rsp_ready,
  output logic [511:0] result,
  output logic busy,
  input logic native_start, native_signed,
  input logic [31:0] native_a, native_b,
  output logic native_busy, native_done,
  output logic [31:0] native_result
);
  logic [15:0] active, ready, valid;
  logic accepted;
  assign cmd_ready = !busy && (&ready);
  assign accepted = cmd_valid && cmd_ready;
  assign rsp_valid = busy && (&(valid | ~active));
  for (genvar i = 0; i < 16; i = i + 1) begin : lane
    logic unused_busy, unused_done;
    logic [31:0] unused_result;
    ArithmeticLane unit (
      .clk(clk), .reset(reset), .in_valid(accepted && mask[i]), .in_ready(ready[i]),
      .operation(operation[i*5+:5]), .a(a[i*32+:32]), .b(b[i*32+:32]),
      .c(c[i*32+:32]), .d(d[i*32+:32]),
      .out_valid(valid[i]), .out_ready(rsp_valid && rsp_ready), .out_data(result[i*32+:32]),
      .native_start(i == 0 && native_start), .native_signed(native_signed),
      .native_a(native_a), .native_b(native_b),
      .native_busy(unused_busy), .native_done(unused_done), .native_result(unused_result)
    );
    if (i == 0) begin : shared_divider
      assign native_busy = unused_busy;
      assign native_done = unused_done;
      assign native_result = unused_result;
    end
  end
  always_ff @(posedge clk) begin
    if (reset) begin busy <= 0; active <= 0; end
    else begin
      if (accepted) begin busy <= 1; active <= mask; end
      if (rsp_valid && rsp_ready) busy <= 0;
    end
  end
endmodule
