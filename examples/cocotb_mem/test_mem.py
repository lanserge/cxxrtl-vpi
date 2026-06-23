# SPDX-License-Identifier: ISC
"""cocotb test: index a CXXRTL memory (dut.store[i]) for read and direct write."""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge


@cocotb.test()
async def test_mem(dut):
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    dut.we.value = 0
    # write via the design's write port
    for a, d in [(3, 0xAA), (7, 0x55), (15, 0xC3)]:
        dut.we.value = 1; dut.addr.value = a; dut.wdata.value = d
        await RisingEdge(dut.clk)
    dut.we.value = 0
    await RisingEdge(dut.clk)
    # read back the backing memory array directly via hierarchical index
    for a, d in [(3, 0xAA), (7, 0x55), (15, 0xC3)]:
        got = int(dut.store[a].value)
        assert got == d, f"store[{a}]: got {got:#x} expected {d:#x}"
    # poke an element directly and read it back
    dut.store[5].value = 0x99
    await RisingEdge(dut.clk)
    assert int(dut.store[5].value) == 0x99, "direct memory write failed"
    dut._log.info("OK: memory read-back + direct element write")
