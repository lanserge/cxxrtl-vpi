# SPDX-License-Identifier: ISC
"""cocotb test: access a generate-scope array, dut.lane[i].r, on CXXRTL."""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge


@cocotb.test()
async def test_gen(dut):
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    dut.din.value = 100
    await RisingEdge(dut.clk)
    await RisingEdge(dut.clk)

    # dut.lane is an array of generated scopes; lane[i].r holds din + i.
    assert len(dut.lane) == 4
    for i in range(4):
        r = int(dut.lane[i].r.value)
        assert r == (100 + i) & 0xFF, f"lane[{i}].r = {r}, expected {100 + i}"

    dut._log.info("OK: generate-scope array dut.lane[i].r works on CXXRTL")
