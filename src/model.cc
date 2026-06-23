// SPDX-License-Identifier: ISC
//
// Model: cxxrtl_capi-backed design wrapper. See include/cxxrtl_vpi/model.h.

#include "cxxrtl_vpi/model.h"

#include <cstring>

namespace cxxrtl_vpi {

Model::~Model() {
    if (handle_) {
        cxxrtl_destroy(handle_);
        handle_ = nullptr;
    }
}

void Model::enum_cb(void *data, const char *name,
                    cxxrtl_object *object, size_t parts) {
    auto *self = static_cast<Model *>(data);

    // cxxrtl_enum reports each object once; `parts` > 1 for split (memory)
    // objects. For the MVP we register scalar/vector wires and regs.
    Signal sig;
    sig.name = name;
    sig.object = object;
    sig.width = object->width;
    sig.type = object->type;
    sig.flags = object->flags;

    self->by_name_[sig.name] = self->signals_.size();
    self->signals_.push_back(sig);
}

void Model::create() {
    handle_ = cxxrtl_create(cxxrtl_design_create());
    // Build the signal table once.
    cxxrtl_enum(handle_, this, &Model::enum_cb);
}

Signal *Model::by_name(const std::string &name) {
    auto it = by_name_.find(name);
    if (it == by_name_.end())
        return nullptr;
    return &signals_[it->second];
}

size_t Model::read(const Signal &sig, std::vector<uint32_t> &out) const {
    // cxxrtl_object stores `width` bits across ceil(width/32) chunks in `curr`.
    const size_t chunks = (sig.width + 31) / 32;
    out.assign(chunks, 0);
    if (sig.object->curr)
        std::memcpy(out.data(), sig.object->curr, chunks * sizeof(uint32_t));
    // TODO: mask the top chunk to `width` bits.
    return sig.width;
}

void Model::write(const Signal &sig, const std::vector<uint32_t> &value) {
    const size_t chunks = (sig.width + 31) / 32;
    if (!sig.object->next)
        return;  // not a writable object
    // TODO: bounds-check value.size() against chunks; mask top chunk.
    std::memcpy(sig.object->next, value.data(),
                std::min(chunks, value.size()) * sizeof(uint32_t));
}

}  // namespace cxxrtl_vpi
