module top (in1, in2, mode0, mode1, clk, out);
  input in1, in2, mode0, mode1, clk;
  output out;
  wire r1q, r2q, u1z, u2z;
  wire b1z, b2z, b3z, b4z, b5z, b6z, b1xz;
  wire sb2z, sb3z, sb4z, sb5z, sb6z;
  wire fb2z, fb3z, fb4z, fb5z;
  wire m1z;

  INV_X1 c0x (.A(clk), .ZN(clkx));
  TBUF_X1 c1 (.A(clk), .Z(ck1));
  TBUF_X1 c2 (.A(ck1), .Z(ck2));
  TBUF_X1 c3 (.A(ck2), .Z(ck3));
  TBUF_X1 c4 (.A(ck3), .Z(ck4));
  TBUF_X1 c5 (.A(ck4), .Z(ck5));
  TBUF_X1 c6 (.A(ck5), .Z(ck6));
  TBUF_X1 c7 (.A(ck6), .Z(ck7));
  TBUF_X1 c8 (.A(ck7), .Z(ck8));
  TBUF_X1 c9 (.A(ck8), .Z(ck9));
  TBUF_X1 c10 (.A(ck9), .Z(ck10));
  TBUF_X1 c11 (.A(ck10), .Z(ck11));
  INV_X1 c11x (.A(ck11), .ZN(ck11x));

  // SDFQD1 r1 (.CP(clk), .D(in1), .SI(in1), .SE(mode), .Q(r1q));

  // AN2D1 u1 (.A1(r1q), .A2(in2), .Z(u1z));

  SDFFRS_X1 r1 (.CK(ck1), .D(in1), .SI(in1), .SE(mode0), .Q(r1q));
  SDFFRS_X1 r2 (.CK(clkx), .D(u1z), .SI(in1), .SE(mode0), .Q(r2q));

  // logic r1 => r2

  TBUF_X1 b1 (.A(r1q), .Z(b1z));
  TBUF_X1 b1x (.A(b1z), .Z(b1xz));

  TBUF_X1 sb1 (.A(b1z), .Z(sb2z));
  TBUF_X1 sb2 (.A(sb2z), .Z(sb3z));
  TBUF_X1 sb3 (.A(sb3z), .Z(sb4z));
  TBUF_X1 sb4 (.A(sb4z), .Z(sb5z));
  TBUF_X1 sb5 (.A(sb5z), .Z(sb6z));

  TBUF_X1 b2 (.A(r2q), .Z(b2z));
  TBUF_X1 b3 (.A(b2z), .Z(b3z));
  TBUF_X1 b4 (.A(b3z), .Z(b4z));
  TBUF_X1 b5 (.A(b4z), .Z(b5z));
  TBUF_X1 b6 (.A(b5z), .Z(b6z));

  AND2_X2 u1 (.A1(b1xz), .A2(b6z), .ZN(u1z));
  AND2_X2 u2 (.A1(b6z), .A2(sb6z), .ZN(u2z));

  SDFFRS_X1 r3 (.CK(ck11x), .D(b2z), .SI(in1), .SE(mode0), .Q(r3q));
  SDFFRS_X1 r4 (.CK(ck11), .D(u2z), .SI(in1), .SE(mode0), .Q(out));

endmodule // top
