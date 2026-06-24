// SPDX-License-Identifier: ISC
// A generate-for loop creates an array of scopes (lane[0..3], each with a reg).
// cocotb reaches them as dut.lane[i].r — exercising generate-scope array support.
module genmod (input clk, input [7:0] din, output [7:0] dout);
    genvar i;
    wire [7:0] stage [0:3];
    generate
      for (i = 0; i < 4; i = i + 1) begin : lane
        reg [7:0] r;
        always @(posedge clk) r <= din + i[7:0];
        assign stage[i] = r;
      end
    endgenerate
    assign dout = stage[0] ^ stage[1] ^ stage[2] ^ stage[3];
endmodule
