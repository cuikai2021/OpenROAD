module escape (ip0,
    ip1,
    ip2,
    ip3,
    ip4,
    op0,
    op1);
 input ip0;
 input ip1;
 input ip2;
 input ip3;
 input ip4;
 output op0;
 output op1;

 wire int1;
 wire int2;
 wire op2;

 NOR2_X1 U3 (.A1(int2),
    .A2(ip3),
    .ZN(op2));
 NOR2_X1 U4 (.A1(ip4),
    .A2(int1),
    .ZN(op1));
 NOR2_X1 NOR2_X1_1 (.A1(ip0),
    .A2(ip1),
    .ZN(int1));
 NOR2_X1 NOR2_X1_2 (.A1(int1),
    .A2(ip2),
    .ZN(int2));
endmodule
