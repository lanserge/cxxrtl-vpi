// SPDX-License-Identifier: ISC
//
// Guards our hand-written vpi_user.h against transcription errors / drift.
// These literals are the normative IEEE-1364/1800 values; any VPI client
// (cocotb) is compiled against them, so ours must match exactly. Compile-time
// only — no system header required.
//
// If you build with -DVPI_INCLUDE_DIR=<full vpi_user.h>, this same file also
// validates that the override agrees with the standard values.

#include <cstddef>
#include <vpi_user.h>

// --- value formats ---
static_assert(vpiBinStrVal == 1, "vpiBinStrVal");
static_assert(vpiIntVal == 6, "vpiIntVal");
static_assert(vpiVectorVal == 9, "vpiVectorVal");

// --- properties ---
static_assert(vpiType == 1, "vpiType");
static_assert(vpiName == 2, "vpiName");
static_assert(vpiFullName == 3, "vpiFullName");
static_assert(vpiSize == 4, "vpiSize");

// --- object types ---
static_assert(vpiModule == 32, "vpiModule");
static_assert(vpiNet == 36, "vpiNet");
static_assert(vpiReg == 48, "vpiReg");
static_assert(vpiGenScopeArray == 133, "vpiGenScopeArray");
static_assert(vpiGenScope == 134, "vpiGenScope");

// --- put flags / control ---
static_assert(vpiNoDelay == 1, "vpiNoDelay");
static_assert(vpiFinish == 67, "vpiFinish");

// --- struct layout: s_vpi_vecval must be {aval, bval} as two 32-bit words ---
static_assert(sizeof(s_vpi_vecval) == 2 * sizeof(PLI_UINT32), "vecval size");
static_assert(offsetof(s_vpi_vecval, aval) == 0, "vecval.aval offset");
static_assert(offsetof(s_vpi_vecval, bval) == sizeof(PLI_UINT32), "vecval.bval offset");

// --- s_vpi_value: format is the first member; the union follows ---
static_assert(offsetof(s_vpi_value, format) == 0, "value.format offset");

// --- callback reasons ---
static_assert(cbValueChange == 1, "cbValueChange");
static_assert(cbReadWriteSynch == 6, "cbReadWriteSynch");
static_assert(cbReadOnlySynch == 7, "cbReadOnlySynch");
static_assert(cbNextSimTime == 8, "cbNextSimTime");
static_assert(cbAfterDelay == 9, "cbAfterDelay");
static_assert(cbStartOfSimulation == 11, "cbStartOfSimulation");
static_assert(cbEndOfSimulation == 12, "cbEndOfSimulation");

// --- s_cb_data field order (normative; cocotb reads these across the ABI) ---
static_assert(offsetof(s_cb_data, reason) == 0, "cb.reason offset");
static_assert(offsetof(s_cb_data, cb_rtn) == sizeof(void *), "cb.cb_rtn offset");

int main() { return 0; }
