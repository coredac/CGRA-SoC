module AsicFixed #(
  parameter integer OP = 0,
  parameter integer WORDS = 16384
) (
  input logic clk, reset, start,
  input logic [2:0] unit,
  input logic [1:0] operation,
  input logic fused,
  input logic [2:0] phase,
  input logic [15:0] count, rows, edges,
  input logic [31:0] divisor,
  input logic [14:0] src0, src1, src2, src3, dst,
  input logic write, read,
  input logic [14:0] address,
  input logic [31:0] wdata,
  output logic [31:0] rdata,
  output logic rvalid, busy, done, write_pending
);
  function automatic [15:0] increment(input logic [15:0] value);
    increment[3:0] = value[3:0] + 4'd1;
    increment[7:4] = value[7:4] + {3'd0, &value[3:0]};
    increment[11:8] = value[11:8] + {3'd0, &value[7:0]};
    increment[15:12] = value[15:12] + {3'd0, &value[11:0]};
  endfunction

  function automatic [15:0] decrement(input logic [15:0] value);
    decrement[3:0] = value[3:0] - 4'd1;
    decrement[7:4] = value[7:4] - {3'd0, value[3:0] == 0};
    decrement[11:8] = value[11:8] - {3'd0, value[7:0] == 0};
    decrement[15:12] = value[15:12] - {3'd0, value[11:0] == 0};
  endfunction

  localparam integer ADDR_BITS = $clog2(WORDS);
  typedef enum logic [1:0] {IDLE, RUN, DRAIN} state_t;
  state_t state;
  logic [14:0] first, second, destination;
  logic [15:0] length, last, issued, completed;
  logic issue_right, receive_right, add_valid, result_valid;
  logic [31:0] left, result, memory_data;
  logic [8:0] sums [4], with_carry [3], selected_sum [4];
  always_comb begin
    selected_sum[0] = sums[0];
    for (integer part = 1; part < 4; part++)
      selected_sum[part] = selected_sum[part-1][8] ? with_carry[part-1] : sums[part];
  end
  logic memory_read, memory_write, memory_valid;
  logic [14:0] memory_source, memory_destination;
  logic [31:0] memory_value;

  assign busy = state != IDLE;
  assign rdata = memory_data;
  assign rvalid = memory_valid && state == IDLE;
  assign memory_read = state == IDLE ? read : state == RUN && issued < length;
  assign memory_source = state == IDLE ? address : (OP == 1 && issue_right ? second : first);
  assign memory_write = state == IDLE ? write : result_valid;
  assign memory_destination = state == IDLE ? address : destination;
  assign memory_value = state == IDLE ? wdata : result;

  AsicFixedMemory #(.WORDS(WORDS)) memory (
    .clk(clk), .reset(reset), .read(memory_read), .write(memory_write),
    .raddr(memory_source[ADDR_BITS-1:0]), .waddr(memory_destination[ADDR_BITS-1:0]),
    .wdata(memory_value), .rdata(memory_data), .rvalid(memory_valid), .write_pending(write_pending)
  );

  always_ff @(posedge clk) begin
    done <= 0;
    add_valid <= 0;
    result_valid <= 0;
    if (state == RUN && memory_read) begin
      if (OP == 1 && !issue_right) begin
        issue_right <= 1;
      end else begin
        issue_right <= 0;
        first <= 15'(increment({1'b0, first}));
        second <= 15'(increment({1'b0, second}));
        issued <= increment(issued);
      end
    end
    if (state == RUN && memory_valid) begin
      if (OP == 1) begin
        receive_right <= !receive_right;
        if (!receive_right) begin
          left <= memory_data;
        end else begin
          for (integer part = 0; part < 4; part++)
            sums[part] <= {1'b0, left[part*8+:8]} + {1'b0, memory_data[part*8+:8]};
          for (integer part = 1; part < 4; part++)
            with_carry[part-1] <= {1'b0, left[part*8+:8]} + {1'b0, memory_data[part*8+:8]} + 9'd1;
          add_valid <= 1;
        end
      end else begin
        result <= memory_data[31] ? 32'd0 : memory_data;
        result_valid <= 1;
      end
    end
    if (OP == 1 && add_valid) begin
      result <= {selected_sum[3][7:0], selected_sum[2][7:0], selected_sum[1][7:0], selected_sum[0][7:0]};
      result_valid <= 1;
    end
    if (result_valid) begin
      destination <= 15'(increment({1'b0, destination}));
      completed <= increment(completed);
      if (completed == last) state <= DRAIN;
    end
    if (state == DRAIN && !write_pending && !result_valid) begin
      state <= IDLE;
      done <= 1;
    end
    if (state == IDLE && start) begin
      state <= RUN;
      first <= src0;
      second <= src1;
      destination <= dst;
      length <= count;
      last <= decrement(count);
      issued <= 0;
      completed <= 0;
      issue_right <= 0;
      receive_right <= 0;
    end
    if (reset) begin
      state <= IDLE;
      issued <= 0;
      completed <= 0;
      issue_right <= 0;
      receive_right <= 0;
      add_valid <= 0;
      result_valid <= 0;
      done <= 0;
    end
  end
endmodule
