# cxxrtl-vpi

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

> Status: **scaffold / work in progress.** The architecture, build, and the full
> API surface are laid out; the VPI entry points are stubs that map onto
> `cxxrtl_capi` (see `docs/vpi-coverage.md`). It does not run testbenches yet.

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

## Building (once implemented)

```sh
cmake -S . -B build && cmake --build build
```

Requires Yosys (for `yosys` / `yosys-config`) and a C++14 compiler.

## License

ISC — the same license as Yosys/CXXRTL. This keeps the VPI core trivially
upstreamable into the Yosys tree (matching file headers, no relicensing), and is
permissively compatible with cocotb (BSD-3-Clause) for the consumer glue.
