# VPI coverage matrix

The subset of IEEE-1364 VPI that cocotb actually exercises, and the
`cxxrtl_capi` primitive each maps to. Status: ☐ todo · ◐ partial · ☑ done.

## MVP (enough for a clocked cocotb testbench: drive inputs, read outputs)

| VPI function | Purpose | cxxrtl_capi mapping | Status |
|---|---|---|---|
| `vlog_startup_routines` | cocotb registers itself at startup | harness calls cocotb's bootstrap | ☐ |
| `vpi_register_cb(cbStartOfSimulation)` | cocotb init | harness fires after model create | ☐ |
| `vpi_register_cb(cbEndOfSimulation)` | cocotb teardown | harness fires before destroy | ☐ |
| `vpi_register_cb(cbReadWriteSynch)` | apply writes this step | queue → `cxxrtl_commit` | ☐ |
| `vpi_register_cb(cbReadOnlySynch)` | sample after settle | post-`cxxrtl_eval` snapshot | ☐ |
| `vpi_register_cb(cbNextSimTime)` | advance to next step | `cxxrtl_step` | ☐ |
| `vpi_register_cb(cbAfterDelay)` | timed callback | harness time queue | ☐ |
| `vpi_handle_by_name` | resolve a signal | `cxxrtl_get(handle, name)` | ☐ |
| `vpi_get(vpiType/vpiSize)` | signal metadata | `cxxrtl_object.{type,width}` | ☐ |
| `vpi_get_str(vpiName/vpiFullName)` | naming | from the `cxxrtl_enum` table | ☐ |
| `vpi_get_value` | read a signal | `cxxrtl_object.curr` (width bits) | ☐ |
| `vpi_put_value` | write a signal | `cxxrtl_object.next` + commit | ☐ |
| `vpi_get_time` | current sim time | harness clock | ☐ |
| `vpi_control(vpiFinish)` | end simulation | set stop flag | ☐ |

## Discovery / iteration (needed for `dut.<...>` access & hierarchy walk)

| VPI function | cxxrtl_capi mapping | Status |
|---|---|---|
| `vpi_iterate` / `vpi_scan` | iterate the `cxxrtl_enum` name table | ☐ |
| `vpi_handle(vpiModule, …)` | hierarchy from dotted names in enum | ☐ |
| `vpi_free_object` | free our wrapper handle | ☐ |

## Deferred / likely-not-needed for MVP

- `cbValueChange` — requires snapshot+diff (no native notify in capi).
- `vpiMemory` / arrays, force/release, `vpiNamedEvent`.
- 4-state (X/Z) value formats — CXXRTL is 2-state.

## Notes

- `cxxrtl_object` exposes `curr` (read) and `next` (write) as arrays of
  `uint32_t` chunks, `width` bits. VPI vector values (`s_vpi_vecval`) are also
  32-bit chunked — the conversion is mostly a repack, not a reinterpretation.
- One design handle (`cxxrtl_handle`) is created once in the harness and shared;
  all VPI handles are lightweight wrappers around `cxxrtl_object*` + cached
  metadata.
