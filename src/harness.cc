// SPDX-License-Identifier: ISC
//
// Simulation harness / main(). The cocotb analogue of cocotb's
// share/lib/verilator/verilator.cpp: instantiate the design, hand control to
// the VPI startup (where cocotb registers itself), and run the event loop.
//
// Build with -DCXXRTL_VPI_COCOTB and link cocotb's libcocotbvpi to enable the
// cocotb bootstrap; without it, the harness runs the model with our own VPI
// provider only (useful for non-cocotb VPI clients and tests).

#include <cstdlib>

#include "cxxrtl_vpi/model.h"
#include "cxxrtl_vpi/sim.h"

#ifdef CXXRTL_VPI_COCOTB
// Provided by cocotb's libcocotbvpi: iterates vlog_startup_routines, each of
// which registers cocotb's cbStartOfSimulation via vpi_register_cb.
extern "C" void vlog_startup_routines_bootstrap(void);
#endif

int main(int /*argc*/, char ** /*argv*/) {
    // NOTE: we deliberately do NOT force COCOTB_TRUST_INERTIAL_WRITES. cocotb's
    // default deferred-write model (apply in the ReadWrite region) is what the
    // simulate() loop implements, and it is also *required* for correct
    // ValueChange/Edge semantics: a testbench that writes a signal and then
    // awaits its change in the same coroutine relies on the write being applied
    // after the trigger is primed. Immediate (trust-inertial) writes would make
    // the change happen before the callback is registered, so it never fires.

    cxxrtl_vpi::Model model;
    model.create();

    if (const char *top = std::getenv("COCOTB_TOPLEVEL"))
        model.set_top_name(top);

    cxxrtl_vpi::vpi_provider_bind(&model);

#ifdef CXXRTL_VPI_COCOTB
    vlog_startup_routines_bootstrap();  // cocotb registers itself here
#endif

    cxxrtl_vpi::simulate(model);
    return 0;
}
