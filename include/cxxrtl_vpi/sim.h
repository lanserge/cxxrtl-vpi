// SPDX-License-Identifier: ISC
//
// Entry points the harness (and tests) use to bind a model and run the VPI
// event loop. The loop mirrors cocotb's reference flow (see cocotb's
// share/lib/verilator/verilator.cpp): start-of-sim, then per time step settle →
// ReadWrite → ReadOnly, advance to the next timed-callback deadline, fire
// NextSimTime + timed (cbAfterDelay) callbacks, repeat until vpi_control(finish)
// or no events remain.

#ifndef CXXRTL_VPI_SIM_H
#define CXXRTL_VPI_SIM_H

#include "cxxrtl_vpi/model.h"

namespace cxxrtl_vpi {

// Make `model` the active design for the VPI entry points. Call once, after
// Model::create(), before registering callbacks or running.
void vpi_provider_bind(Model *model);

// Run the VPI event loop to completion on the bound model.
void simulate(Model &model);

}  // namespace cxxrtl_vpi

#endif  // CXXRTL_VPI_SIM_H
