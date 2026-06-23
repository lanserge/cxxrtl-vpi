// SPDX-License-Identifier: ISC
//
// vpi_user.h - a minimal, self-contained subset of the IEEE 1364/1800 VPI
// (Verilog Procedural Interface) header.
//
// This is an original, hand-written header that declares only the portion of
// VPI that cxxrtl-vpi implements. The constant values and the s_vpi_value /
// s_vpi_vecval memory layouts are *normative* parts of the IEEE standard: they
// must match the values any VPI client (e.g. cocotb's libcocotbvpi) was
// compiled against, since those structs cross the ABI boundary. They are
// reproduced here from the public standard, not copied from any vendor's file.
//
// As the provider grows (callbacks, iteration, control), extend this header
// with the corresponding standard declarations. For a drop-in full header,
// build with -DVPI_INCLUDE_DIR pointing at a complete vpi_user.h instead.

#ifndef VPI_USER_H
#define VPI_USER_H

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Portable scalar types (IEEE 1364 PLI) ---- */
typedef int32_t  PLI_INT32;
typedef uint32_t PLI_UINT32;
typedef int64_t  PLI_INT64;
typedef uint64_t PLI_UINT64;
typedef char     PLI_BYTE8;

/* Opaque handle to any VPI object. */
typedef PLI_UINT32 *vpiHandle;

/* ---- Value formats (s_vpi_value.format) ---- */
#define vpiBinStrVal    1
#define vpiOctStrVal    2
#define vpiDecStrVal    3
#define vpiHexStrVal    4
#define vpiScalarVal    5
#define vpiIntVal       6
#define vpiRealVal      7
#define vpiStringVal    8
#define vpiVectorVal    9
#define vpiTimeVal      11

/* ---- Object properties (vpi_get / vpi_get_str) ---- */
#define vpiType         1
#define vpiName         2
#define vpiFullName     3
#define vpiSize         4

/* ---- Object types (returned by vpi_get(vpiType, ...)) ---- */
#define vpiModule       32
#define vpiNet          36
#define vpiParameter    41
#define vpiPort         44
#define vpiReg          48

/* ---- Scalar bit values ---- */
#define vpi0            0
#define vpi1            1
#define vpiZ            2
#define vpiX            3

/* ---- vpi_put_value() delay flags ---- */
#define vpiNoDelay       1
#define vpiInertialDelay 2

/* ---- vpi_control() operations ---- */
#define vpiStop         66
#define vpiFinish       67

/* ---- Time types (s_vpi_time.type) ---- */
#define vpiScaledRealTime 1
#define vpiSimTime        2

/* ---- Callback reasons (for the next implementation stage) ---- */
#define cbValueChange         1
#define cbReadWriteSynch      6
#define cbReadOnlySynch       7
#define cbNextSimTime         8
#define cbAfterDelay          9
#define cbStartOfSimulation  11
#define cbEndOfSimulation    12

/* ---- Vector value element: ab encoding 00=0 10=1 11=X 01=Z (2-state: b=0) ---- */
typedef struct t_vpi_vecval {
    PLI_UINT32 aval, bval;
} s_vpi_vecval, *p_vpi_vecval;

/* ---- Time value ---- */
typedef struct t_vpi_time {
    PLI_INT32  type;
    PLI_UINT32 high, low;
    double     real;
} s_vpi_time, *p_vpi_time;

/* ---- Tagged value union exchanged by vpi_get_value / vpi_put_value ---- */
typedef struct t_vpi_value {
    PLI_INT32 format;
    union {
        PLI_BYTE8           *str;
        PLI_INT32            scalar;
        PLI_INT32            integer;
        double               real;
        struct t_vpi_time   *time;
        struct t_vpi_vecval *vector;
        PLI_BYTE8           *misc;
    } value;
} s_vpi_value, *p_vpi_value;

/* ---- Callback data (layout is normative; matches IEEE 1364) ---- */
typedef struct t_cb_data {
    PLI_INT32   reason;
    PLI_INT32 (*cb_rtn)(struct t_cb_data *);
    vpiHandle   obj;
    p_vpi_time  time;
    p_vpi_value value;
    PLI_INT32   index;
    PLI_BYTE8  *user_data;
} s_cb_data, *p_cb_data;

/* ---- Implemented entry points ---- */
vpiHandle vpi_handle_by_name(PLI_BYTE8 *name, vpiHandle scope);
vpiHandle vpi_handle(PLI_INT32 type, vpiHandle refHandle);
vpiHandle vpi_iterate(PLI_INT32 type, vpiHandle refHandle);
vpiHandle vpi_scan(vpiHandle iterator);
vpiHandle vpi_register_cb(p_cb_data cb_data_p);
PLI_INT32 vpi_remove_cb(vpiHandle cb_obj);
void      vpi_get_time(vpiHandle object, p_vpi_time time_p);
PLI_INT32 vpi_control(PLI_INT32 operation, ...);
void      vpi_get_value(vpiHandle expr, p_vpi_value value_p);
vpiHandle vpi_put_value(vpiHandle object, p_vpi_value value_p,
                        p_vpi_time time_p, PLI_INT32 flags);
PLI_INT32 vpi_get(PLI_INT32 property, vpiHandle object);
PLI_BYTE8 *vpi_get_str(PLI_INT32 property, vpiHandle object);
PLI_INT32 vpi_free_object(vpiHandle object);

#ifdef __cplusplus
}
#endif

#endif /* VPI_USER_H */
