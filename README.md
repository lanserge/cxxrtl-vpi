# cxxrtl-vpi

[![PyPI](https://img.shields.io/pypi/v/cxxrtl-vpi)](https://pypi.org/project/cxxrtl-vpi/)
[![License: ISC](https://img.shields.io/badge/license-ISC-blue.svg)](LICENSE)

**An IEEE-1364 VPI interface for [CXXRTL](https://github.com/YosysHQ/yosys), the Yosys C++ simulation backend.**

CXXRTL compiles a (System)Verilog design into a fast, embeddable C++ model, but
exposes only its own `cxxrtl_capi` C API — it does not speak VPI. This project
implements a VPI provider *on top of* `cxxrtl_capi`, turning any CXXRTL-generated
model into a VPI-capable simulator.

The immediate motivation is **cocotb** (Python testbenches) — cocotb drives any
VPI/VHPI/FLI simulator, but ships no CXXRTL backend. With `cxxrtl-vpi`, cocotb's
existing generic VPI consumer can drive a CXXRTL model unmodified. But the VPI
layer is deliberately **client-agnostic**: any VPI tool (debuggers, custom
harnesses) can use it.

> Status: **working.** Unmodified cocotb testbenches run against CXXRTL models
> and pass. Measured against cocotb 2.0's own core regression suite, the
> scheduler/timing/callback core is at **Verilator parity** (~91% of the runnable
> core suite; the rest is the inherent CXXRTL-is-a-synthesizer boundary —
> `real`/`string`, folded parameters), and **ahead** on force/release (which
> cocotb does not support on Verilator). Implemented over `cxxrtl_capi`: object
> access, callbacks/time/control, hierarchy + parent-nav, memories, wide signals,
> net/reg classification, ranges, picosecond time, init-fuzzing, VCD tracing, and
> an optional yosys-slang frontend for full SystemVerilog. See
> `docs/vpi-coverage.md` for the coverage matrix and limits.

## Why this is an engine adapter, not a cocotb plugin

This mirrors how **Verilator** integrates with cocotb:

| Piece | Owner for Verilator | Intended owner here |
|---|---|---|
| The **VPI implementation** | Verilator itself (`verilated_vpi`) | **this repo → upstream to Yosys/CXXRTL** |
| VPI consumer bootstrap (`libcocotbvpi_*`) | cocotb | cocotb (thin glue) |
| Harness `main()` | cocotb (`verilator.cpp`) | cocotb (thin glue) — prototyped here under `examples/` |

The heavy, reusable piece — making CXXRTL speak VPI — is a **simulator
capability**, so its natural home is the engine (Yosys), exactly as Verilator
owns its own VPI. This repo is the **incubator**: it depends only on two stable,
public contracts and is unblocked regardless of either project's review cycle:

- `cxxrtl/capi/cxxrtl_capi.h` (Yosys — stable, external-facing)
- `vpi_user.h` (IEEE 1364 — standard)

See [`docs/design.md`](docs/design.md) for the full rationale and upstreaming plan.

## Layout

```
include/cxxrtl_vpi/   public headers for the model wrapper + VPI provider
src/
  model.cc            thin wrapper over cxxrtl_capi (create/eval/step/signals)
  vpi_provider.cc     IEEE-1364 VPI entry points implemented over cxxrtl_capi
  harness.cc          main(): build model, run VPI startup, drive the sim loop
examples/cocotb_counter/   end-to-end cocotb example (the first consumer)
cxxrtl_vpi/           small Python helper: locate includes, build, run
docs/                 design rationale + VPI coverage matrix
```

## Install

```sh
pip install cxxrtl-vpi
```

The cocotb path requires **Python ≤ 3.13** (cocotb's cap), plus **Yosys**
(`yosys` / `yosys-config`) and a **C++14 compiler** on `PATH` — the same external
toolchain every simulator needs. (Latest from git:
`pip install git+https://github.com/lanserge/cxxrtl-vpi`.)
The quickest way to see it work is the cocotb example:

```sh
PYTHON=/path/to/venv/bin/python bash examples/cocotb_counter/run_cocotb.sh
# -> test_count_up passed ... TESTS=1 PASS=1 FAIL=0
```

The provider library alone also builds standalone with CMake:

```sh
cmake -S . -B build && cmake --build build
```

## License

ISC — the same license as Yosys/CXXRTL. This keeps the VPI core trivially
upstreamable into the Yosys tree (matching file headers, no relicensing), and is
permissively compatible with cocotb (BSD-3-Clause) for the consumer glue.
