module Gcn #(
  parameter integer NODES = 16,
  parameter integer EDGES = 256,
  parameter integer CHANNELS = 128,
  parameter integer ADDR_BITS = $clog2(NODES * CHANNELS)
) (
  input logic clk,
  input logic reset,
  input logic load,
  input logic [1:0] bank,
  input logic [ADDR_BITS-1:0] address,
  input logic [31:0] data,
  input logic start,
  input logic fused,
  input logic [15:0] nodes,
  output logic busy,
  output logic done,
  output logic out_valid,
  input logic out_ready,
  output logic signed [31:0] out_data
);
  logic [31:0] rowptr [0:NODES];
  logic [31:0] columns [0:EDGES-1];
  localparam integer FEATURE_BANKS = (NODES * CHANNELS + 127) / 128;
  localparam integer WEIGHT_BANKS = (EDGES + 127) / 128;
  logic [31:0] feature_values [FEATURE_BANKS], weight_values [WEIGHT_BANKS];
  typedef enum logic [2:0] {IDLE, ROW, PREP, EDGE, READ_Z, MUL, MAC, EMIT} state_t;
  state_t state;
  logic [31:0] row, channel, edge_index, edge_start, edge_end, column;
  logic [15:0] rows;
  logic activate;
  logic signed [31:0] weight, value, accumulator, sum;
  logic signed [31:0] product;

  assign busy = state != IDLE;
  assign out_valid = state == EMIT;
  assign sum = accumulator + product;

  for (genvar i = 0; i < FEATURE_BANKS; i = i + 1) begin : feature_banks
    localparam integer WORDS = NODES * CHANNELS - i * 128 < 128 ? NODES * CHANNELS - i * 128 : 128;
    GcnBank #(.WORDS(WORDS)) memory (
      .clk(clk), .we(load && !busy && bank == 3 && (32'(address) >> 7) == i),
      .waddr(7'(address)), .wdata(data),
      .raddr(7'(column * CHANNELS + channel)), .rdata(feature_values[i])
    );
  end
  for (genvar i = 0; i < WEIGHT_BANKS; i = i + 1) begin : weight_banks
    localparam integer WORDS = EDGES - i * 128 < 128 ? EDGES - i * 128 : 128;
    GcnBank #(.WORDS(WORDS)) memory (
      .clk(clk), .we(load && !busy && bank == 2 && (32'(address[$clog2(EDGES)-1:0]) >> 7) == i),
      .waddr(7'(address[$clog2(EDGES)-1:0])), .wdata(data),
      .raddr(7'(edge_index)), .rdata(weight_values[i])
    );
  end

  always_ff @(posedge clk) begin
    if (load && !busy) begin
      case (bank)
        2'd0: rowptr[address[$clog2(NODES+1)-1:0]] <= data;
        2'd1: columns[address[$clog2(EDGES)-1:0]] <= data;
        default: begin end
      endcase
    end
    if (reset) begin
      state <= IDLE;
      done <= 0;
      out_data <= 0;
      row <= 0;
      channel <= 0;
      edge_index <= 0;
      edge_start <= 0;
      edge_end <= 0;
      column <= 0;
      rows <= 0;
      activate <= 0;
      weight <= 0;
      value <= 0;
      accumulator <= 0;
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
          accumulator <= 0;
          edge_index <= edge_start;
          state <= EDGE;
        end
        EDGE: begin
          column <= columns[edge_index];
          weight <= weight_values[edge_index >> 7];
          state <= READ_Z;
        end
        READ_Z: begin
          value <= feature_values[(column * CHANNELS + channel) >> 7];
          state <= MUL;
        end
        MUL: begin
          product <= value * weight;
          state <= MAC;
        end
        MAC: begin
          accumulator <= sum;
          if (edge_index + 1 == edge_end) begin
            out_data <= activate && sum < 0 ? 32'sd0 : sum;
            state <= EMIT;
          end else begin
            edge_index <= edge_index + 1;
            state <= EDGE;
          end
        end
        EMIT: if (out_ready) begin
          if (channel + 1 == CHANNELS) begin
            channel <= 0;
            if (row + 1 == {16'd0, rows}) begin
              done <= 1;
              state <= IDLE;
            end else begin
              row <= row + 1;
              state <= ROW;
            end
          end else begin
            channel <= channel + 1;
            state <= PREP;
          end
        end
        default: state <= IDLE;
      endcase
    end
  end
endmodule

// Keep each local array's read selection and write decode within 128 words.
/* verilator lint_off DECLFILENAME */
module GcnBank #(
  parameter integer WORDS = 128
) (
  input logic clk, we,
  input logic [6:0] waddr, raddr,
  input logic [31:0] wdata,
  output logic [31:0] rdata
);
  logic [31:0] words [WORDS];
  assign rdata = words[raddr];
  always_ff @(posedge clk) if (we) words[waddr] <= wdata;
endmodule
/* verilator lint_on DECLFILENAME */
