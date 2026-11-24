// FAMILY: 0 Add, 1 ReLU, 2 GCN, 3 Norm, 4 Softmax, 5 GPT-vector.
// Native commands and primitive packets are mutually exclusive.
module ExpandedAsic #(
  parameter integer FAMILY = 0
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
