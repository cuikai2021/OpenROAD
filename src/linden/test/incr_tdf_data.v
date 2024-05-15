module top (in1, in2, mode, clk, out);
  input in1, in2, mode, clk;
  output out;
  wire r1q, r2q, u1z, u2z;
  wire b1z, b2z, b3z;
  wire ck1, ck2, ck3, ck4, ck5, ck6, ck7, ck8, ck9;


  sky130_fd_sc_hd__sdlclkp_1 cg1 (.CLK(clk), .GATE(in1), .M0(in2), .SCE(mode), .GCLK(cg1gclk));
  sky130_fd_sc_hd__sdfxtp_1 r2 (.CLK(cg1gclk), .D(in1), .SCD(in1), .SCE(mode), .Q(r2q));

  // logic r1 => r2
  sky130_fd_sc_hd__dlymetal6s6s_1 b1 (.A(r2q), .X(b1z));
  sky130_fd_sc_hd__and2_2 u2 (.A(b1z), .B(b6z), .X(u2z));
  sky130_fd_sc_hd__buf_1 b2 (.A(r2q), .X(b2z));
  sky130_fd_sc_hd__buf_1 b3 (.A(b2z), .X(b3z));
  sky130_fd_sc_hd__dlymetal6s6s_1 b4 (.A(b3z), .X(b4z));
  sky130_fd_sc_hd__buf_1 b5 (.A(b4z), .X(b5z));
  sky130_fd_sc_hd__buf_1 b6 (.A(b5z), .X(b6z));

  sky130_fd_sc_hd__sdfxtp_1 r3 (.CLK(clk), .D(u2z), .SCD(r2q), .SCE(mode), .Q(out));

endmodule // top
