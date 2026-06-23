// SPDX-License-Identifier: ISC
//
// IEEE-1364 VPI provider implemented on top of cxxrtl_capi.
//
// This is the heart of the project: it makes a CXXRTL model look like a
// VPI-speaking simulator, so a VPI client (cocotb's libcocotbvpi first) can
// drive it. See docs/vpi-coverage.md for the surface and status.
//
// MVP IMPLEMENTED: object access (handle-by-name, get/put value, get, get_str,
// free_object). Callbacks / time / control are the next stage (still stubs in
// the harness).

#include <vpi_user.h>

#include <cstring>
#include <string>
#include <vector>

#include "cxxrtl_vpi/model.h"

namespace {

// The single design instance, owned by the harness (see harness.cc).
cxxrtl_vpi::Model *g_model = nullptr;

// A VPI handle is an opaque pointer; we back it with this wrapper.
struct VpiObject {
    cxxrtl_vpi::Signal *sig;
};

VpiObject *as_obj(vpiHandle h) { return reinterpret_cast<VpiObject *>(h); }
vpiHandle as_handle(VpiObject *o) { return reinterpret_cast<vpiHandle>(o); }

}  // namespace

namespace cxxrtl_vpi {
// Called by the harness right after Model::create(), before VPI startup.
void vpi_provider_bind(Model *model) { g_model = model; }
}  // namespace cxxrtl_vpi

// ---------------------------------------------------------------------------
// Object access
// ---------------------------------------------------------------------------

vpiHandle vpi_handle_by_name(PLI_BYTE8 *name, vpiHandle scope) {
    (void)scope;  // TODO: honour scope once hierarchy iteration lands.
    if (!g_model || !name)
        return nullptr;
    cxxrtl_vpi::Signal *sig = g_model->by_name(name);
    if (!sig)
        return nullptr;
    return as_handle(new VpiObject{sig});
}

PLI_INT32 vpi_free_object(vpiHandle object) {
    delete as_obj(object);
    return 1;
}

PLI_INT32 vpi_get(PLI_INT32 property, vpiHandle object) {
    VpiObject *o = as_obj(object);
    if (!o)
        return 0;
    switch (property) {
        case vpiSize:
            return static_cast<PLI_INT32>(o->sig->width);
        case vpiType:
            // CXXRTL doesn't distinguish net vs reg at the capi level here;
            // report writable objects as reg, others as net. Good enough for
            // cocotb's purposes. TODO: refine via cxxrtl_flag.
            return o->sig->object->next ? vpiReg : vpiNet;
        default:
            return 0;
    }
}

PLI_BYTE8 *vpi_get_str(PLI_INT32 property, vpiHandle object) {
    VpiObject *o = as_obj(object);
    if (!o)
        return nullptr;
    // VPI strings are owned by the provider, valid until the next call.
    static thread_local std::string buf;
    switch (property) {
        case vpiFullName:
            buf = o->sig->name;
            return const_cast<PLI_BYTE8 *>(buf.c_str());
        case vpiName: {
            // Local name = last '.'-separated component of the full name.
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
            value_p->value.integer =
                chunks ? static_cast<PLI_INT32>(bits[0]) : 0;
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
            // Unsupported format requested. TODO: vpiHexStrVal, vpiRealVal, ...
            break;
    }
}

vpiHandle vpi_put_value(vpiHandle object, p_vpi_value value_p,
                        p_vpi_time time_p, PLI_INT32 flags) {
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
            // Unsupported format. TODO: string/hex formats.
            return nullptr;
    }

    // Stage the write into `next`. The harness latches it (cxxrtl_commit) and
    // settles (cxxrtl_eval/step) on the next time advance; for the MVP the test
    // drives stepping explicitly.
    g_model->write(*o->sig, val);
    return object;
}
