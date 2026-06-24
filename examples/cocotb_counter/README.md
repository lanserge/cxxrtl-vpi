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
PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
```

where the Python has `cocotb>=2.0` installed (and `yosys` + a C++ compiler are on
PATH). This builds the model + harness against cocotb's runtime and runs the
test — it **passes** (`TESTS=1 PASS=1 FAIL=0`). Set `CXXRTL_VPI_DEBUG=1` to trace
the VPI scheduler.

The `Makefile` shows the lower-level build steps; `run_cocotb.sh` is the
complete, working path.

To dump a waveform, set `CXXRTL_VPI_VCD`:

```sh
CXXRTL_VPI_VCD=dump.vcd PYTHON=/path/to/venv/bin/python bash run_cocotb.sh
```
