module floploop (clk_i,
    ip0,
    ip1,
    ip2,
    ip3,
    op);
 input clk_i;
 input ip0;
 input ip1;
 input ip2;
 input ip3;
 output op;

 wire mux2_1_int;
 wire q_int;
 wire xor2_1_int;
 wire xor2_2_int;
 wire MUX2_X1_1_4;
 wire XNOR2_X1_2_5;

 DFF_X1 U4 (.D(xor2_2_int),
    .CK(clk_i),
    .Q(q_int));
 XOR2_X1 U5 (.A(q_int),
    .B(ip2),
    .Z(op));
 MUX2_X1 MUX2_X1_1 (.A(q_int),
    .B(ip0),
    .S(ip3),
    .Z(MUX2_X1_1_4));
 XNOR2_X1 XNOR2_X1_2 (.A(ip0),
    .B(ip1),
    .ZN(XNOR2_X1_2_5));
 XNOR2_X1 XNOR2_X1_3 (.A(MUX2_X1_1_4),
    .B(XNOR2_X1_2_5),
    .ZN(xor2_2_int));
endmodule
