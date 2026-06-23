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
    // Our vpi_put_value stages into the model's `next` buffer and latches it on
    // the next step()/commit — i.e. it honours inertial-write semantics. Tell
    // cocotb it can apply writes immediately (the alternative, its deferred
    // ReadWrite-phase queue, assumes a phase model we don't match). Don't
    // override if the user set it explicitly.
    setenv("COCOTB_TRUST_INERTIAL_WRITES", "1", 0);

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
