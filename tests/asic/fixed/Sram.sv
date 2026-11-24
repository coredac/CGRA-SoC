// Functional model of the bound one-read/one-write ports. No timing delays.
// CSB is sampled on the preceding falling edge; read/write data on rising edges.
// Colliding reads deliberately return poison; the wrapper must forward.
module SRAM2RW128x32 (
  input [6:0] A1, A2,
  input CE1, CE2, WEB1, WEB2, OEB1, OEB2, CSB1, CSB2,
  input [31:0] I1, I2,
  output logic [31:0] O1,
  output [31:0] O2
);
  logic [31:0] words [128];
  logic selected1, selected2;
  always @(negedge CE1) selected1 <= !CSB1;
  always @(negedge CE2) selected2 <= !CSB2;
  assign O2 = 32'd0;
  always @(posedge CE1) begin
    if (selected1 && WEB1 && !OEB1)
      O1 <= selected2 && !WEB2 && A1 == A2 ? 32'hdeadbeef : words[A1];
  end
  always @(posedge CE2) begin
    if (selected2 && !WEB2)
      words[A2] <= I2;
  end
endmodule
