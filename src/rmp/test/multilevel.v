/*
 Cut which could cross multiple levels of state
 (cut from op requires logic which drives flop and also logic
 driven by flop).
 */

module multilevel(ip0,ip1,ip2, clk, op);
input ip0;
input ip1;
input ip2;
input clk;
output op;

   wire int1;
   wire int2;
   wire flop_op;
   OR2M0HM nr1 (.A(ip0),.B(ip1),.Z(int1));
   MUX2M0HM mux1 (.A(int1), .B(flop_op), .S(ip2), .Z(int2));
   DFM2HM flop(.CK(clk),.D(int2),.Q(flop_op) );
   OR2M0HM nr2 (.A(int1),.B(flop_op),.Z(op));
endmodule // multilevel
