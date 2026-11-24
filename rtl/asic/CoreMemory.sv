// Synchronous read-first SRAM with one-cycle delayed writes and forwarding.
module AsicCoreBank (
  input wire clk,
  input wire re,
  input wire we,
  input wire [6:0] waddr,
  input wire [6:0] raddr,
  input wire [31:0] wdata,
  output wire [31:0] rdata
);
  reg pending_valid;
  reg [6:0] pending_addr;
  reg [31:0] pending_data;
  reg forward;
  reg [31:0] forward_data;
  wire [31:0] R0_data;
  wire [6:0] R0_addr = raddr;
  wire R0_clk = clk;
  wire R0_en = re;
  wire [6:0] W0_addr = pending_addr;
  wire W0_clk = clk;
  wire [31:0] W0_data = pending_data;
  wire W0_en = pending_valid;
  always @(posedge clk) begin
    pending_valid <= we;
    if (we) begin
      pending_addr <= waddr;
      pending_data <= wdata;
    end
    if (re) begin
      forward <= pending_valid & (raddr == pending_addr);
      forward_data <= pending_data;
    end
  end
  assign rdata = forward ? forward_data : R0_data;
  wire [31:0] sram_0_0_0_data;
  SRAM2RW128x32 sram_0_0_0 (
    .A1(R0_addr[6:0]),
    .A2(W0_addr[6:0]),
    .CE1(R0_clk), .CE2(W0_clk),
    .WEB1(1'b1), .WEB2(1'b0),
    .OEB1(1'b0), .OEB2(1'b1),
    .CSB1(~(R0_en)), .CSB2(~(W0_en)),
    .I1(32'b0), .I2(W0_data[31:0]),
    .O1(sram_0_0_0_data), .O2()
  );
  assign R0_data[31:0] = sram_0_0_0_data[31:0];
endmodule

module AsicCoreMemory #(
  parameter integer WORDS = 32768
) (
  input wire clk, re, we,
  input wire [$clog2(WORDS)-1:0] waddr, raddr,
  input wire [31:0] wdata,
  output wire [31:0] rdata
);
  localparam integer ADDR_BITS = $clog2(WORDS);
  localparam integer BANKS = WORDS / 128;
  wire [31:0] values [BANKS];
  reg [ADDR_BITS-8:0] selected;
  for (genvar bank = 0; bank < BANKS; bank = bank + 1) begin : banks
    AsicCoreBank memory (
      .clk(clk), .re(re && raddr[ADDR_BITS-1:7] == (ADDR_BITS-7)'(bank)),
      .we(we && waddr[ADDR_BITS-1:7] == (ADDR_BITS-7)'(bank)),
      .waddr(waddr[6:0]), .wdata(wdata), .raddr(raddr[6:0]), .rdata(values[bank])
    );
  end
  always @(posedge clk) if (re) selected <= raddr[ADDR_BITS-1:7];
  assign rdata = values[selected];
endmodule
