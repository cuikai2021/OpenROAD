module asynch_path(clk_i,
             clk_inv_i,
             latch_o,
             d_i);
   input clk_i;
   output latch_o;
   input  d_i;
   input  clk_inv_i;
   
   wire   async_int;
   wire   latch_second_stage_int;
   

   MUX2M1HM U1(.A(latch_o),
              .B(d_i),
              .S(clk_i),
              .Z(async_int));
   
   INVM0HM U2 (.A(async_int),
              .Z(latch_second_stage_int));

   MUX2M1HM U3(.A(latch_o),
              .B(latch_second_stage_int),
              .S(clk_inv_i),
              .Z(latch_o));
   
endmodule // async_path

