// FAMILY: 0 Add, 1 ReLU, 2 GCN, 3 Norm, 4 Softmax, 5 GPT-vector.
// Native commands and primitive packets are mutually exclusive.
module ExpandedAsic #(
  parameter integer FAMILY = 5
) (
  input logic clk, reset,
  input logic native_start,
  output logic native_ready,
  input logic fused,
  input logic [2:0] phase,
  input logic [1:0] native_operation,
  input logic [31:0] length, divisor,
  input logic [15:0] nodes,
  input logic row,
  input logic write,
  input logic [1:0] bank,
  input logic [10:0] address,
  input logic [31:0] wdata,
  output logic [31:0] rdata,
  input logic in_valid,
  output logic in_ready,
  input logic [31:0] left, right,
  output logic out_valid,
  input logic out_ready,
  output logic [31:0] out_data,
  output logic native_busy, native_done,
  input logic cmd_valid,
  output logic cmd_ready,
  input logic [15:0] mask,
  input logic [79:0] operation,
  input logic [511:0] a, b, c, d,
  output logic rsp_valid,
  input logic rsp_ready,
  output logic [511:0] result
);
  logic accept_native, bank_busy, bank_valid, bank_ready;
  logic native_write;
  logic bank_rsp_valid, bank_rsp_ready;
  logic [15:0] bank_mask;
  logic [79:0] bank_operation;
  logic [511:0] bank_a, bank_b, bank_c, bank_d, bank_result;
  logic div_start, div_signed, div_busy, div_done;
  logic [31:0] div_a, div_b, div_result;
  assign native_ready = !native_busy && !bank_busy && !cmd_valid;
  assign accept_native = native_start && native_ready;
  assign native_write = write && !bank_busy && !cmd_valid;
  assign cmd_ready = !native_busy && bank_ready;
  assign rsp_valid = !native_busy && bank_rsp_valid;
  assign result = bank_result;
  PrimitiveBank primitives (
    .clk(clk), .reset(reset), .cmd_valid(bank_valid), .cmd_ready(bank_ready),
    .mask(bank_mask), .operation(bank_operation), .a(bank_a), .b(bank_b), .c(bank_c), .d(bank_d),
    .rsp_valid(bank_rsp_valid), .rsp_ready(bank_rsp_ready), .result(bank_result), .busy(bank_busy),
    .native_start(div_start), .native_signed(div_signed), .native_a(div_a), .native_b(div_b),
    .native_busy(div_busy), .native_done(div_done), .native_result(div_result)
  );

  if (FAMILY == 0 || FAMILY == 1) begin : streaming
    logic [31:0] size, received, sent;
    assign rdata = 0;
    assign out_valid = native_busy && bank_rsp_valid;
    assign out_data = bank_result[31:0];
    assign in_ready = native_busy && received < size && bank_ready;
    assign bank_valid = native_busy ? in_valid && received < size : cmd_valid;
    assign bank_mask = native_busy ? 16'h0001 : mask;
    assign bank_operation = native_busy ? (FAMILY == 0 ? {16{5'd0}} : {16{5'd20}}) : operation;
    assign bank_a = native_busy ? {480'd0, left} : a;
    assign bank_b = native_busy ? {480'd0, right} : b;
    assign bank_c = native_busy ? 512'd0 : c;
    assign bank_d = native_busy ? 512'd0 : d;
    assign bank_rsp_ready = native_busy ? out_ready : rsp_ready;
    assign div_start = 0;
    assign div_signed = 0;
    assign div_a = 0;
    assign div_b = 0;
    always_ff @(posedge clk) begin
      native_done <= 0;
      if (reset) begin
        native_busy <= 0;
        size <= 0; received <= 0; sent <= 0;
      end else begin
        if (accept_native) begin
          native_busy <= 1;
          size <= length;
          received <= 0;
          sent <= 0;
        end
        if (in_valid && in_ready) received <= received + 1'b1;
        if (out_valid && out_ready) begin
          sent <= sent + 1'b1;
          if (sent + 1'b1 == size) begin native_busy <= 0; native_done <= 1; end
        end
      end
    end
  end else if (FAMILY == 2) begin : gcn
    logic arith_valid, arith_rsp_ready;
    logic [79:0] arith_operation;
    logic [511:0] arith_a, arith_b;
    assign rdata = 0;
    assign in_ready = 0;
    assign bank_valid = native_busy ? arith_valid : cmd_valid;
    assign bank_mask = native_busy ? 16'hffff : mask;
    assign bank_operation = native_busy ? arith_operation : operation;
    assign bank_a = native_busy ? arith_a : a;
    assign bank_b = native_busy ? arith_b : b;
    assign bank_c = native_busy ? 512'd0 : c;
    assign bank_d = native_busy ? 512'd0 : d;
    assign bank_rsp_ready = native_busy ? arith_rsp_ready : rsp_ready;
    assign div_start = 0;
    assign div_signed = 0;
    assign div_a = 0;
    assign div_b = 0;
    GcnNative kernel (
      .clk(clk), .reset(reset), .load(native_write), .bank(bank), .address(address), .data(wdata),
      .start(accept_native), .fused(fused), .nodes(nodes),
      .busy(native_busy), .done(native_done), .out_valid(out_valid), .out_ready(out_ready), .out_data(out_data),
      .arith_valid(arith_valid), .arith_ready(bank_ready), .arith_operation(arith_operation),
      .arith_a(arith_a), .arith_b(arith_b), .arith_result(bank_result),
      .arith_rsp_valid(bank_rsp_valid), .arith_rsp_ready(arith_rsp_ready)
    );
  end else begin : gpt
    assign in_ready = 0;
    assign out_valid = 0;
    assign out_data = 0;
    assign bank_valid = cmd_valid && !native_busy;
    assign bank_mask = mask;
    assign bank_operation = operation;
    assign bank_a = a; assign bank_b = b; assign bank_c = c; assign bank_d = d;
    assign bank_rsp_ready = rsp_ready && !native_busy;
    if (FAMILY == 3) begin : norm
      GptNormNative kernel (
        .clk(clk), .reset(reset), .start(accept_native), .fused(fused), .phase(phase),
        .write(native_write), .bank(bank), .address(address[9:0]), .wdata(wdata), .rdata(rdata),
        .busy(native_busy), .done(native_done), .div_start(div_start), .div_signed(div_signed),
        .div_a(div_a), .div_b(div_b), .div_busy(div_busy), .div_done(div_done), .div_result(div_result)
      );
    end else if (FAMILY == 4) begin : softmax
      GptSoftmaxNative kernel (
        .clk(clk), .reset(reset), .start(accept_native), .fused(fused), .phase(phase), .row(row),
        .write(native_write), .address(address[9:0]), .wdata(wdata), .rdata(rdata),
        .busy(native_busy), .done(native_done), .div_start(div_start), .div_signed(div_signed),
        .div_a(div_a), .div_b(div_b), .div_busy(div_busy), .div_done(div_done), .div_result(div_result)
      );
    end else begin : vector_unit
      GptVectorNative kernel (
        .clk(clk), .reset(reset), .start(accept_native), .operation(native_operation),
        .count(length[10:0]), .divisor(divisor), .write(native_write), .bank(bank[0]),
        .address(address[9:0]), .wdata(wdata), .rdata(rdata),
        .busy(native_busy), .done(native_done), .div_start(div_start), .div_signed(div_signed),
        .div_a(div_a), .div_b(div_b), .div_busy(div_busy), .div_done(div_done), .div_result(div_result)
      );
    end
  end
endmodule

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

// Temporary 16-channel trial for the existing 16-node, 128-channel GCN shape.
// Feature storage is striped across lanes; at most seven operands are captured
// per READ_Z cycle. This is not a replacement for the SoC SPM interface.
module GcnNative (
  input logic clk, reset, load,
  input logic [1:0] bank,
  input logic [10:0] address,
  input logic [31:0] data,
  input logic start, fused,
  input logic [15:0] nodes,
  output logic busy, done, out_valid,
  input logic out_ready,
  output logic signed [31:0] out_data,
  output logic arith_valid,
  input logic arith_ready,
  output logic [79:0] arith_operation,
  output logic [511:0] arith_a, arith_b,
  input logic [511:0] arith_result,
  input logic arith_rsp_valid,
  output logic arith_rsp_ready
);
  localparam integer LANES = 16;
  localparam integer READS = 7;
  logic [31:0] rowptr [0:16];
  logic [31:0] columns [0:255];
  logic [31:0] feature_values [LANES], weight_values [2];
  typedef enum logic [3:0] {IDLE, ROW, PREP, EDGE, READ_Z, MUL, MUL_WAIT, MAC, MAC_WAIT, EMIT} state_t;
  state_t state;
  logic [31:0] row, channel, edge_index, edge_start, edge_end, column;
  logic [15:0] rows;
  logic activate;
  logic [4:0] read_base;
  logic [3:0] emit_lane;
  logic signed [31:0] weight;
  logic signed [31:0] value [LANES], product [LANES];
  logic signed [31:0] accumulator [LANES], sum [LANES], result [LANES];

  assign arith_valid = state == MUL || state == MAC;
  assign arith_operation = state == MUL ? {16{5'd2}} : {16{5'd0}};
  assign arith_rsp_ready = state == MUL_WAIT || state == MAC_WAIT;
  assign busy = state != IDLE;
  assign out_valid = state == EMIT;
  assign out_data = result[emit_lane];

  for (genvar i = 0; i < LANES; i = i + 1) begin : lanes
    // Channel groups are aligned to 16, so all banks use the same word address.
    GcnBank memory (
      .clk(clk), .we(load && !busy && bank == 3 && address[3:0] == 4'(i)),
      .waddr(address[10:4]), .wdata(data),
      .raddr(7'((column * 128 + channel) >> 4)), .rdata(feature_values[i])
    );
    assign sum[i] = arith_result[i*32+:32];
    assign arith_a[i*32+:32] = state == MUL ? value[i] : accumulator[i];
    assign arith_b[i*32+:32] = state == MUL ? weight : product[i];
  end
  for (genvar i = 0; i < 2; i = i + 1) begin : weight_banks
    GcnBank memory (
      .clk(clk), .we(load && !busy && bank == 2 && address[7] == 1'(i)),
      .waddr(address[6:0]), .wdata(data),
      .raddr(7'(edge_index)), .rdata(weight_values[i])
    );
  end

  always_ff @(posedge clk) begin
    if (load && !busy) begin
      case (bank)
        2'd0: rowptr[address[4:0]] <= data;
        2'd1: columns[address[7:0]] <= data;
        default: begin end
      endcase
    end
    if (reset) begin
      state <= IDLE;
      done <= 0;
      row <= 0;
      channel <= 0;
      edge_index <= 0;
      edge_start <= 0;
      edge_end <= 0;
      column <= 0;
      rows <= 0;
      activate <= 0;
      weight <= 0;
      read_base <= 0;
      emit_lane <= 0;
      for (integer i = 0; i < LANES; i = i + 1) begin
        value[i] <= 0;
        accumulator[i] <= 0;
        result[i] <= 0;
      end
    end else begin
      done <= 0;
      case (state)
        IDLE: if (start) begin
          rows <= nodes;
          activate <= fused;
          row <= 0;
          channel <= 0;
          state <= ROW;
        end
        ROW: begin
          edge_start <= rowptr[row];
          edge_end <= rowptr[row + 1];
          state <= PREP;
        end
        PREP: begin
          for (integer i = 0; i < LANES; i = i + 1) accumulator[i] <= 0;
          edge_index <= edge_start;
          state <= EDGE;
        end
        EDGE: begin
          column <= columns[edge_index];
          weight <= weight_values[edge_index >> 7];
          read_base <= 0;
          state <= READ_Z;
        end
        READ_Z: begin
          for (integer i = 0; i < LANES; i = i + 1) begin
            if (i >= 32'(read_base) && i < 32'(read_base) + READS)
              value[i] <= feature_values[i];
          end
          if (32'(read_base) + READS >= LANES) state <= MUL;
          else read_base <= read_base + 5'(READS);
        end
        MUL: if (arith_ready) state <= MUL_WAIT;
        MUL_WAIT: if (arith_rsp_valid) begin
          for (integer i = 0; i < LANES; i = i + 1) product[i] <= arith_result[i*32+:32];
          state <= MAC;
        end
        MAC: if (arith_ready) state <= MAC_WAIT;
        MAC_WAIT: if (arith_rsp_valid) begin
          for (integer i = 0; i < LANES; i = i + 1) begin
            accumulator[i] <= sum[i];
            if (edge_index + 1 == edge_end)
              result[i] <= activate && sum[i] < 0 ? 32'sd0 : sum[i];
          end
          if (edge_index + 1 == edge_end) begin
            emit_lane <= 0;
            state <= EMIT;
          end else begin
            edge_index <= edge_index + 1;
            state <= EDGE;
          end
        end
        EMIT: if (out_ready) begin
          if (emit_lane != 15) emit_lane <= emit_lane + 1;
          else if (channel + LANES == 128) begin
            channel <= 0;
            if (row + 1 == {16'd0, rows}) begin
              done <= 1;
              state <= IDLE;
            end else begin
              row <= row + 1;
              state <= ROW;
            end
          end else begin
            channel <= channel + LANES;
            state <= PREP;
          end
        end
        default: state <= IDLE;
      endcase
    end
  end
endmodule

/* verilator lint_off DECLFILENAME */
module GcnBank (
  input logic clk, we,
  input logic [6:0] waddr, raddr,
  input logic [31:0] wdata,
  output logic [31:0] rdata
);
  logic [31:0] words [128];
  assign rdata = words[raddr];
  always_ff @(posedge clk) if (we) words[waddr] <= wdata;
endmodule
/* verilator lint_on DECLFILENAME */

// Fixed H128 LayerNorm with the repository's Q6/Q8 integer arithmetic.
module GptNormNative (
  input logic clk, reset, start, fused,
  input logic [2:0] phase,
  input logic write,
  input logic [1:0] bank,
  input logic [9:0] address,
  input logic signed [31:0] wdata,
  output logic signed [31:0] rdata,
  output logic busy, done,
  output logic div_start, div_signed,
  output logic [31:0] div_a, div_b,
  input logic div_busy, div_done,
  input logic [31:0] div_result
);
  logic signed [31:0] source [128], gain [128], offset [128], result [128];
  logic [2:0] stage, step;
  logic automatic_run;
  logic [6:0] index;
  logic signed [31:0] sum, mean;
  logic [31:0] squares, variance, std;
  logic [31:0] radicand;
  logic [33:0] remainder;
  logic [15:0] root;
  logic [4:0] iteration;
  logic signed [31:0] sample, factor, bias, delta, product, affine, rounded;
  logic signed [63:0] square;
  logic [33:0] next_rem, trial;
  logic [15:0] next_root;

  assign div_start = busy && stage == 5 && step == 3;
  assign div_signed = 1'b1;
  assign div_a = product;
  assign div_b = std;

  always_comb begin
    rdata = result[address[6:0]];
    next_rem = (remainder << 2) | {32'b0, radicand[31:30]};
    trial = ({18'b0, root} << 2) | 34'd1;
    next_root = {root[14:0], (next_rem >= trial)};
  end

  task automatic advance(input logic [2:0] next_stage);
    begin
      index <= 0;
      step <= 0;
      if (automatic_run && stage != 6) stage <= next_stage;
      else begin busy <= 0; done <= 1; end
    end
  endtask

  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      busy <= 0;
      stage <= 0;
      index <= 0;
      automatic_run <= 0;
      sum <= 0; mean <= 0; squares <= 0; variance <= 0; std <= 0;
      radicand <= 0; remainder <= 0; root <= 0; iteration <= 0;
      step <= 0;
    end else if (!busy) begin
      if (write) begin
        case (bank)
          0: source[address[6:0]] <= wdata;
          1: gain[address[6:0]] <= wdata;
          2: offset[address[6:0]] <= wdata;
          default: result[address[6:0]] <= wdata;
        endcase
      end
      if (start) begin
        busy <= 1;
        automatic_run <= fused;
        stage <= fused ? 3'd0 : phase;
        step <= 0;
        index <= 0;
        if (fused || phase == 0) sum <= 0;
        if (phase == 2) squares <= 0;
        if (phase == 4) begin
          radicand <= (variance << 4) + 4096;
          remainder <= 0; root <= 0; iteration <= 0;
        end
      end
    end else begin
      case (stage)
        0: begin
          if (step == 0) begin
            sample <= source[index];
            step <= 1;
          end else begin
            sum <= sum + sample;
            step <= 0;
            index <= index + 1'b1;
            if (index == 127) advance(1);
          end
        end
        1: begin
          // Width is 128, so arithmetic shift implements the required floor.
          mean <= (sum <<< 6) >>> 7;
          squares <= 0;
          advance(2);
        end
        2: begin
          case (step)
            0: begin sample <= source[index]; step <= 1; end
            1: begin delta <= (sample <<< 6) - mean; step <= 2; end
            2: begin square <= 64'(delta) * 64'(delta); step <= 3; end
            3: begin
              squares <= squares + 32'(square >> 4);
              step <= 0;
              index <= index + 1'b1;
              if (index == 127) advance(3);
            end
            default: step <= 0;
          endcase
        end
        3: begin
          variance <= squares >> 7;
          radicand <= ((squares >> 7) << 4) + 4096;
          remainder <= 0; root <= 0; iteration <= 0;
          advance(4);
        end
        4: begin
          root <= next_root;
          remainder <= next_rem >= trial ? next_rem - trial : next_rem;
          radicand <= radicand << 2;
          iteration <= iteration + 1'b1;
          if (iteration == 15) begin std <= {16'b0, next_root}; advance(5); end
        end
        5: begin
          case (step)
            0: begin
              sample <= source[index];
              factor <= gain[index];
              bias <= offset[index];
              step <= 1;
            end
            1: begin delta <= (sample <<< 6) - mean; step <= 2; end
            2: begin product <= delta * factor; step <= 3; end
            3: step <= 4;
            4: if (div_done) begin affine <= $signed(div_result) + bias; step <= 5; end
            5: begin
              result[index] <= affine;
              step <= 0;
              index <= index + 1'b1;
              if (index == 127) advance(6);
            end
            default: step <= 0;
          endcase
        end
        6: begin
          case (step)
            0: begin sample <= result[index]; step <= 1; end
            1: begin
              rounded <= sample < 0 ? -(((-sample) + 2) >>> 2) : (sample + 2) >>> 2;
              step <= 2;
            end
            2: begin
              result[index] <= rounded > 127 ? 127 : rounded < -128 ? -128 : rounded;
              step <= 0;
              index <= index + 1'b1;
              if (index == 127) advance(0);
            end
            default: step <= 0;
          endcase
        end
        default: begin busy <= 0; done <= 1; end
      endcase
    end
  end
endmodule

// Two-column causal Softmax, matching attention_data.probabilities.
module GptSoftmaxNative (
  input logic clk, reset, start, fused,
  input logic [2:0] phase,
  input logic row,
  input logic write,
  input logic [9:0] address,
  input logic signed [31:0] wdata,
  output logic signed [31:0] rdata,
  output logic busy, done,
  output logic div_start, div_signed,
  output logic [31:0] div_a, div_b,
  input logic div_busy, div_done,
  input logic [31:0] div_result
);
  logic signed [31:0] source [2], result [2], maximum;
  logic [15:0] exponent [2], exp_value;
  logic [16:0] total, square;
  logic [1:0] stage;
  logic [3:0] step;
  logic index, limit, automatic_run;
  logic signed [31:0] difference;
  logic [31:0] halved;
  logic [12:0] distance, correction, base;
  logic [20:0] scaled;
  logic [4:0] shift;
  logic [8:0] polynomial;
  logic [17:0] biased;
  logic [22:0] product, numerator;

  assign rdata = result[address[0]];
  assign halved = 32'(difference >>> 1);
  assign div_start = busy && stage == 2 && step == 2;
  assign div_signed = 1'b0;
  assign div_a = {9'd0, numerator};
  assign div_b = {15'd0, total};

  task automatic advance(input logic [1:0] next_stage);
    begin
      index <= 0;
      step <= 0;
      if (automatic_run && stage != 2) stage <= next_stage;
      else begin busy <= 0; done <= 1; end
    end
  endtask

  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      busy <= 0; stage <= 0; step <= 0; index <= 0; limit <= 0; automatic_run <= 0;
      maximum <= 0; total <= 0;
    end else if (!busy) begin
      if (write) source[address[0]] <= wdata;
      if (start) begin
        busy <= 1; automatic_run <= fused; stage <= fused ? 2'd0 : phase[1:0];
        index <= 0; step <= 0; limit <= row;
        if (fused || phase == 0) maximum <= -32'sd2147483647 - 1;
        if (fused || phase == 1) total <= 0;
      end
    end else begin
      case (stage)
        0: begin
          if (index <= limit && source[index] > maximum) maximum <= source[index];
          index <= 1;
          if (index) advance(1);
        end
        1: begin
          step <= step + 1'b1;
          // Clamping bounds polynomial to167..346, exponent to45650 and sum to91300.
          case (step)
            0: difference <= maximum - source[index];
            1: distance <= halved > 4096 ? 13'd4096 : halved[12:0];
            2: scaled <= 21'(distance) * 21'd370;
            3: shift <= scaled[20:16];
            4: correction <= 13'(shift) * 13'd177;
            5: base <= 13'd346 + correction;
            6: polynomial <= 9'(base - distance);
            7: square <= 17'(polynomial) * 17'(polynomial);
            8: biased <= {1'b0, square} + 18'd62885;
            9: exp_value <= 16'(biased >> (shift + 5'd2));
            10: begin
              exponent[index] <= index <= limit ? exp_value : 16'd0;
              if (index <= limit) total <= total + {1'b0, exp_value};
              step <= 0;
              index <= 1;
              if (index) advance(2);
            end
            default: step <= 0;
          endcase
        end
        2: begin
          case (step)
            0: begin product <= 23'(exponent[index]) * 23'd127; step <= 1; end
            1: begin numerator <= product + {7'd0, total[16:1]}; step <= 2; end
            2: step <= 3;
            3: if (div_done) begin
              result[index] <= div_result;
              step <= 0;
              index <= 1;
              if (index) advance(0);
            end
            default: step <= 0;
          endcase
        end
        default: begin busy <= 0; done <= 1; end
      endcase
    end
  end
endmodule

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
    IDLE, LOAD, PREPARE, ABSOLUTE, DELTA, COEFFICIENT, SQUARE, SQUARE_SUM, SQUARE_FINISH,
    CORRECTION, GATE, PRODUCT, BIAS, BIAS_FINISH, ACTIVATE, SIGN,
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
  logic [31:0] square_low;
  logic [15:0] square_cross_a, square_cross_b, square_upper;
  logic [16:0] bias_low;

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
        SQUARE: begin
          square_low <= coefficient[15:0] * delta[15:0];
          square_cross_a <= coefficient[31:16] * delta[15:0];
          square_cross_b <= coefficient[15:0] * delta[31:16];
          state <= SQUARE_SUM;
        end
        SQUARE_SUM: begin
          square_upper <= square_low[31:16] + square_cross_a;
          state <= SQUARE_FINISH;
        end
        SQUARE_FINISH: begin
          square <= {16'(square_upper + square_cross_b), square_low[15:0]};
          state <= CORRECTION;
        end
        CORRECTION: begin correction <= square >>> 12; state <= GATE; end
        GATE: begin gate <= q < 0 ? correction : 8192 - correction; state <= PRODUCT; end
        PRODUCT: begin product <= magnitude * gate; state <= BIAS; end
        BIAS: begin
          bias_low <= {1'b0, product[15:0]} + 17'd4096;
          state <= BIAS_FINISH;
        end
        BIAS_FINISH: begin
          biased <= {16'(product[31:16] + {15'd0, bias_low[16]}), bias_low[15:0]};
          state <= ACTIVATE;
        end
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
