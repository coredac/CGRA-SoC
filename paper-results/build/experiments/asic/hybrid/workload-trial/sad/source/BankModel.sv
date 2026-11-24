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
  reg [1:0] phase;
  reg [511:0] held;
  assign cmd_ready = phase == 0;
  assign rsp_valid = phase == 2;
  assign busy = phase != 0;
  assign result = held;
  assign native_busy = 0;
  assign native_done = 0;
  assign native_result = 0;
  always @(posedge clk) begin
    if (reset) begin phase <= 0; held <= 0; end
    else case (phase)
      0: if (cmd_valid) begin
        for (integer i=0; i<16; i=i+1) begin
          if (mask[i]) case (operation[i*5+:5])
            0: held[i*32+:32] <= a[i*32+:32] + b[i*32+:32];
            1: held[i*32+:32] <= a[i*32+:32] - b[i*32+:32];
            21: held[i*32+:32] <= a[i*32+:32];
            default: held[i*32+:32] <= 0;
          endcase
        end
        phase <= 1;
      end
      1: phase <= 2;
      2: if (rsp_ready) phase <= 0;
    endcase
  end
endmodule
