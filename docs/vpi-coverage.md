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
| `vpi_iterate(vpiNet, module)` / `vpi_scan` | a scope's direct signals | ☑ |
| `vpi_iterate(vpiModule, module)` | a scope's direct sub-modules | ☑ |
| `vpi_handle_by_name` (signals + module scopes) | `cxxrtl_get`, `.`↔` ` translation, `<top>.` strip | ☑ |
| `vpi_handle(vpiParent/vpiScope, ref)` | drop the leaf of the CXXRTL path | ☑ |
| `vpi_handle_by_index(mem, addr)` | memory word view into `curr` | ☑ |
| `vpi_free_object` | free our wrapper handle | ☑ |

Hierarchy notes: CXXRTL names objects with a space separator (`u_inner dout`);
we translate to/from VPI's dotted form. Nested sub-modules are supported
(`dut.u_inner.dout`). Signals are reported under `vpiNet` (none under `vpiReg`)
to avoid net/reg double-listing.

## Signal values

Get/put support `vpiIntVal`, `vpiVectorVal`, and `vpiBinStrVal`; wide
(>32-bit, multi-chunk) signals work, with padding bits masked. cocotb writes
wide values as `vpiBinStrVal`, so that format is required, not optional.

## Memories / arrays

A CXXRTL memory (`cxxrtl_object.depth > 1`) is reported as `vpiMemory` with
`vpiSize` = depth. `vpi_handle_by_index(mem, addr)` returns a word view (offset
into the memory's `curr` storage by `(addr - zero_at) * chunks`). Words read and
write through `curr` because memories expose no `next` buffer. cocotb's
`dut.store[i]` read and write both work.

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

## Object classification & ranges

`vpi_get(vpiType)` distinguishes **net / reg / memory** from `cxxrtl_object`:
`depth > 1` → `vpiMemory`; `CXXRTL_DRIVEN_SYNC` (a flop/latch output) → `vpiReg`;
else `vpiNet`. `vpi_iterate` returns nets under `vpiNet` and regs+memories under
`vpiReg`, so cocotb's union sees each object once with the right kind.
`vpi_get(vpiLeftRange/vpiRightRange)` reports bit ranges (`lsb_at + width - 1`
down to `lsb_at`) for vectors and address ranges for memories — used by cocotb
for slicing and array bounds.

## cocotb regression suite results

Run cocotb 2.0's own core suite (`tests/test_cases/test_cocotb`) against
cxxrtl-vpi, using cocotb's `sample_module` built with `-D__ICARUS__` (the
plain-Verilog subset Yosys can parse — its full SystemVerilog uses unpacked
structs/interfaces/reals that Yosys's native frontend rejects).

**Core suite: 231 PASS / 24 FAIL / 3 SKIP (~91%).** The scheduler/timing/
concurrency core is essentially complete:

| module | result | module | result |
|---|---|---|---|
| concurrency_primitives | 25/25 | tests | 25/25 |
| synchronization_primitives | 9/9 | testfactory | 18/18 |
| queues | 9/9 | async_coroutines | 4/4 |
| async_generators | 4/4 | start_soon | 1/1 |
| sim_time_utils | 1/1 | edge_triggers | 17/18 |
| scheduler | 48/49 | timing_triggers | 22/23 |
| clock | 14/15 | handle | 36/57 |

Plus the standalone `test_force_release` case: **6/7** (force/release works —
which cocotb notes it does *not* on Verilator). Time precision is reported as
picoseconds, so sub-ns `Timer`/`Clock` values work.

The failures cluster in:
- **`test_handle`** (19) — SV `real`/`string`/`integer` signals (don't exist in
  the 2-state CXXRTL build), force/immediate writes, and escaped-identifier edge
  cases. Not scheduler/VPI faults.
- A few timing/clock edge cases (sub-ns precision; we report ns).

A correctness fix this surfaced: cocotb's **deferred** (ReadWrite-region) write
model must be used — *not* `COCOTB_TRUST_INERTIAL_WRITES`. Immediate writes break
same-coroutine "write a signal then `await ValueChange` on it" because top-level
inputs are CXXRTL `VALUE` objects (`next == curr`), so an immediate write changes
the value before the callback is primed. The `simulate()` loop applies writes in
the ReadWrite region, which both flushes correctly and preserves Edge semantics
(`test_edge_triggers` went 2/18 → 17/18).

## SystemVerilog frontend (verilog vs slang)

`build_cocotb_sim(..., frontend=...)` / `write_cxxrtl(..., frontend=...)` choose
how RTL is read:

- **`"verilog"`** (default) — Yosys's native `read_verilog -sv`: synthesizable
  Verilog and a SystemVerilog subset. Rejects full SV (e.g. unpacked structs:
  *"Only PACKED supported"*).
- **`"slang"`** — the [yosys-slang](https://github.com/povik/yosys-slang) plugin's
  `read_slang` (followed by `opt_clean` to lower its `$buf` cells). Handles full
  **synthesizable** SystemVerilog: unpacked structs, interfaces, packages,
  packed/multi-dim types, generate. Pass the built plugin via
  `slang_plugin="…/slang.so"` (and run the Yosys whose `yosys-config` built it).
  See `examples/cocotb_slang/`.

What slang does **not** unlock: non-synthesizable types like `real` and `string`.
CXXRTL *synthesizes* to a 2-state netlist, so these aren't representable
regardless of frontend (Verilator can model them because it's a behavioral
simulator, not a synthesizer). cocotb's `test_handle` `real`/`string` cases
therefore remain out of reach; its struct/interface/array cases need the slang
frontend.

## Known limitations

- **4-state (X/Z): not supported — inherent to CXXRTL.** Like Verilator (and
  unlike Icarus, which is a 4-state interpreter), CXXRTL is a 2-state engine.
  `s_vpi_vecval.bval` is always 0; X/Z written by a testbench collapse to 0.
  This cannot be fixed in the VPI layer — the engine never computes X/Z. Two
  mitigations exist:
  - **Visible coercion**: writing X/Z to a signal emits a one-time warning
    (`cxxrtl-vpi: warning: X/Z value written ... coerced to 0`) so the silent
    downgrade isn't a surprise.
  - **Init fuzzing**: CXXRTL inits state to 0, so a design that secretly relies
    on uninitialized state passes silently (the gap Verilator's `--x-initial
    unique` exists to catch). `build_cocotb_sim(..., randomize_init=True,
    init_seed=N)` (or `write_cxxrtl(..., randomize_init=True)`) seeds flop init
    with `setundef -init -random N`; varying the seed across runs exposes such
    dependence.
- **force/release**: supported (`vpiForceFlag`/`vpiReleaseFlag`) — a forced value
  is re-imposed after every evaluation, overriding the design's drivers, until
  released (6/7 of cocotb's `test_force_release`). The unhandled corner is a
  *deposit on a forced signal* (the deposited value isn't remembered for after
  release). cocotb's `Immediate` write action is also not specially handled.
- **Packed multi-dim arrays / structs, named events**: not handled. Packed
  vectors appear as plain wide signals.

## Notes

- `cxxrtl_object` exposes `curr` (read) and `next` (write) as arrays of
  `uint32_t` chunks, `width` bits. VPI vector values (`s_vpi_vecval`) are also
  32-bit chunked — the conversion is mostly a repack, not a reinterpretation.
- One design handle (`cxxrtl_handle`) is created once in the harness and shared;
  all VPI handles are lightweight wrappers around `cxxrtl_object*` + cached
  metadata.
