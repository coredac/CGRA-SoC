module AsicCore #(
  parameter integer FAMILY = -1,
  parameter integer WORDS = 32768
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
  localparam integer ADDR_BITS = $clog2(WORDS);
  assign write_pending = 1'b0;
  localparam logic [2:0] GCN = 0, VECTOR = 1, NORM = 2, SOFTMAX = 3, GPT_VECTOR = 4;
  typedef enum logic [3:0] {
    IDLE, LOAD_READ, LOAD_WRITE, RUN_START, RUN_WAIT, STORE,
    LEFT_READ, LEFT_CAPTURE, RIGHT_READ, RIGHT_CAPTURE, FEED, DRAIN, COMPLETE
  } state_t;
  state_t state;
  logic mem_read, mem_write;
  logic [14:0] mem_address, mem_destination;
  logic [31:0] mem_data, mem_value;
  logic [2:0] selected_unit, job_unit, job_phase;
  logic [1:0] job_operation, load_bank;
  logic job_fused;
  logic [15:0] job_count, job_rows, job_edges, index, emitted;
  logic [31:0] job_divisor;
  logic [14:0] sources [4];
  logic [14:0] destination, load_address;
  logic [15:0] load_count, output_count;
  logic [1:0] last_bank;
  logic signed [31:0] left, right;
  logic [4:0] native_busy, native_done;
  logic gcn_valid, vector_ready, vector_valid;
  logic signed [31:0] gcn_data, vector_data, norm_data, softmax_data, gpt_data;
  logic loading, starting, vector_accept;

  assign selected_unit = FAMILY < 0 ? unit : 3'(FAMILY);
  assign loading = state == LOAD_WRITE;
  assign starting = state == RUN_START;
  assign vector_accept = state == DRAIN;
  assign rdata = mem_data;

  if (FAMILY < 0 || FAMILY == int'(GCN)) begin : family_gcn
    Gcn gcn (
      .clk(clk), .reset(reset), .load(loading && job_unit == GCN),
      .bank(load_bank), .address(index[10:0]), .data(mem_data),
      .start(starting && job_unit == GCN), .fused(job_fused), .nodes(job_rows),
      .busy(native_busy[0]), .done(native_done[0]), .out_valid(gcn_valid),
      .out_ready(state == RUN_WAIT && job_unit == GCN), .out_data(gcn_data)
    );
  end else begin : no_gcn
    assign native_busy[0] = '0;
    assign native_done[0] = '0;
    assign gcn_valid = '0;
    assign gcn_data = '0;
  end
  if (FAMILY < 0 || FAMILY == int'(VECTOR)) begin : family_vector
    Vector vector_unit (
      .clk(clk), .reset(reset), .start(starting && job_unit == VECTOR),
      .operation(job_operation), .length({16'd0, job_count}),
      .in_valid(state == FEED), .in_ready(vector_ready), .left(left), .right(right),
      .out_valid(vector_valid), .out_ready(vector_accept), .out_data(vector_data),
      .busy(native_busy[1]), .done(native_done[1])
    );
  end else begin : no_vector
    assign native_busy[1] = '0;
    assign native_done[1] = '0;
    assign vector_ready = '0;
    assign vector_valid = '0;
    assign vector_data = '0;
  end
  if (FAMILY < 0 || FAMILY == int'(NORM)) begin : family_norm
    GptNorm norm (
      .clk(clk), .reset(reset), .start(starting && job_unit == NORM),
      .fused(job_fused), .phase(job_phase), .write(loading && job_unit == NORM),
      .bank(load_bank), .address(index[9:0]), .wdata(mem_data), .rdata(norm_data),
      .busy(native_busy[2]), .done(native_done[2])
    );
  end else begin : no_norm
    assign native_busy[2] = '0;
    assign native_done[2] = '0;
    assign norm_data = '0;
  end
  if (FAMILY < 0 || FAMILY == int'(SOFTMAX)) begin : family_softmax
    GptSoftmax softmax (
      .clk(clk), .reset(reset), .start(starting && job_unit == SOFTMAX),
      .fused(job_fused), .phase(job_phase), .row(job_rows[0]),
      .write(loading && job_unit == SOFTMAX), .address(index[9:0]),
      .wdata(mem_data), .rdata(softmax_data), .busy(native_busy[3]), .done(native_done[3])
    );
  end else begin : no_softmax
    assign native_busy[3] = '0;
    assign native_done[3] = '0;
    assign softmax_data = '0;
  end
  if (FAMILY < 0 || FAMILY == int'(GPT_VECTOR)) begin : family_gpt_vector
    GptVector gpt_vector (
      .clk(clk), .reset(reset), .start(starting && job_unit == GPT_VECTOR),
      .operation(job_operation), .count(job_count[10:0]), .divisor(job_divisor),
      .write(loading && job_unit == GPT_VECTOR), .bank(load_bank[0]),
      .address(index[9:0]), .wdata(mem_data), .rdata(gpt_data),
      .busy(native_busy[4]), .done(native_done[4])
    );
  end else begin : no_gpt_vector
    assign native_busy[4] = '0;
    assign native_done[4] = '0;
    assign gpt_data = '0;
  end

  always_comb begin
    load_count = job_count;
    last_bank = 0;
    output_count = job_count;
    case (job_unit)
      GCN: begin
        last_bank = 3;
        output_count = job_rows << 7;
        case (load_bank)
          0: load_count = job_rows + 1'b1;
          1, 2: load_count = job_edges;
          3: load_count = job_rows << 7;
        endcase
      end
      NORM: begin
        last_bank = 2;
        load_count = 128;
        output_count = job_fused || job_phase >= 5 ? 16'd128 : 16'd0;
      end
      SOFTMAX: begin
        load_count = 2;
        output_count = job_fused || job_phase == 2 ? 16'd2 : 16'd0;
      end
      GPT_VECTOR: last_bank = job_operation == 2 ? 2'd1 : 2'd0;
      default: begin end
    endcase
  end

  always_comb begin
    mem_read = 0;
    mem_write = 0;
    mem_address = busy ? load_address : address;
    mem_destination = destination + index[14:0];
    mem_value = 0;
    case (state)
      IDLE: begin
        mem_read = read;
        mem_write = write;
        mem_destination = address;
        mem_value = wdata;
      end
      LOAD_READ: begin
        mem_read = 1;
      end
      RUN_WAIT: if (job_unit == GCN) begin
        mem_write = gcn_valid;
        mem_destination = destination + emitted[14:0];
        mem_value = gcn_data;
      end
      STORE: begin
        mem_write = 1;
        case (job_unit)
          NORM: mem_value = norm_data;
          SOFTMAX: mem_value = softmax_data;
          GPT_VECTOR: mem_value = gpt_data;
          default: begin end
        endcase
      end
      LEFT_READ: if (FAMILY < 0 || FAMILY == int'(VECTOR)) begin
        mem_read = 1;
        mem_address = sources[0] + index[14:0];
      end
      RIGHT_READ: if (FAMILY < 0 || FAMILY == int'(VECTOR)) begin
        mem_read = 1;
        mem_address = sources[1] + index[14:0];
      end
      DRAIN: begin
        mem_write = vector_valid;
        mem_value = vector_data;
      end
      default: begin end
    endcase
  end

  // One synchronous read port and one write port, with read-first collisions.
`ifdef ASIC_SRAM
  AsicCoreMemory #(.WORDS(WORDS)) memory (
    .clk(clk), .re(mem_read), .we(mem_write),
    .raddr(mem_address[ADDR_BITS-1:0]), .waddr(mem_destination[ADDR_BITS-1:0]),
    .rdata(mem_data), .wdata(mem_value)
  );
`else
  logic [31:0] memory [WORDS];
  always_ff @(posedge clk) begin
    if (mem_read) mem_data <= memory[mem_address[ADDR_BITS-1:0]];
    if (mem_write) memory[mem_destination[ADDR_BITS-1:0]] <= mem_value;
  end
`endif
  always_ff @(posedge clk) rvalid <= read && state == IDLE && !reset;

  always_ff @(posedge clk) begin
    done <= 0;
    if (reset) begin
      state <= IDLE;
      busy <= 0;
      job_unit <= 0;
      job_operation <= 0;
      job_fused <= 0;
      job_phase <= 0;
      job_count <= 0;
      job_rows <= 0;
      job_edges <= 0;
      job_divisor <= 1;
      for (integer i = 0; i < 4; i = i + 1) sources[i] <= 0;
      destination <= 0;
      load_bank <= 0;
      index <= 0;
      emitted <= 0;
      left <= 0;
      right <= 0;
    end else begin
      case (state)
        IDLE: if (start) begin
          busy <= 1;
          job_unit <= selected_unit;
          job_operation <= operation;
          job_fused <= fused;
          job_phase <= phase;
          job_count <= count;
          job_rows <= rows;
          job_edges <= edges;
          job_divisor <= divisor;
          load_address <= src0;
          sources[0] <= src0;
          sources[1] <= src1;
          sources[2] <= src2;
          sources[3] <= src3;
          destination <= dst;
          load_bank <= 0;
          index <= 0;
          emitted <= 0;
          state <= selected_unit == VECTOR ? RUN_START : LOAD_READ;
        end
        LOAD_READ: state <= LOAD_WRITE;
        LOAD_WRITE: begin
          if (index + 1'b1 == load_count) begin
            index <= 0;
            if (load_bank == last_bank) state <= RUN_START;
            else begin
              load_bank <= load_bank + 1'b1;
              load_address <= sources[load_bank + 2'd1];
              state <= LOAD_READ;
            end
          end else begin
            index <= index + 1'b1;
            load_address <= load_address + 1'b1;
            state <= LOAD_READ;
          end
        end
        RUN_START: state <= job_unit == VECTOR ? LEFT_READ : RUN_WAIT;
        RUN_WAIT: begin
          if (job_unit == GCN && gcn_valid) emitted <= emitted + 1'b1;
          if (native_done[job_unit]) begin
            index <= 0;
            state <= job_unit == GCN || output_count == 0 ? COMPLETE : STORE;
          end
        end
        STORE: begin
          index <= index + 1'b1;
          if (index + 1'b1 == output_count) state <= COMPLETE;
        end
        LEFT_READ: state <= LEFT_CAPTURE;
        LEFT_CAPTURE: begin
          left <= mem_data;
          right <= 0;
          state <= job_operation == 0 ? FEED : RIGHT_READ;
        end
        RIGHT_READ: state <= RIGHT_CAPTURE;
        RIGHT_CAPTURE: begin
          right <= mem_data;
          state <= FEED;
        end
        FEED: if (vector_ready) state <= DRAIN;
        DRAIN: if (vector_valid) begin
          index <= index + 1'b1;
          state <= index + 1'b1 == job_count ? COMPLETE : LEFT_READ;
        end
        COMPLETE: begin
          busy <= 0;
          done <= 1;
          state <= IDLE;
        end
        default: begin
          state <= IDLE;
          busy <= 0;
        end
      endcase
    end
  end
endmodule
