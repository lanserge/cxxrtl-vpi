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
#include <cstdio>
#include <cstdlib>
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

bool g_debug = false;
#define DBG(...)                              \
    do {                                      \
        if (g_debug) {                        \
            std::fprintf(stderr, "[cxxrtl-vpi] " __VA_ARGS__); \
            std::fprintf(stderr, "\n");       \
        }                                     \
    } while (0)

// A VPI handle is an opaque pointer. We back it with a tagged wrapper that can
// be a signal, a module (scope), or an iterator over child handles.
enum HKind { H_SIGNAL, H_MODULE, H_ITER };
struct VpiObject {
    HKind kind;
    cxxrtl_vpi::Signal *sig = nullptr;    // H_SIGNAL
    std::string name;                     // H_MODULE: scope name
    std::vector<vpiHandle> items;         // H_ITER: handles to hand out
    size_t pos = 0;                       // H_ITER: scan cursor
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

VpiObject *make_signal(cxxrtl_vpi::Signal *s) {
    auto *o = new VpiObject;
    o->kind = H_SIGNAL;
    o->sig = s;
    return o;
}
VpiObject *make_module(const std::string &name) {
    auto *o = new VpiObject;
    o->kind = H_MODULE;
    o->name = name;
    return o;
}
VpiObject *make_iter(std::vector<vpiHandle> items) {
    auto *o = new VpiObject;
    o->kind = H_ITER;
    o->items = std::move(items);
    return o;
}

// A top-level signal is one with no hierarchy separator in its name.
bool is_top_level(const cxxrtl_vpi::Signal &s) {
    return s.name.find('.') == std::string::npos;
}
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
            DBG("value-change: %s %u->%u @t=%llu", o->watch->name.c_str(),
                o->last.empty() ? 0 : o->last[0], cur.empty() ? 0 : cur[0],
                (unsigned long long)g_time);
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
    if (!due.empty())
        DBG("timed: firing %zu cb(s) @t=%llu", due.size(),
            (unsigned long long)g_time);
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
    g_debug = std::getenv("CXXRTL_VPI_DEBUG") != nullptr;

    DBG("start: %zu start cbs, %zu timed, %zu value", g_startsim.size(),
        g_timed.size(), g_value.size());
    fire_oneshot(g_startsim);
    // Flush writes queued during startup (e.g. initial reset), then settle.
    fire_oneshot(g_rw);
    settle();

    while (!g_finished) {
        // Read-only sampling region for the settled current state.
        fire_oneshot(g_ro);

        uint64_t next = next_deadline();
        DBG("t=%llu: next=%llu timed=%zu value=%zu rw=%zu",
            (unsigned long long)g_time, (unsigned long long)next,
            g_timed.size(), g_value.size(), g_rw.size());
        if (next == NO_DEADLINE)
            break;  // no more events scheduled
        g_time = next;

        // Advance: NextSimTime, then timed callbacks (cocotb coroutines resume
        // here and queue writes), then flush those writes in the ReadWrite
        // region and settle so value-change callbacks see the new values.
        fire_oneshot(g_nextsim);
        fire_timed_due();
        settle();
        fire_oneshot(g_rw);
        settle();
    }

    DBG("done at t=%llu (finished=%d)", (unsigned long long)g_time, g_finished);
    fire_oneshot(g_endsim);
}

}  // namespace cxxrtl_vpi

// ===========================================================================
// VPI: object access
// ===========================================================================

vpiHandle vpi_handle_by_name(PLI_BYTE8 *name, vpiHandle scope) {
    (void)scope;  // flat lookup; both "clk" and "<top>.clk" are accepted below.
    if (!g_model || !name)
        return nullptr;

    std::string n(name);
    cxxrtl_vpi::Signal *sig = g_model->by_name(n);
    if (!sig) {
        // cocotb often qualifies with the toplevel ("counter.clk"); our enum
        // names top ports without that prefix, so strip a leading "<top>.".
        const std::string &top = g_model->top_name();
        if (!top.empty() && n.rfind(top + ".", 0) == 0)
            sig = g_model->by_name(n.substr(top.size() + 1));
    }
    if (!sig)
        return nullptr;
    return obj_handle(make_signal(sig));
}

// Discover the root (toplevel) module: vpi_iterate(vpiModule, NULL).
vpiHandle vpi_iterate(PLI_INT32 type, vpiHandle refHandle) {
    if (!g_model)
        return nullptr;

    if (type == vpiModule && refHandle == nullptr) {
        std::vector<vpiHandle> items{obj_handle(make_module(g_model->top_name()))};
        return obj_handle(make_iter(std::move(items)));
    }

    // Children of the root module. CXXRTL's capi doesn't cleanly separate
    // nets from regs, so for the MVP we return all top-level signals under
    // vpiNet (and nothing under vpiReg) to avoid double-listing. TODO: refine
    // net/reg classification via cxxrtl_flag once needed.
    VpiObject *ref = as_obj(refHandle);
    if (type == vpiNet && ref && ref->kind == H_MODULE) {
        std::vector<vpiHandle> items;
        for (const auto &s : g_model->signals())
            if (is_top_level(s))
                items.push_back(obj_handle(
                    make_signal(const_cast<cxxrtl_vpi::Signal *>(&s))));
        return obj_handle(make_iter(std::move(items)));
    }
    return nullptr;
}

vpiHandle vpi_scan(vpiHandle iterator) {
    VpiObject *it = as_obj(iterator);
    if (!it || it->kind != H_ITER)
        return nullptr;
    if (it->pos >= it->items.size()) {
        delete it;  // VPI auto-frees an iterator once exhausted
        return nullptr;
    }
    return it->items[it->pos++];
}

vpiHandle vpi_handle(PLI_INT32 type, vpiHandle refHandle) {
    (void)type;
    (void)refHandle;
    // TODO: parent/scope navigation if a client needs it.
    return nullptr;
}

vpiHandle vpi_handle_by_index(vpiHandle object, PLI_INT32 index) {
    (void)object;
    (void)index;
    // TODO: bit/element select. Null is a valid "not found" response.
    return nullptr;
}

PLI_INT32 vpi_get_vlog_info(p_vpi_vlog_info vlog_info_p) {
    if (!vlog_info_p)
        return 0;
    static char product[] = "cxxrtl-vpi";
    static char version[] = "0.0.0";
    static char arg0[] = "cxxrtl-vpi";
    static char *argv[] = {arg0};
    vlog_info_p->argc = 1;
    vlog_info_p->argv = argv;
    vlog_info_p->product = product;
    vlog_info_p->version = version;
    return 1;
}

PLI_INT32 vpi_chk_error(p_vpi_error_info error_info_p) {
    // We never raise VPI errors yet; report "no error".
    if (error_info_p)
        error_info_p->level = 0;
    return 0;
}

PLI_INT32 vpi_get(PLI_INT32 property, vpiHandle object) {
    // Global time properties are queried with a NULL object. We run on an
    // integer time axis in nanoseconds, so report precision/unit = -9. cocotb
    // uses this to scale Timer/Clock values onto our scheduler's units.
    if (property == vpiTimePrecision || property == vpiTimeUnit)
        return -9;

    VpiObject *o = as_obj(object);
    if (!o)
        return 0;
    if (o->kind == H_MODULE)
        return property == vpiType ? vpiModule : 0;
    if (o->kind != H_SIGNAL)
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

    if (o->kind == H_MODULE) {
        if (property == vpiName || property == vpiFullName) {
            buf = o->name;
            return const_cast<PLI_BYTE8 *>(buf.c_str());
        }
        return nullptr;
    }
    if (o->kind != H_SIGNAL)
        return nullptr;

    switch (property) {
        case vpiFullName: {
            // Present signals under the toplevel scope cocotb expects.
            const std::string &top = g_model->top_name();
            buf = top.empty() ? o->sig->name : top + "." + o->sig->name;
            return const_cast<PLI_BYTE8 *>(buf.c_str());
        }
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
    if (!o || o->kind != H_SIGNAL || !value_p)
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
    if (!o || o->kind != H_SIGNAL || !value_p)
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

    DBG("put_value: %s = %u (fmt=%d) @t=%llu", o->sig->name.c_str(),
        val.empty() ? 0 : val[0], value_p->format, (unsigned long long)g_time);
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
    DBG("register_cb reason=%d @t=%llu", cb_data_p->reason,
        (unsigned long long)g_time);

    switch (cb_data_p->reason) {
        case cbAfterDelay:
            o->deadline = g_time + time_from(cb_data_p->time);
            g_timed.push_back(o);
            break;
        case cbValueChange: {
            VpiObject *vobj = as_obj(cb_data_p->obj);
            if (!vobj || vobj->kind != H_SIGNAL) {
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
