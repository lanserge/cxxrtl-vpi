// SPDX-License-Identifier: ISC
//
// Simulation harness / main(). This is the cocotb analogue of cocotb's
// share/lib/verilator/verilator.cpp: it instantiates the design, hands control
// to the VPI startup (where cocotb registers itself), and drives the time loop.
//
// STATUS: SCAFFOLD. The model lifecycle and step loop are real; the cocotb
// startup hook and callback servicing are marked TODO.

#include <cstdio>

#include "cxxrtl_vpi/model.h"

namespace cxxrtl_vpi {
void vpi_provider_bind(Model *model);  // from vpi_provider.cc
}

// cocotb exposes a startup entry that registers its VPI callbacks. For a
// generic VPI host that owns main(), we call it directly rather than relying on
// the simulator's vlog_startup_routines discovery.
//   TODO: confirm the exact symbol cocotb's libcocotbvpi exports and declare it.
//   extern "C" void <cocotb_startup_entry>();

// Set by vpi_control(vpiFinish) through the provider.
extern "C" volatile int cxxrtl_vpi_finished;
volatile int cxxrtl_vpi_finished = 0;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    cxxrtl_vpi::Model model;
    model.create();
    cxxrtl_vpi::vpi_provider_bind(&model);

    // TODO: invoke cocotb's VPI startup here so it registers cbStartOfSimulation
    //       and discovers the toplevel. e.g. run registered startup routines.

    // Drive the simulation until cocotb requests finish.
    //
    // CXXRTL is delta/cycle oriented; cocotb expects a VPI time axis. The MVP
    // model: each iteration services time-0 callbacks (ReadWrite/ReadOnly),
    // then advances one step. A real clock is produced by cocotb toggling a
    // clock input via vpi_put_value; here we just keep stepping.
    const long max_steps = 1000000;  // safety bound for the scaffold
    long steps = 0;
    while (!cxxrtl_vpi_finished && steps++ < max_steps) {
        // TODO: service cbReadWriteSynch -> model.commit()
        // TODO: service cbReadOnlySynch  (sample)
        model.eval();
        model.step();
        // TODO: fire cbNextSimTime / cbAfterDelay due at this time
    }

    if (steps >= max_steps)
        std::fprintf(stderr, "cxxrtl-vpi: hit max_steps safety bound\n");

    // TODO: fire cbEndOfSimulation so cocotb writes results.xml
    return 0;
}
