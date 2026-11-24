module Vector (
  input logic clk,
  input logic reset,
  input logic start,
  input logic [1:0] operation,
  input logic [31:0] length,
  input logic in_valid,
  output logic in_ready,
  input logic signed [31:0] left,
  input logic signed [31:0] right,
  output logic out_valid,
  input logic out_ready,
  output logic signed [31:0] out_data,
  output logic busy,
  output logic done
);
  logic [1:0] op;
  logic [31:0] count, received, sent;
  logic full;
  logic signed [31:0] value;
  assign value = op == 0 ? left : left + right;
  assign in_ready = busy && received < count && (!full || out_ready);
  assign out_valid = full;

  always_ff @(posedge clk) begin
    if (reset) begin
      busy <= 0;
      done <= 0;
      full <= 0;
      received <= 0;
      sent <= 0;
      count <= 0;
      op <= 0;
      out_data <= 0;
    end else begin
      done <= 0;
      if (start && !busy) begin
        busy <= 1;
        count <= length;
        received <= 0;
        sent <= 0;
        op <= operation;
      end
      if (in_valid && in_ready) begin
        out_data <= op != 1 && value < 0 ? 32'sd0 : value;
        full <= 1;
        received <= received + 1;
      end else if (out_valid && out_ready) begin
        full <= 0;
      end
      if (out_valid && out_ready) begin
        sent <= sent + 1;
        if (sent + 1 == count) begin
          busy <= 0;
          done <= 1;
        end
      end
    end
  end
endmodule
