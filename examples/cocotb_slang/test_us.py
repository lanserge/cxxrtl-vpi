# SPDX-License-Identifier: ISC
"""cocotb test for an unpacked-struct design built via the yosys-slang frontend."""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge


@cocotb.test()
async def test_us(dut):
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    dut.x.value = 10
    await RisingEdge(dut.clk)
    await RisingEdge(dut.clk)
    got = int(dut.sum.value)
    assert got == 21, f"got {got}"   # p.a (=10) + p.b (=11)
    dut._log.info(f"OK: unpacked-struct design via slang frontend, sum={got}")
