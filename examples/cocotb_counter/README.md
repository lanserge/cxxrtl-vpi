# cocotb_counter

The bring-up example for `cxxrtl-vpi`: an ordinary cocotb testbench
(`test_counter.py`) driving a CXXRTL-generated model of `counter.v`.

This directory is also the **prototype of the cocotb-side glue** — the
`Makefile` is the analogue of cocotb's `share/lib/verilator/verilator.cpp` build
step. When the glue is upstreamed, this becomes (almost verbatim) what cocotb's
runner does for a `"cxxrtl"` target, and what the SiliconCompiler
`cxxrtl-cocotb` flow would invoke.

## Pipeline

```
counter.v
   │  yosys: read_verilog; write_cxxrtl
   ▼
counter_cxxrtl.cc        (extern "C" cxxrtl_design_create())
   │  c++ : model + ../../src/harness.cc + libcxxrtl_vpi + cocotb VPI consumer
   ▼
sim_counter              (a VPI-speaking executable)
   │  cocotb env: COCOTB_TOPLEVEL=counter, COCOTB_TEST_MODULES=test_counter
   ▼
cocotb runs test_counter.py  →  results.xml
```

## Run

```sh
make run
```

> STATUS: scaffold. `make model` and `make build` show the real pipeline; `run`
> is a placeholder until the VPI provider implements the MVP surface in
> `../../docs/vpi-coverage.md`.
