// SPDX-License-Identifier: ISC
//
// Thin C++ wrapper around a CXXRTL design accessed via cxxrtl_capi.
//
// One Model owns a single cxxrtl_handle for the whole simulation. It builds a
// name -> signal table once (via cxxrtl_enum) so the VPI layer can resolve
// handles by name and iterate the hierarchy without re-walking the design.

#ifndef CXXRTL_VPI_MODEL_H
#define CXXRTL_VPI_MODEL_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <cxxrtl/capi/cxxrtl_capi.h>

// Provided by the generated model (write_cxxrtl emits this with extern "C"):
//     extern "C" cxxrtl_toplevel cxxrtl_design_create();
extern "C" cxxrtl_toplevel cxxrtl_design_create();

namespace cxxrtl_vpi {

// A single design object (wire/reg/...) discovered via cxxrtl_enum.
struct Signal {
    std::string name;        // full hierarchical name, '.' separated
    cxxrtl_object *object;    // owned by the cxxrtl runtime, not by us
    size_t width;             // in bits
    uint32_t type;            // enum cxxrtl_type
    uint32_t flags;           // bit mask of enum cxxrtl_flag (input/output/...)
};

class Model {
public:
    Model() = default;
    ~Model();

    // Instantiate the generated design and enumerate its signals.
    void create();

    // Resolve a signal by full name, or nullptr if unknown.
    Signal *by_name(const std::string &name);

    // All signals, in discovery order (for vpi_iterate / hierarchy walk).
    const std::vector<Signal> &signals() const { return signals_; }

    // Read current value into out (one bit per uint32_t chunk packed, LSB
    // first). Returns width in bits. TODO: pack to s_vpi_vecval in the VPI layer.
    size_t read(const Signal &sig, std::vector<uint32_t> &out) const;

    // Stage a write into the signal's `next` buffer. Applied on commit().
    void write(const Signal &sig, const std::vector<uint32_t> &value);

    // Settle combinational logic (cxxrtl_eval) after staged writes.
    int eval() { return cxxrtl_eval(handle_); }

    // Latch `next` -> `curr` for stateful elements (cxxrtl_commit).
    int commit() { return cxxrtl_commit(handle_); }

    // Advance one step (eval+commit until stable); returns delta cycle count.
    size_t step() { return cxxrtl_step(handle_); }

    cxxrtl_handle handle() const { return handle_; }

private:
    cxxrtl_handle handle_ = nullptr;
    std::vector<Signal> signals_;
    std::map<std::string, size_t> by_name_;  // name -> index into signals_

    static void enum_cb(void *data, const char *name,
                        cxxrtl_object *object, size_t parts);
};

}  // namespace cxxrtl_vpi

#endif  // CXXRTL_VPI_MODEL_H
