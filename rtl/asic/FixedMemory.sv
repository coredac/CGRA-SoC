module AsicFixedMemory #(
  parameter integer WORDS = 16384
) (
  input logic clk, reset, read, write,
  input logic [$clog2(WORDS)-1:0] raddr, waddr,
  input logic [31:0] wdata,
  output logic [31:0] rdata,
  output logic rvalid, write_pending
);
  localparam integer ADDR_BITS = $clog2(WORDS);
  localparam integer BANKS = WORDS / 128;
  localparam integer GROUPS = BANKS / 8;
  logic r0, r1, r2, r3, w0, w1, w2;
  logic [ADDR_BITS-1:0] read_addr, write_addr0, write_addr1;
  logic [31:0] write_data0, write_data1;
  logic [BANKS-1:0] ren, wen, selected;
  logic [6:0] read_row [BANKS], write_row [BANKS];
  logic [31:0] write_data [BANKS], bank_data [BANKS];
  logic [31:0] group_data [GROUPS], merged;

  assign write_pending = w0 || w1 || w2;
  always_comb begin
    merged = 0;
    for (integer group = 0; group < GROUPS; group++)
      merged |= group_data[group];
  end

  always_ff @(posedge clk) begin
    r0 <= read;
    r1 <= r0;
    r2 <= r1;
    r3 <= r2;
    w0 <= write;
    w1 <= w0;
    w2 <= w1;
    rvalid <= r3;
    if (read) read_addr <= raddr;
    if (write) begin
      write_addr0 <= waddr;
      write_data0 <= wdata;
    end
    if (w0) begin
      write_addr1 <= write_addr0;
      write_data1 <= write_data0;
    end
    if (r3) rdata <= merged;
    if (reset) begin
      r0 <= 0;
      r1 <= 0;
      r2 <= 0;
      r3 <= 0;
      w0 <= 0;
      w1 <= 0;
      w2 <= 0;
      rvalid <= 0;
    end
  end

  for (genvar bank = 0; bank < BANKS; bank++) begin : banks
    always_ff @(posedge clk) begin
      ren[bank] <= r0 && read_addr[ADDR_BITS-1:7] == (ADDR_BITS-7)'(bank);
      wen[bank] <= w1 && write_addr1[ADDR_BITS-1:7] == (ADDR_BITS-7)'(bank);
      selected[bank] <= ren[bank];
      if (r0 && read_addr[ADDR_BITS-1:7] == (ADDR_BITS-7)'(bank))
        read_row[bank] <= read_addr[6:0];
      if (w1 && write_addr1[ADDR_BITS-1:7] == (ADDR_BITS-7)'(bank)) begin
        write_row[bank] <= write_addr1[6:0];
        write_data[bank] <= write_data1;
      end
      if (reset) begin
        ren[bank] <= 0;
        wen[bank] <= 0;
        selected[bank] <= 0;
      end
    end
`ifdef ASIC_SRAM
    logic [31:0] unused;
    SRAM2RW128x32 memory (
      .A1(read_row[bank]), .A2(write_row[bank]),
      .CE1(clk), .CE2(clk), .WEB1(1'b1), .WEB2(1'b0),
      .OEB1(1'b0), .OEB2(1'b1), .CSB1(!ren[bank]), .CSB2(!wen[bank]),
      .I1(32'd0), .I2(write_data[bank]), .O1(bank_data[bank]), .O2(unused)
    );
`else
    logic [31:0] words [128];
    always_ff @(posedge clk) begin
      if (ren[bank]) bank_data[bank] <= words[read_row[bank]];
      if (wen[bank]) words[write_row[bank]] <= write_data[bank];
    end
`endif
  end

  for (genvar group = 0; group < GROUPS; group++) begin : groups
    logic [31:0] selected_data;
    always_comb begin
      selected_data = 0;
      for (integer bank = 0; bank < 8; bank++)
        selected_data |= {32{selected[group*8+bank]}} & bank_data[group*8+bank];
    end
    always_ff @(posedge clk) group_data[group] <= selected_data;
  end
endmodule
