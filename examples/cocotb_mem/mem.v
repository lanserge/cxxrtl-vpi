// SPDX-License-Identifier: ISC
// A small RAM, for testing cocotb access to a CXXRTL memory (dut.store[i]).
module mem (
    input  wire       clk,
    input  wire       we,
    input  wire [3:0] addr,
    input  wire [7:0] wdata,
    output reg  [7:0] rdata
);
    reg [7:0] store [0:15];
    always @(posedge clk) begin
        if (we) store[addr] <= wdata;
        rdata <= store[addr];
    end
endmodule
