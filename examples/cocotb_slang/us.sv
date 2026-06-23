// SPDX-License-Identifier: ISC
// An UNPACKED struct — Yosys's native frontend rejects this ("Only PACKED
// supported"), but the yosys-slang frontend (frontend="slang") handles it.
typedef struct {
    logic [7:0] a;
    logic [7:0] b;
} pair_t;

module us (
    input  wire        clk,
    input  wire [7:0]  x,
    output wire [7:0]  sum
);
    pair_t p;
    always @(posedge clk) begin
        p.a <= x;
        p.b <= x + 8'd1;
    end
    assign sum = p.a + p.b;
endmodule
