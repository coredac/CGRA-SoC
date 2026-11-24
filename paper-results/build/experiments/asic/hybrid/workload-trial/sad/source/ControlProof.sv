module ControlProof(input clk, input native_rsp_ready);
  reg [7:0] tick = 0;
  wire reset = tick == 0 || tick == 18;
  reg [1:0] issued;
  reg seen;
  wire native_ready, native_rsp_valid, native_busy, cmd_ready, rsp_valid;
  wire [31:0] native_result;
  wire [511:0] result;
  wire native_valid = issued < 3;
  wire [15:0] native_mask = issued == 0 ? 16'h00ff : issued == 1 ? 16'h0003 : 16'h0000;
  reg [511:0] native_a, native_b;
  always_comb begin
    for (integer i=0;i<16;i=i+1) begin
      native_a[i*32+:32] = 32'(i+1);
      native_b[i*32+:32] = issued == 0 ? 32'(i+4) : 32'(2*i+5);
    end
  end
  ExpandedSad dut(
    .clk(clk),.reset(reset),.native_valid(native_valid),.native_ready(native_ready),
    .native_last(issued == 2),.native_mask(native_mask),.native_a(native_a),.native_b(native_b),
    .native_rsp_valid(native_rsp_valid),.native_rsp_ready(native_rsp_ready),.native_result(native_result),.native_busy(native_busy),
    .cmd_valid(tick == 1 || tick == 19),.cmd_ready(cmd_ready),.mask(16'h0001),.operation(80'd0),
    .a(512'd10),.b(512'd20),.c(512'd0),.d(512'd0),.rsp_valid(rsp_valid),.rsp_ready(1'b1),.result(result)
  );
  always @(posedge clk) begin
    if (tick < 110) tick <= tick + 1'b1;
    if (reset) begin issued <= 0; seen <= 0; end
    else begin
      if (native_valid && native_ready) issued <= issued + 1'b1;
      if (native_rsp_valid) seen <= 1;
      if (native_busy) begin
        assert (!cmd_ready);
        assert (!rsp_valid);
      end
      if (rsp_valid) assert(result[31:0] == 30);
      if (native_rsp_valid) assert(native_result == 33);
      if (tick == 105) assert(seen);
    end
    if (tick > 0 && $past(reset)) begin
      assert(!native_busy && !native_rsp_valid && native_result == 0);
    end
    if (tick > 0 && !reset && $past(native_rsp_valid && !native_rsp_ready && !reset)) begin
      assert(native_rsp_valid && native_result == $past(native_result));
    end
  end
endmodule
