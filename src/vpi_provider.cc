// SPDX-License-Identifier: ISC
//
// IEEE-1364 VPI provider implemented on top of cxxrtl_capi.
//
// This is the heart of the project: it makes a CXXRTL model look like a
// VPI-speaking simulator, so cocotb's generic VPI consumer (libcocotbvpi) can
// drive it unmodified. The entry points below are the subset cocotb exercises;
// see docs/vpi-coverage.md for the full matrix and status.
//
// STATUS: SCAFFOLD. Each function documents its intended cxxrtl_capi mapping
// and currently returns a safe not-implemented value. Filling these in (in the
// order of vpi-coverage.md "MVP") is the implementation work.

#include <vpi_user.h>

#include "cxxrtl_vpi/model.h"

namespace {

// The single design instance, owned by the harness (see harness.cc).
cxxrtl_vpi::Model *g_model = nullptr;

}  // namespace

namespace cxxrtl_vpi {

// Called by the harness right after Model::create(), before VPI startup.
void vpi_provider_bind(Model *model) { g_model = model; }

}  // namespace cxxrtl_vpi

// ---------------------------------------------------------------------------
// Object access
// ---------------------------------------------------------------------------

// Resolve a signal handle by hierarchical name.
//   maps to: cxxrtl_get(handle, name) via Model::by_name
PLI_INT32 /*vpiHandle*/ // NOTE: real signature returns vpiHandle
vpi_handle_by_name_stub(PLI_BYTE8 *name, vpiHandle scope) {
    (void)scope;
    // TODO: cxxrtl_vpi::Signal *s = g_model->by_name(name);
    //       wrap `s` in a vpiHandle (a small struct we allocate) and return it.
    (void)name;
    return 0;  // null handle
}

// Read a signal's current value.
//   maps to: Model::read() -> repack into value_p->value.vector (s_vpi_vecval)
//   void vpi_get_value(vpiHandle expr, p_vpi_value value_p);
// TODO: implement against the real prototype from vpi_user.h.

// Write a signal value.
//   maps to: Model::write() -> staged into `next`, committed at cbReadWriteSynch
// TODO: vpiHandle vpi_put_value(vpiHandle, p_vpi_value, p_vpi_time, PLI_INT32);

// ---------------------------------------------------------------------------
// Iteration / hierarchy (for dut.<...> discovery)
//   maps to: walk Model::signals(); split full names on '.' for module scopes
// TODO: vpi_iterate / vpi_scan / vpi_handle(vpiModule, ...)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Callbacks & time
//   cbStartOfSimulation / cbEndOfSimulation : harness lifecycle
//   cbReadWriteSynch  : flush staged writes -> Model::commit()
//   cbReadOnlySynch   : sample after Model::eval()
//   cbNextSimTime     : Model::step()
//   cbAfterDelay      : harness time queue
// TODO: vpi_register_cb / vpi_remove_cb backed by a harness-owned queue.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Control
//   vpi_control(vpiFinish/vpiStop) : signal the harness loop to stop.
// TODO.
// ---------------------------------------------------------------------------

// NOTE: The stubs above intentionally use placeholder names/signatures to keep
// the scaffold compiling without a full vpi_user.h surface on every platform.
// The implementation step replaces them with the exact prototypes from
// IEEE 1364 vpi_user.h and registers cocotb's startup (see harness.cc).
