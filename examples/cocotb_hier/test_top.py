# SPDX-License-Identifier: ISC
"""cocotb test: wide (40-bit) bus + hierarchical access through a submodule."""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge


@cocotb.test()
async def test_wide_hier(dut):
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())

    val = 0xABCDEF1234           # 40-bit value (spans two 32-bit chunks)
    dut.data_in.value = val
    await RisingEdge(dut.clk)
    await RisingEdge(dut.clk)

    out = int(dut.data_out.value)
    assert out == val, f"data_out: got {out:#x} expected {val:#x}"

    # hierarchical access to a submodule signal: dut.<instance>.<signal>
    inner = int(dut.u_inner.dout.value)
    assert inner == val, f"u_inner.dout: got {inner:#x} expected {val:#x}"

    dut._log.info(f"OK: data_out={out:#x} u_inner.dout={inner:#x}")
