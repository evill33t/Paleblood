// SPDX-License-Identifier: GPL-2.0-or-later
// Two setup cleanup records share this leading wire layout. The virtual call's
// ownership/service meaning and any additional ambient inputs remain unclosed.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace render {
template <typename T> struct SwapChainCleanupField {
    uint8_t bytes[sizeof(T)];
    T get() const { T value; std::memcpy(&value, bytes, sizeof(value)); return value; }
    void set(T value) { std::memcpy(bytes, &value, sizeof(value)); }
};

struct SwapChainCleanupOwner;
using SwapChainCleanupCallback = uint64_t (*)(SwapChainCleanupOwner *, uintptr_t);

struct SwapChainCleanupVtable {
    uint8_t unknown_00[0x70];
    SwapChainCleanupField<SwapChainCleanupCallback> callback;
};
struct SwapChainCleanupOwner {
    SwapChainCleanupField<SwapChainCleanupVtable *> vtable;
};
struct SwapChainCleanupRecord {
    SwapChainCleanupField<SwapChainCleanupOwner *> owner;
    SwapChainCleanupField<uintptr_t> resource;
};

static_assert(sizeof(uintptr_t) == 8);
static_assert(sizeof(SwapChainCleanupCallback) == 8);
static_assert(offsetof(SwapChainCleanupVtable, callback) == 0x70);
static_assert(offsetof(SwapChainCleanupOwner, vtable) == 0);
static_assert(offsetof(SwapChainCleanupRecord, owner) == 0);
static_assert(offsetof(SwapChainCleanupRecord, resource) == 8);
static_assert(sizeof(SwapChainCleanupRecord) == 0x10);

inline uint64_t clear_swap_chain_cleanup_record(SwapChainCleanupRecord *record,
                                                uint64_t incoming_rax)
{
    // There is no native null-record guard. A null owner skips the resource read;
    // a null resource skips the vtable read. Both routes preserve incoming RAX.
    SwapChainCleanupOwner *owner = record->owner.get();
    if (owner == nullptr) return incoming_rax;
    const uintptr_t resource = record->resource.get();
    if (resource == 0) return incoming_rax;
    SwapChainCleanupVtable *vtable = owner->vtable.get();
    const SwapChainCleanupCallback callback = vtable->callback.get();
    const uint64_t result = callback(owner, resource);
    // Use the original record after the call. This overwrites a callback change
    // to resource, but leaves owner and all other bytes as the callback left them.
    record->resource.set(0);
    return result;
}
}  // namespace render

// Explicit-input bodies are also usable without an AMD64 native entry. These
// preserve the established GP/result profile, not unknown callback ambient state
// or the original physical stack frame. No source has been compiled or validated.
extern "C" uint64_t bb_render_clear_swap_chain_curve_body(
    render::SwapChainCleanupRecord *, uint64_t incoming_rax);
extern "C" uint64_t bb_render_clear_swap_chain_cached_record_body(
    render::SwapChainCleanupRecord *, uint64_t incoming_rax);

// The one-argument native entries capture entry RAX on AMD64. Other architectures
// stop explicitly; a by-value C++ call alone cannot supply that ambient register.
extern "C" uint64_t bb_render_clear_swap_chain_curve(render::SwapChainCleanupRecord *);
extern "C" uint64_t bb_render_clear_swap_chain_cached_record(render::SwapChainCleanupRecord *);
