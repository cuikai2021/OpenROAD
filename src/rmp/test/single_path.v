module top (clk,si,se,e,d,op);
 input clk;
   input si,se,e,d;
   output op;
   
 wire inv1in;
 wire inv2in;
 wire inv3in;
 wire inv4in;
 wire inv5in;
 wire inv6in;
 wire inv7in;
 wire inv8in;
 wire reg2d;

 SEDFD4 reg1 (.Q(inv1in),.SI(si),.SE(se),.E(e),.D(d),
    .CP(clk));
 SEDFXD0 reg2 (.D(reg2d),.SI(si), .SE(se),.E(e),
    .CP(clk),.Q(op));
 CKND2 inv1 (.ZN(inv2in),
    .I(inv1in));
 INVD2 inv2 (.ZN(inv3in),
    .I(inv2in));
 CKND2 inv3 (.ZN(inv4in),
    .I(inv3in));
 INVD2 inv4 (.ZN(inv5in),
    .I(inv4in));
 CKND2 inv5 (.ZN(inv6in),
    .I(inv5in));
 INVD2 inv6 (.ZN(inv7in),
    .I(inv6in));
 CKND2 inv7 (.ZN(inv8in),
    .I(inv7in));
 INVD2 inv8 (.ZN(reg2d),
    .I(inv8in));
endmodule
