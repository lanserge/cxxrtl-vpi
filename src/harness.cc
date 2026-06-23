// SPDX-License-Identifier: ISC
//
// Simulation harness / main(). The cocotb analogue of cocotb's
// share/lib/verilator/verilator.cpp: instantiate the design, hand control to
// the VPI startup (where cocotb registers itself), and run the event loop.
//
// STATUS: the model lifecycle and event loop are real (see simulate()). The
// cocotb bootstrap is the remaining wiring: cocotb's libcocotbvpi exports a
// startup routine that registers cbStartOfSimulation; once linked, call it
// before simulate() so cocotb discovers the toplevel and starts its scheduler.

#include "cxxrtl_vpi/model.h"
#include "cxxrtl_vpi/sim.h"

// cocotb's VPI bootstrap (provided by libcocotbvpi). It runs the registered
// vlog_startup_routines, which register cbStartOfSimulation.
//   TODO: link libcocotbvpi and enable this.
// extern "C" void vlog_startup_routines_bootstrap(void);

int main(int /*argc*/, char ** /*argv*/) {
    cxxrtl_vpi::Model model;
    model.create();
    cxxrtl_vpi::vpi_provider_bind(&model);

    // TODO: vlog_startup_routines_bootstrap();  // cocotb registers itself here

    cxxrtl_vpi::simulate(model);
    return 0;
}
