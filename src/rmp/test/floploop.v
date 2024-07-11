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

   wire   q_int;
   wire   mux2_1_int;
   wire   xor2_1_int;
   wire   xor2_2_int;
   wire   xor2_3_int;

   
   XOR2_X1 U1  (.A(ip0),
                .B(ip1),
                .Z(xor2_1_int));
   
   MUX2_X1 U2  (.A(q_int),
                .B(ip0),
                .S(ip3),
                .Z(mux2_1_int));

   XOR2_X1 U3 (.A(mux2_1_int),
               .B(xor2_1_int),
               .Z(xor2_2_int));
   
   DFF_X1 U4 (.D(xor2_2_int),
              .CK(clk_i),
              .Q(q_int));

   XOR2_X1 U5 (.A(q_int),
               .B(ip2),
               .Z(op));

endmodule // floploop

                 
