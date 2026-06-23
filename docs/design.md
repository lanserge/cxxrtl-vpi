# Design & upstreaming plan

## Problem

cocotb (and other VPI tools) can drive any simulator that exposes IEEE-1364 VPI.
CXXRTL exposes only `cxxrtl_capi` — a small, stable C API of its own
(`cxxrtl_create`, `cxxrtl_step`, `cxxrtl_get`, `cxxrtl_enum`, …). There is no
VPI, VHPI, or FLI surface, and cocotb ships **no** CXXRTL backend. So Python
testbenches cannot target the CXXRTL engine today.

## Two ways to bridge — and why we chose VPI

| Approach | What you implement | Depends on | Lands in |
|---|---|---|---|
| **A. cocotb GpiImpl** | cocotb's private C++ `GpiImpl` interface | `gpi_priv.h` (NOT shipped in the wheel) + cocotb's internal, unstable ABI | cocotb, in-tree only |
| **B. VPI-over-capi** (chosen) | IEEE-1364 VPI on top of `cxxrtl_capi` | `vpi_user.h` (IEEE standard) + `cxxrtl_capi.h` (stable, public) | engine adapter → Yosys; thin glue → cocotb |

Approach **B** wins for a standalone project:

- It depends only on **two stable public contracts**, never on cocotb internals.
- The result is **reusable by any VPI client**, not just cocotb.
- It matches the **Verilator precedent**: Verilator implements its own VPI
  (`verilated_vpi`); cocotb contains only a thin consumer + a `main()`. So the
  VPI implementation is a *simulator* concern, and CXXRTL's analogue belongs
  with CXXRTL (Yosys) — not cocotb.

## Architecture

```
            ┌─────────────────────────┐
 Python  →  │ cocotb (unmodified)     │   cocotb's generic VPI consumer
            │  libcocotbvpi + libgpi  │   (vlog_startup_routines)
            └───────────┬─────────────┘
                        │  IEEE-1364 VPI calls (vpi_user.h)
            ┌───────────▼─────────────┐
            │ cxxrtl-vpi (THIS REPO)  │   VPI provider
            │  src/vpi_provider.cc    │   maps VPI → cxxrtl_capi
            │  src/model.cc           │   wraps a cxxrtl_handle
            │  src/harness.cc         │   main(): create, startup, sim loop
            └───────────┬─────────────┘
                        │  cxxrtl_capi (cxxrtl_capi.h)
            ┌───────────▼─────────────┐
            │ generated model .cc     │   write_cxxrtl  →  cxxrtl_design_create()
            │  (cxxrtl_design::p_top) │
            └─────────────────────────┘
```

`write_cxxrtl` always emits:

```c
extern "C" cxxrtl_toplevel cxxrtl_design_create();
```

so the harness is concrete:

```c
cxxrtl_handle h = cxxrtl_create(cxxrtl_design_create());
// ... discover signals via cxxrtl_enum / cxxrtl_get
// ... loop: service VPI callbacks, cxxrtl_step(h)
```

## VPI → cxxrtl_capi mapping (summary; full matrix in vpi-coverage.md)

| VPI need | cxxrtl_capi |
|---|---|
| `vpi_handle_by_name` | `cxxrtl_get(handle, name)` → `cxxrtl_object*` |
| `vpi_get_value` | read `object->curr`, `object->width` |
| `vpi_put_value` | write `object->next`, then `cxxrtl_commit`/`cxxrtl_eval` |
| `vpi_iterate` / `vpi_scan` | `cxxrtl_enum` (build name/hierarchy table once) |
| advance time / `cbNextSimTime` | `cxxrtl_step(handle)` driven by the harness clock model |
| `cbValueChange` | snapshot + compare `curr` after each settle |
| `vpi_control(vpiFinish)` | set stop flag; harness exits its loop |

## Upstreaming plan

1. **Incubate here** (standalone), building only against `cxxrtl_capi.h` +
   `vpi_user.h`. Prove it end-to-end with the cocotb example.
2. **VPI core → Yosys.** Open an RFC: "a `cxxrtl_capi`-based VPI provider for
   CXXRTL". Precedent for an in-tree capi consumer exists (`cxxrtl_capi_vcd`).
   If maintainers prefer it stays external (CXXRTL favours a minimal core),
   this repo simply remains the engine adapter — still a valid home.
3. **Thin glue → cocotb.** A `libcocotbvpi_cxxrtl` registration + a harness
   `main()` (the cocotb analogue of `share/lib/verilator/verilator.cpp`), so
   `cocotb`'s runner gains a `"cxxrtl"` target. Then the SiliconCompiler
   `cxxrtl-cocotb` flow becomes a near copy of the existing `verilator-cocotb`
   tasks.

## Open questions (to resolve during implementation)

- **Library naming / loader.** cocotb's per-sim VPI libs (`libcocotbvpi_<sim>`)
  differ in how the sim *discovers* `vlog_startup_routines`. Since we own the
  harness `main()`, we can call cocotb's startup directly — confirm the exact
  entry symbol cocotb exposes for a "generic VPI" host.
- **Time model.** CXXRTL is cycle/delta oriented; VPI exposes a 64-bit sim time.
  Decide the mapping (e.g. one `cxxrtl_step` per delta, a harness-owned clock).
- **4-state.** CXXRTL is 2-state; VPI value formats include X/Z. Decide how to
  present `vpiBinStr`/`vpiVectorVal` (likely 0/1 only, no X/Z).
- **Edge callbacks.** `cbValueChange` requires snapshot/diff since `cxxrtl_capi`
  has no native change notification.
