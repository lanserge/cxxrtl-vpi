// SPDX-License-Identifier: ISC
// Exercises a wide (40-bit) bus through a submodule instance, to test
// hierarchical signal access and multi-chunk values via cocotb + cxxrtl-vpi.
module inner #(parameter W = 40) (
    input  wire         clk,
    input  wire [W-1:0] din,
    output reg  [W-1:0] dout
);
    always @(posedge clk) dout <= din;
endmodule

module top (
    input  wire        clk,
    input  wire [39:0] data_in,
    output wire [39:0] data_out
);
    inner #(.W(40)) u_inner (.clk(clk), .din(data_in), .dout(data_out));
endmodule
