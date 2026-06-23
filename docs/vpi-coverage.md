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
- **force/release**: `vpi_put_value` flags are ignored; a write behaves like a
  deposit (in trust-inertial mode it applies immediately). True force/release
  (override + later restore) is not tracked.
- **Packed multi-dim arrays / structs, named events**: not handled. Packed
  vectors appear as plain wide signals.

## Notes

- `cxxrtl_object` exposes `curr` (read) and `next` (write) as arrays of
  `uint32_t` chunks, `width` bits. VPI vector values (`s_vpi_vecval`) are also
  32-bit chunked — the conversion is mostly a repack, not a reinterpretation.
- One design handle (`cxxrtl_handle`) is created once in the harness and shared;
  all VPI handles are lightweight wrappers around `cxxrtl_object*` + cached
  metadata.
