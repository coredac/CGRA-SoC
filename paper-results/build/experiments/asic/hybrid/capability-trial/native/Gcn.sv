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
