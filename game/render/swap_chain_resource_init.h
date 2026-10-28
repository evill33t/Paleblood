// SPDX-License-Identifier: GPL-2.0-or-later
// Observed resource-wrapper initialization wire. Lower service/ownership effects
// remain original; these field names do not imply GPU formats or resource kinds.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/original.h"

namespace render {
template <typename T> struct SwapResourceField {
    uint8_t bytes[sizeof(T)];
    T get() const { T value; std::memcpy(&value, bytes, sizeof(value)); return value; }
    void set(T value) { std::memcpy(bytes, &value, sizeof(value)); }
};

struct SwapResourceOwner;
struct SwapResourceObject;
using SwapResourceAllocate = SwapResourceObject *(*)(SwapResourceOwner *, uint64_t, uint64_t);
using SwapResourceRelease = uintptr_t (*)(SwapResourceOwner *, uintptr_t);
struct SwapResourceOwnerVtable {
    uint8_t unknown_00[0x58];
    SwapResourceField<SwapResourceAllocate> allocate;
    uint8_t unknown_60[0x10];
    SwapResourceField<SwapResourceRelease> release;
};
struct SwapResourceOwner {
    SwapResourceField<SwapResourceOwnerVtable *> vtable;
};

struct SwapResourceServices {
    SwapResourceField<uint64_t> capability_00;
    SwapResourceField<uint64_t> capability_08;
    uint8_t unknown_10[0x10];
    SwapResourceField<uint64_t> capability_20;
    uint8_t unknown_28[0xdc10 - 0x28];
    SwapResourceField<SwapResourceOwner *> default_allocator;
    uint8_t unknown_dc18[8];
    SwapResourceField<SwapResourceOwner *> default_owner;
};
struct SwapResourceParameters {
    SwapResourceField<uint32_t> parameter_00;
    SwapResourceField<uint32_t> parameter_04;
    SwapResourceField<uint32_t> parameter_08;
    SwapResourceField<uint32_t> parameter_0c;
    uint8_t unknown_10[4];
    SwapResourceField<uint32_t> parameter_14;
    uint8_t unknown_18[4];
    SwapResourceField<uint32_t> kind;
    SwapResourceField<uint32_t> flags;
    SwapResourceField<uint32_t> selection;
    SwapResourceField<uint32_t> secondary_flags;
};
struct SwapResourceInitialEntry {
    uint8_t unknown_00[8];
    SwapResourceField<uint32_t> value;
    uint8_t unknown_0c[4];
};
struct SwapResourceInfo {
    SwapResourceField<SwapResourceOwner *> owner;
    SwapResourceField<uintptr_t> resource;
    SwapResourceField<uint64_t> value_10;
    SwapResourceField<uint64_t> value_18;
    SwapResourceField<uint64_t> value_20;
    SwapResourceField<uint64_t> value_28;
    SwapResourceField<uint64_t> value_30;
};
struct SwapResourceObject {
    SwapResourceField<uintptr_t> vtable;
    SwapResourceField<SwapResourceOwner *> allocator;
    SwapResourceField<uint32_t> state_10;
    uint8_t unknown_14[4];
    SwapResourceInfo info;
};
struct SwapResourceOutput {
    SwapResourceField<SwapResourceObject *> object;
};

// Preserve the exposed local-region relationships, not a compiler's physical
// frame: native RSP=RBP-0x80, info=RBP-0x68, saved guard=RBP-0x30.
// The info bytes are intentionally uninitialized before the original lower call.
struct alignas(8) SwapResourceInitFrame {
    SwapResourceField<uint64_t> outgoing_argument;
    SwapResourceField<SwapResourceOwner *> default_allocator;
    SwapResourceField<SwapResourceOwner *> default_owner;
    SwapResourceInfo info;
    SwapResourceField<uint64_t> saved_guard;
};

static_assert(sizeof(uintptr_t) == 8);
static_assert(sizeof(SwapResourceAllocate) == 8);
static_assert(sizeof(SwapResourceRelease) == 8);
static_assert(offsetof(SwapResourceOwnerVtable, allocate) == 0x58);
static_assert(offsetof(SwapResourceOwnerVtable, release) == 0x70);
static_assert(offsetof(SwapResourceOwner, vtable) == 0);
static_assert(offsetof(SwapResourceServices, capability_08) == 8);
static_assert(offsetof(SwapResourceServices, capability_20) == 0x20);
static_assert(offsetof(SwapResourceServices, default_allocator) == 0xdc10);
static_assert(offsetof(SwapResourceServices, default_owner) == 0xdc20);
static_assert(offsetof(SwapResourceParameters, parameter_14) == 0x14);
static_assert(offsetof(SwapResourceParameters, kind) == 0x1c);
static_assert(offsetof(SwapResourceParameters, flags) == 0x20);
static_assert(offsetof(SwapResourceParameters, selection) == 0x24);
static_assert(offsetof(SwapResourceParameters, secondary_flags) == 0x28);
static_assert(sizeof(SwapResourceParameters) == 0x2c);
static_assert(offsetof(SwapResourceInitialEntry, value) == 8);
static_assert(sizeof(SwapResourceInitialEntry) == 0x10);
static_assert(offsetof(SwapResourceInfo, resource) == 8);
static_assert(offsetof(SwapResourceInfo, value_10) == 0x10);
static_assert(offsetof(SwapResourceInfo, value_18) == 0x18);
static_assert(offsetof(SwapResourceInfo, value_20) == 0x20);
static_assert(offsetof(SwapResourceInfo, value_28) == 0x28);
static_assert(offsetof(SwapResourceInfo, value_30) == 0x30);
static_assert(sizeof(SwapResourceInfo) == 0x38);
static_assert(offsetof(SwapResourceObject, allocator) == 8);
static_assert(offsetof(SwapResourceObject, state_10) == 0x10);
static_assert(offsetof(SwapResourceObject, unknown_14) == 0x14);
static_assert(offsetof(SwapResourceObject, info) == 0x18);
static_assert(sizeof(SwapResourceObject) == 0x50);
static_assert(sizeof(SwapResourceOutput) == 8);
static_assert(offsetof(SwapResourceInitFrame, default_allocator) == 8);
static_assert(offsetof(SwapResourceInitFrame, default_owner) == 0x10);
static_assert(offsetof(SwapResourceInitFrame, info) == 0x18);
static_assert(offsetof(SwapResourceInitFrame, saved_guard) == 0x50);
static_assert(sizeof(SwapResourceInitFrame) == 0x58);

namespace swap_resource_calls {
RT_ORIGINAL(0x02571d60, prepare_resource_info,
            uint32_t(SwapResourceInfo *, SwapResourceServices *, SwapResourceParameters *,
                     SwapResourceInitialEntry *, SwapResourceOwner *, SwapResourceOwner *,
                     uint64_t));
RT_ORIGINAL(0x02fbe4f8, fail_guard, void());
RT_GLOBAL(0x057e3be0, guard_pointer, const uint64_t *);
RT_GLOBAL(0x057b6240, initial_vtable, uint8_t);
RT_GLOBAL(0x0575f3e0, final_vtable, uint8_t);
}  // namespace swap_resource_calls
}  // namespace render

// Full RCX optional-array pointer and full64 seventh stack word are preserved.
// Normal return is uint32 status (native EAX zeroextension). Profile covers a
// matching guard or a nonreturning original failure import, not its unknown
// returning continuation, callback ambient state or full native-frame identity.
extern "C" uint64_t bb_render_initialize_swap_chain_resource(
    render::SwapResourceServices *, render::SwapResourceOutput *,
    render::SwapResourceParameters *, render::SwapResourceInitialEntry *,
    render::SwapResourceOwner *, render::SwapResourceOwner *, uint64_t);
