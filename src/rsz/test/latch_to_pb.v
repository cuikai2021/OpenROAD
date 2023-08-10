module top (in1, clk, out);
  input in1, in2, clk;
  output out;
  wire r2clk;

  CLKGATETST_X4 r1 (.CK(clk), .E(in1), .SE(in1), .GCK(r2clk));

  DFF_X1 r2 (.D(in1), .CK(r2clk), .Q(out));
endmodule // top
