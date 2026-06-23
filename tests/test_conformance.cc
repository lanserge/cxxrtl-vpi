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

// --- put flags / control ---
static_assert(vpiNoDelay == 1, "vpiNoDelay");
static_assert(vpiFinish == 67, "vpiFinish");

// --- struct layout: s_vpi_vecval must be {aval, bval} as two 32-bit words ---
static_assert(sizeof(s_vpi_vecval) == 2 * sizeof(PLI_UINT32), "vecval size");
static_assert(offsetof(s_vpi_vecval, aval) == 0, "vecval.aval offset");
static_assert(offsetof(s_vpi_vecval, bval) == sizeof(PLI_UINT32), "vecval.bval offset");

// --- s_vpi_value: format is the first member; the union follows ---
static_assert(offsetof(s_vpi_value, format) == 0, "value.format offset");

int main() { return 0; }
