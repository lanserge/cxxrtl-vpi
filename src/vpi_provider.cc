// SPDX-License-Identifier: ISC
//
// IEEE-1364 VPI provider implemented on top of cxxrtl_capi.
//
// Makes a CXXRTL model look like a VPI-speaking simulator so a VPI client
// (cocotb's libcocotbvpi) can drive it. Two layers:
//
//   1. Object access  - handle/get/put on signals (resolved via cxxrtl_enum).
//   2. Event loop      - callback registry + dispatch + simulate(), mirroring
//                        cocotb's reference flow. cocotb drives the clock and
//                        timers via cbAfterDelay and edges via cbValueChange;
//                        we fire ReadWrite/ReadOnly/NextSimTime each step.
//
// See docs/vpi-coverage.md for the surface and status.

#include <vpi_user.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "cxxrtl_vpi/model.h"
#include "cxxrtl_vpi/sim.h"

namespace {

// ---- active design & simulation state ------------------------------------
cxxrtl_vpi::Model *g_model = nullptr;
uint64_t g_time = 0;
bool g_finished = false;

constexpr uint64_t NO_DEADLINE = std::numeric_limits<uint64_t>::max();

// A VPI signal handle is an opaque pointer; back it with this wrapper.
struct VpiObject {
    cxxrtl_vpi::Signal *sig;
};

// A registered callback. Also an opaque vpiHandle (returned by vpi_register_cb).
struct CbObject {
    s_cb_data data;                  // copied from registration
    s_vpi_time time;                 // backing storage for data.time
    uint64_t deadline = 0;           // absolute time, for cbAfterDelay
    cxxrtl_vpi::Signal *watch = nullptr;  // for cbValueChange
    std::vector<uint32_t> last;      // last seen value, for change detection
    bool removed = false;
};

VpiObject *as_obj(vpiHandle h) { return reinterpret_cast<VpiObject *>(h); }
vpiHandle obj_handle(VpiObject *o) { return reinterpret_cast<vpiHandle>(o); }
CbObject *as_cb(vpiHandle h) { return reinterpret_cast<CbObject *>(h); }
vpiHandle cb_handle(CbObject *o) { return reinterpret_cast<vpiHandle>(o); }

// Callback registries, by reason.
std::vector<CbObject *> g_timed;     // cbAfterDelay
std::vector<CbObject *> g_value;     // cbValueChange (persistent)
std::vector<CbObject *> g_rw;        // cbReadWriteSynch (one-shot)
std::vector<CbObject *> g_ro;        // cbReadOnlySynch  (one-shot)
std::vector<CbObject *> g_nextsim;   // cbNextSimTime    (one-shot)
std::vector<CbObject *> g_startsim;  // cbStartOfSimulation (one-shot)
std::vector<CbObject *> g_endsim;    // cbEndOfSimulation   (one-shot)

uint64_t time_from(const s_vpi_time *t) {
    if (!t)
        return 0;
    return (static_cast<uint64_t>(t->high) << 32) | t->low;
}

void stamp_time(CbObject *o) {
    if (o->data.time) {
        o->time.type = vpiSimTime;
        o->time.high = static_cast<PLI_UINT32>(g_time >> 32);
        o->time.low = static_cast<PLI_UINT32>(g_time & 0xffffffffu);
    }
}

void invoke(CbObject *o) {
    if (o->removed)
        return;
    stamp_time(o);
    if (o->data.cb_rtn)
        o->data.cb_rtn(&o->data);
}

// Fire and consume a one-shot list. Swapped out first so callbacks that
// register new ones for the next round don't fire this round.
void fire_oneshot(std::vector<CbObject *> &list) {
    std::vector<CbObject *> batch;
    batch.swap(list);
    for (CbObject *o : batch) {
        invoke(o);
        delete o;
    }
}

// Fire value-change callbacks whose watched signal changed. Returns true if any
// fired (so the caller can settle to a fixed point).
bool fire_value_cbs() {
    bool any = false;
    for (size_t i = 0; i < g_value.size();) {
        CbObject *o = g_value[i];
        if (o->removed) {
            g_value.erase(g_value.begin() + i);
            delete o;
            continue;
        }
        std::vector<uint32_t> cur;
        g_model->read(*o->watch, cur);
        if (cur != o->last) {
            o->last = cur;
            invoke(o);
            any = true;
        }
        ++i;
    }
    return any;
}

// Advance combinational + sequential logic, then run value-change callbacks to
// a fixed point (a callback may drive new values, causing more changes).
void settle() {
    do {
        g_model->step();
    } while (fire_value_cbs());
}

void fire_timed_due() {
    std::vector<CbObject *> due;
    for (size_t i = 0; i < g_timed.size();) {
        CbObject *o = g_timed[i];
        if (o->removed || o->deadline <= g_time) {
            due.push_back(o);
            g_timed.erase(g_timed.begin() + i);
        } else {
            ++i;
        }
    }
    for (CbObject *o : due) {
        invoke(o);
        delete o;
    }
}

uint64_t next_deadline() {
    uint64_t m = NO_DEADLINE;
    for (CbObject *o : g_timed)
        if (!o->removed)
            m = std::min(m, o->deadline);
    return m;
}

}  // namespace

// ===========================================================================
// Provider-internal entry points (called by the harness via sim.h)
// ===========================================================================

namespace cxxrtl_vpi {

void vpi_provider_bind(Model *model) {
    g_model = model;
    g_time = 0;
    g_finished = false;
}

void simulate(Model &model) {
    g_model = &model;

    fire_oneshot(g_startsim);
    settle();

    while (!g_finished) {
        settle();
        fire_oneshot(g_rw);
        settle();
        fire_oneshot(g_ro);

        uint64_t next = next_deadline();
        if (next == NO_DEADLINE)
            break;  // no more events scheduled
        g_time = next;

        fire_oneshot(g_nextsim);
        fire_timed_due();  // clock toggles, cocotb timers, ...
        settle();
    }

    fire_oneshot(g_endsim);
}

}  // namespace cxxrtl_vpi

// ===========================================================================
// VPI: object access
// ===========================================================================

vpiHandle vpi_handle_by_name(PLI_BYTE8 *name, vpiHandle scope) {
    (void)scope;  // TODO: honour scope once hierarchy iteration lands.
    if (!g_model || !name)
        return nullptr;
    cxxrtl_vpi::Signal *sig = g_model->by_name(name);
    if (!sig)
        return nullptr;
    return obj_handle(new VpiObject{sig});
}

PLI_INT32 vpi_get(PLI_INT32 property, vpiHandle object) {
    VpiObject *o = as_obj(object);
    if (!o)
        return 0;
    switch (property) {
        case vpiSize:
            return static_cast<PLI_INT32>(o->sig->width);
        case vpiType:
            return o->sig->object->next ? vpiReg : vpiNet;
        default:
            return 0;
    }
}

PLI_BYTE8 *vpi_get_str(PLI_INT32 property, vpiHandle object) {
    VpiObject *o = as_obj(object);
    if (!o)
        return nullptr;
    static thread_local std::string buf;
    switch (property) {
        case vpiFullName:
            buf = o->sig->name;
            return const_cast<PLI_BYTE8 *>(buf.c_str());
        case vpiName: {
            const std::string &full = o->sig->name;
            size_t dot = full.rfind('.');
            buf = (dot == std::string::npos) ? full : full.substr(dot + 1);
            return const_cast<PLI_BYTE8 *>(buf.c_str());
        }
        default:
            return nullptr;
    }
}

void vpi_get_value(vpiHandle expr, p_vpi_value value_p) {
    VpiObject *o = as_obj(expr);
    if (!o || !value_p)
        return;

    std::vector<uint32_t> bits;
    const size_t width = g_model->read(*o->sig, bits);
    const size_t chunks = bits.size();

    switch (value_p->format) {
        case vpiIntVal:
            value_p->value.integer = chunks ? static_cast<PLI_INT32>(bits[0]) : 0;
            break;

        case vpiBinStrVal: {
            static thread_local std::string s;
            s.clear();
            for (size_t i = width; i-- > 0;) {
                uint32_t chunk = bits[i / 32];
                s.push_back(((chunk >> (i % 32)) & 1u) ? '1' : '0');
            }
            value_p->value.str = const_cast<PLI_BYTE8 *>(s.c_str());
            break;
        }

        case vpiVectorVal: {
            static thread_local std::vector<s_vpi_vecval> vec;
            vec.assign(chunks ? chunks : 1, s_vpi_vecval{0, 0});
            for (size_t i = 0; i < chunks; i++) {
                vec[i].aval = bits[i];  // 2-state: bval stays 0
                vec[i].bval = 0;
            }
            value_p->value.vector = vec.data();
            break;
        }

        default:
            break;  // TODO: vpiHexStrVal, vpiRealVal, ...
    }
}

vpiHandle vpi_put_value(vpiHandle object, p_vpi_value value_p, p_vpi_time time_p,
                        PLI_INT32 flags) {
    (void)time_p;
    (void)flags;  // TODO: honour vpiInertialDelay scheduling; MVP = NoDelay.
    VpiObject *o = as_obj(object);
    if (!o || !value_p)
        return nullptr;

    const size_t width = o->sig->width;
    const size_t chunks = (width + 31) / 32;
    std::vector<uint32_t> val(chunks, 0);

    switch (value_p->format) {
        case vpiIntVal:
            if (chunks)
                val[0] = static_cast<uint32_t>(value_p->value.integer);
            break;
        case vpiVectorVal:
            for (size_t i = 0; i < chunks; i++)
                val[i] = value_p->value.vector[i].aval;
            break;
        default:
            return nullptr;
    }

    g_model->write(*o->sig, val);  // staged into `next`; latched by settle()
    return object;
}

PLI_INT32 vpi_free_object(vpiHandle object) {
    // Signal handles are freed here; callback handles are freed when they fire
    // or via vpi_remove_cb. cocotb does not pass one to the other.
    delete as_obj(object);
    return 1;
}

// ===========================================================================
// VPI: callbacks, time, control
// ===========================================================================

vpiHandle vpi_register_cb(p_cb_data cb_data_p) {
    if (!cb_data_p)
        return nullptr;

    CbObject *o = new CbObject;
    o->data = *cb_data_p;
    if (cb_data_p->time) {
        o->time = *cb_data_p->time;
        o->data.time = &o->time;  // point at our own storage
    }

    switch (cb_data_p->reason) {
        case cbAfterDelay:
            o->deadline = g_time + time_from(cb_data_p->time);
            g_timed.push_back(o);
            break;
        case cbValueChange: {
            VpiObject *vobj = as_obj(cb_data_p->obj);
            if (!vobj) {
                delete o;
                return nullptr;
            }
            o->watch = vobj->sig;
            g_model->read(*o->watch, o->last);
            g_value.push_back(o);
            break;
        }
        case cbReadWriteSynch:
            g_rw.push_back(o);
            break;
        case cbReadOnlySynch:
            g_ro.push_back(o);
            break;
        case cbNextSimTime:
            g_nextsim.push_back(o);
            break;
        case cbStartOfSimulation:
            g_startsim.push_back(o);
            break;
        case cbEndOfSimulation:
            g_endsim.push_back(o);
            break;
        default:
            delete o;
            return nullptr;
    }
    return cb_handle(o);
}

PLI_INT32 vpi_remove_cb(vpiHandle cb_obj) {
    CbObject *o = as_cb(cb_obj);
    if (!o)
        return 0;
    o->removed = true;  // reaped by the next dispatch pass
    return 1;
}

void vpi_get_time(vpiHandle object, p_vpi_time time_p) {
    (void)object;
    if (!time_p)
        return;
    time_p->type = vpiSimTime;
    time_p->high = static_cast<PLI_UINT32>(g_time >> 32);
    time_p->low = static_cast<PLI_UINT32>(g_time & 0xffffffffu);
}

PLI_INT32 vpi_control(PLI_INT32 operation, ...) {
    if (operation == vpiFinish || operation == vpiStop)
        g_finished = true;
    return 1;
}
