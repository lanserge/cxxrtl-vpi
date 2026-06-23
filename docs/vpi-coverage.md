# VPI coverage matrix

The subset of IEEE-1364 VPI that cocotb actually exercises, and the
`cxxrtl_capi` primitive each maps to. Status: ☐ todo · ◐ partial · ☑ done.

## MVP (enough for a clocked cocotb testbench: drive inputs, read outputs)

| VPI function | Purpose | cxxrtl_capi mapping | Status |
|---|---|---|---|
| `vlog_startup_routines` | cocotb registers itself at startup | harness calls cocotb's bootstrap | ☐ |
| `vpi_register_cb(cbStartOfSimulation)` | cocotb init | fired by `simulate()` at start | ☑ |
| `vpi_register_cb(cbEndOfSimulation)` | cocotb teardown | fired by `simulate()` at end | ☑ |
| `vpi_register_cb(cbReadWriteSynch)` | apply writes this step | one-shot, per step | ☑ |
| `vpi_register_cb(cbReadOnlySynch)` | sample after settle | one-shot, per step | ☑ |
| `vpi_register_cb(cbNextSimTime)` | advance to next step | one-shot, per advance | ☑ |
| `vpi_register_cb(cbAfterDelay)` | timed callback | absolute-time queue | ☑ |
| `vpi_register_cb(cbValueChange)` | signal edges | snapshot + diff after settle | ☑ |
| `vpi_remove_cb` | cancel a callback | flagged, reaped on dispatch | ☑ |
| `vpi_handle_by_name` | resolve a signal | `cxxrtl_get(handle, name)` | ☑ |
| `vpi_get(vpiType/vpiSize)` | signal metadata | `cxxrtl_object.{flags,width}` | ☑ |
| `vpi_get_str(vpiName/vpiFullName)` | naming | from the `cxxrtl_enum` table | ☑ |
| `vpi_get_value` | read a signal | `cxxrtl_object.curr` (width bits) | ☑ (int/binstr/vector) |
| `vpi_put_value` | write a signal | `cxxrtl_object.next` + commit | ☑ (int/vector) |
| `vpi_free_object` | release a handle | delete wrapper | ☑ |
| `vpi_get_time` | current sim time | scheduler clock | ☑ |
| `vpi_control(vpiFinish)` | end simulation | set stop flag | ☑ |

**Verified by** `tests/test_getput.cc` (`tests/run_getput.sh`): drives a counter
through the VPI API and checks it counts; `tests/test_conformance.cc` guards the
header values/layout.

## Discovery / iteration (needed for `dut.<...>` access & hierarchy walk)

| VPI function | cxxrtl_capi mapping | Status |
|---|---|---|
| `vpi_iterate(vpiModule, NULL)` | root toplevel handle | ☑ |
| `vpi_iterate(vpiNet, module)` / `vpi_scan` | top-level signals from the enum table | ☑ |
| `vpi_handle_by_name` (qualified + bare) | `cxxrtl_get` with `<top>.` strip | ☑ |
| `vpi_handle(type, ref)` | parent/scope nav | ☐ (stub) |
| `vpi_free_object` | free our wrapper handle | ☑ |

Simplifications (MVP): a single flat toplevel scope (no nested submodules yet);
all top-level signals reported under `vpiNet` to avoid net/reg double-listing.

## cocotb bring-up: WORKING ✅

An **unmodified cocotb testbench runs against a CXXRTL model and passes**
(`examples/cocotb_counter/run_cocotb.sh` → `TESTS=1 PASS=1 FAIL=0`, clean exit).
The harness links `libcocotbvpi` + libpython, calls
`vlog_startup_routines_bootstrap()`, and cocotb then:

- ✅ embeds Python, registers our VPI (`vpi_get_vlog_info` → "cxxrtl-vpi 0.0.0"),
- ✅ discovers the toplevel and signals (`vpi_iterate`/`vpi_scan`),
- ✅ drives the clock and reset through `vpi_put_value`, observes edges via
  `cbValueChange`,
- ✅ runs the test to completion, ends via `vpi_control(vpiFinish)`, writes
  `results.xml`,
- ✅ reports time precision so `Clock(…, "ns")` works (`vpi_get(vpiTimePrecision)` = -9).

Startup also needed three VPI calls cocotb makes early:
`vpi_get_vlog_info`, `vpi_chk_error`, `vpi_handle_by_index`.

### The write-flush resolution

cocotb 2.0's default (`COCOTB_TRUST_INERTIAL_WRITES=0`) defers `handle.value = x`
writes into a queue applied only when its `_do_writes` task's `await ReadWrite()`
fires (`cocotb/handle.py`, `cocotb/_scheduler.py:_sim_react`). That path assumes a
ReadWrite-phase model our scheduler doesn't reproduce, so writes never reached
`vpi_put_value`. The fix: our `vpi_put_value` already honours inertial semantics
(stages into `next`, latched on `step()`), so the harness sets
`COCOTB_TRUST_INERTIAL_WRITES=1` (default, user-overridable) and cocotb applies
each write immediately via `vpi_put_value`. Writes flow, the clock toggles, the
test passes. Trace the scheduler with `CXXRTL_VPI_DEBUG=1`.

## Deferred / likely-not-needed for MVP

- `vpiMemory` / arrays, force/release, `vpiNamedEvent`.
- 4-state (X/Z) value formats — CXXRTL is 2-state.

## Notes

- `cxxrtl_object` exposes `curr` (read) and `next` (write) as arrays of
  `uint32_t` chunks, `width` bits. VPI vector values (`s_vpi_vecval`) are also
  32-bit chunked — the conversion is mostly a repack, not a reinterpretation.
- One design handle (`cxxrtl_handle`) is created once in the harness and shared;
  all VPI handles are lightweight wrappers around `cxxrtl_object*` + cached
  metadata.
