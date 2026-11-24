// Optional independent native SAD endpoint sharing its complete primitive bank.
module ExpandedSad (
  input logic clk, reset,
  input logic native_valid,
  output logic native_ready,
  input logic native_last,
  input logic [15:0] native_mask,
  input logic [511:0] native_a, native_b,
  output logic native_rsp_valid,
  input logic native_rsp_ready,
  output logic [31:0] native_result,
  output logic native_busy,
  input logic cmd_valid,
  output logic cmd_ready,
  input logic [15:0] mask,
  input logic [79:0] operation,
  input logic [511:0] a, b, c, d,
  output logic rsp_valid,
  input logic rsp_ready,
  output logic [511:0] result
);
  typedef enum logic [3:0] {
    IDLE, SUB_WAIT, ABS_SEND, ABS_WAIT, REDUCE_SEND, REDUCE_WAIT,
    SUM_SEND, SUM_WAIT, OUTPUT, NEXT_BLOCK
  } state_t;
  state_t state;
  logic bank_valid, bank_ready, bank_rsp_valid, bank_rsp_ready, bank_busy;
  logic [15:0] bank_mask, active;
  logic [79:0] bank_operation;
  logic [511:0] bank_a, bank_b, bank_c, bank_d, bank_result, values;
  logic [31:0] accumulator;
  logic [1:0] round;
  logic last_block, native_accept, primitive_access;

  assign primitive_access = state == IDLE;
  assign cmd_ready = primitive_access && bank_ready;
  assign rsp_valid = primitive_access && bank_rsp_valid;
  assign result = bank_result;
  assign native_ready = bank_ready &&
    ((state == IDLE && !cmd_valid) || state == NEXT_BLOCK);
  assign native_accept = native_valid && native_ready;
  assign native_busy = state != IDLE;
  assign native_rsp_valid = state == OUTPUT;
  assign native_result = accumulator;

  PrimitiveBank primitives (
    .clk(clk), .reset(reset), .cmd_valid(bank_valid), .cmd_ready(bank_ready),
    .mask(bank_mask), .operation(bank_operation),
    .a(bank_a), .b(bank_b), .c(bank_c), .d(bank_d),
    .rsp_valid(bank_rsp_valid), .rsp_ready(bank_rsp_ready), .result(bank_result),
    .busy(bank_busy), .native_start(1'b0), .native_signed(1'b0),
    .native_a(32'd0), .native_b(32'd0),
    .native_busy(), .native_done(), .native_result()
  );

  always_comb begin
    bank_valid = 0;
    bank_mask = 0;
    bank_operation = 0;
    bank_a = 0; bank_b = 0; bank_c = 0; bank_d = 0;
    bank_rsp_ready = 0;
    case (state)
      IDLE, NEXT_BLOCK: begin
        bank_valid = native_valid;
        bank_mask = native_mask;
        bank_operation = {16{5'd1}};
        bank_a = native_b;
        bank_b = native_a;
        if (primitive_access) begin
          bank_rsp_ready = rsp_ready;
          if (cmd_valid) begin
            bank_valid = cmd_valid;
            bank_mask = mask;
            bank_operation = operation;
            bank_a = a; bank_b = b; bank_c = c; bank_d = d;
          end
        end
      end
      SUB_WAIT, ABS_WAIT, REDUCE_WAIT, SUM_WAIT: bank_rsp_ready = 1;
      ABS_SEND: begin
        bank_valid = 1;
        bank_mask = 16'hffff;
        for (integer i = 0; i < 16; i = i + 1) begin
          bank_operation[i*5+:5] = values[i*32+31] ? 5'd1 : 5'd21;
          bank_a[i*32+:32] = values[i*32+31] ? 32'd0 : values[i*32+:32];
          bank_b[i*32+:32] = values[i*32+:32];
        end
      end
      REDUCE_SEND: begin
        bank_valid = 1;
        case (round)
          0: bank_mask = 16'h00ff;
          1: bank_mask = 16'h000f;
          2: bank_mask = 16'h0003;
          3: bank_mask = 16'h0001;
        endcase
        for (integer i = 0; i < 8; i = i + 1) begin
          bank_a[i*32+:32] = values[i*64+:32];
          bank_b[i*32+:32] = values[i*64+32+:32];
        end
      end
      SUM_SEND: begin
        bank_valid = 1;
        bank_mask = 16'h0001;
        bank_a[31:0] = accumulator;
        bank_b[31:0] = values[31:0];
      end
      default: begin end
    endcase
  end

  always_ff @(posedge clk) begin
    if (reset) begin
      state <= IDLE;
      active <= 0;
      values <= 0;
      accumulator <= 0;
      round <= 0;
      last_block <= 0;
    end else begin
      case (state)
        IDLE, NEXT_BLOCK: if (native_accept) begin
          if (state == IDLE) accumulator <= 0;
          active <= native_mask;
          last_block <= native_last;
          state <= SUB_WAIT;
        end
        SUB_WAIT: if (bank_rsp_valid) begin
          for (integer i = 0; i < 16; i = i + 1)
            values[i*32+:32] <= active[i] ? bank_result[i*32+:32] : 32'd0;
          state <= ABS_SEND;
        end
        ABS_SEND: if (bank_ready) state <= ABS_WAIT;
        ABS_WAIT: if (bank_rsp_valid) begin
          values <= bank_result;
          round <= 0;
          state <= REDUCE_SEND;
        end
        REDUCE_SEND: if (bank_ready) state <= REDUCE_WAIT;
        REDUCE_WAIT: if (bank_rsp_valid) begin
          values <= bank_result;
          round <= round + 1'b1;
          state <= round == 3 ? SUM_SEND : REDUCE_SEND;
        end
        SUM_SEND: if (bank_ready) state <= SUM_WAIT;
        SUM_WAIT: if (bank_rsp_valid) begin
          accumulator <= bank_result[31:0];
          state <= last_block ? OUTPUT : NEXT_BLOCK;
        end
        OUTPUT: if (native_rsp_ready) state <= IDLE;
        default: state <= IDLE;
      endcase
    end
  end
endmodule
