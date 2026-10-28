// SPDX-License-Identifier: GPL-2.0-or-later
// Resource-info preparation wire. Original lower calls retain unclosed service,
// ownership and descriptor effects; these names do not assert GPU semantics.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/original.h"

namespace render {
template <typename T> struct ResourceInfoField {
    uint8_t bytes[sizeof(T)];
    T get() const { T value; std::memcpy(&value, bytes, sizeof(value)); return value; }
    void set(T value) { std::memcpy(bytes, &value, sizeof(value)); }
};
struct ResourceInfoOwner;
struct ResourceInfoControl;
struct ResourceInfoProcessor;
using ResourceInfoAllocate = uintptr_t (*)(ResourceInfoOwner *, uint64_t, uint64_t);
using ResourceInfoRelease = uintptr_t (*)(ResourceInfoOwner *, uintptr_t);
using ResourceInfoControl18 = uintptr_t (*)(ResourceInfoControl *, uint64_t);
using ResourceInfoControl28 = uintptr_t (*)(ResourceInfoControl *);
struct ResourceInfoOwnerVtable {
    uint8_t unknown_00[0x58];
    ResourceInfoField<ResourceInfoAllocate> allocate;
    uint8_t unknown_60[0x10];
    ResourceInfoField<ResourceInfoRelease> release;
};
struct ResourceInfoOwner { ResourceInfoField<ResourceInfoOwnerVtable *> vtable; };
struct ResourceInfoControlVtable {
    uint8_t unknown_00[0x18];
    ResourceInfoField<ResourceInfoControl18> operation_18;
    uint8_t unknown_20[8];
    ResourceInfoField<ResourceInfoControl28> operation_28;
};
struct ResourceInfoControl { ResourceInfoField<ResourceInfoControlVtable *> vtable; };
struct ResourceInfoServices {
    uint8_t unknown_00[0x40];
    uint8_t processor_address;
    uint8_t unknown_41[0x118 - 0x41];
    ResourceInfoField<ResourceInfoControl *> control;
};
struct ResourceInfoParameters {
    ResourceInfoField<uint32_t> words[11];
};
struct ResourceInfoOptions {
    ResourceInfoField<uint8_t> capacity_enabled;
    ResourceInfoField<uint8_t> span_copy_enabled;
    ResourceInfoField<uint8_t> payload_enabled;
    ResourceInfoField<uint8_t> alternate_path;
    ResourceInfoField<uint32_t> capacity;
    ResourceInfoField<uint32_t> alignment;
    uint8_t payload_address;
};
struct ResourceInfoOptionsSlot { ResourceInfoField<ResourceInfoOptions *> value; };
struct ResourceInfoInitialEntry {
    ResourceInfoField<uintptr_t> data;
    ResourceInfoField<uint32_t> value;
    uint8_t unknown_0c[4];
};
struct ResourceInfoDescriptor { uint8_t bytes[0x20]; };
struct ResourceInfoLevelDescriptor { uint8_t bytes[0x30]; };
struct ResourceInfoRequirements {
    ResourceInfoField<uint32_t> bytes;
    ResourceInfoField<uint32_t> alignment;
};
struct ResourceInfoOutput {
    ResourceInfoField<ResourceInfoOwner *> owner;
    ResourceInfoField<uintptr_t> allocation;
    ResourceInfoField<uint32_t> mode;
    ResourceInfoField<uint32_t> required_bytes;
    ResourceInfoField<uint32_t> allocation_bytes;
    ResourceInfoField<uint32_t> alignment;
    ResourceInfoField<uint8_t> kind;
    ResourceInfoField<uint8_t> parameter_1c;
    ResourceInfoField<uint8_t> parameter_24;
    ResourceInfoField<uint8_t> flags;
    ResourceInfoField<uint32_t> secondary_flags;
    ResourceInfoField<uint32_t> classification;
    ResourceInfoField<uint16_t> first;
    ResourceInfoField<uint16_t> second;
    ResourceInfoField<uint16_t> count;
    ResourceInfoField<uint8_t> levels;
    ResourceInfoField<uint8_t> selector;
    ResourceInfoField<uint16_t> parameter_14;
    ResourceInfoField<uint16_t> parameter_18;
};

// Base corresponds to native RBP-0xe8. Preserve exposed relative bytes, not a
// compiler-generated physical frame. Only explicit native stores initialize it.
struct alignas(8) ResourceInfoFrame {
    ResourceInfoField<uintptr_t> saved_allocation;
    ResourceInfoField<uint32_t> alignment;
    ResourceInfoField<uint32_t> allocation_flag;
    ResourceInfoField<uint64_t> levels;
    ResourceInfoField<uint64_t> required_bytes;
    uint8_t unknown_20[4];
    ResourceInfoField<uint32_t> level_count;
    ResourceInfoField<ResourceInfoServices *> services;
    ResourceInfoField<uint64_t> output_index_or_destination;
    ResourceInfoField<ResourceInfoParameters *> parameters;
    ResourceInfoField<uintptr_t> owner_or_processor;
    ResourceInfoField<ResourceInfoInitialEntry *> initial;
    ResourceInfoLevelDescriptor temporary_descriptor;
    uint8_t size_slot[8];
    ResourceInfoField<uint64_t> offset;
    ResourceInfoRequirements requirements;
    ResourceInfoDescriptor descriptor;
    ResourceInfoField<uint64_t> saved_guard;
};
static_assert(sizeof(uintptr_t) == 8);
static_assert(offsetof(ResourceInfoOwnerVtable, allocate) == 0x58);
static_assert(offsetof(ResourceInfoOwnerVtable, release) == 0x70);
static_assert(offsetof(ResourceInfoControlVtable, operation_18) == 0x18);
static_assert(offsetof(ResourceInfoControlVtable, operation_28) == 0x28);
static_assert(offsetof(ResourceInfoServices, processor_address) == 0x40);
static_assert(offsetof(ResourceInfoServices, control) == 0x118);
static_assert(sizeof(ResourceInfoParameters) == 0x2c);
static_assert(offsetof(ResourceInfoOptions, capacity) == 4);
static_assert(offsetof(ResourceInfoOptions, alignment) == 8);
static_assert(offsetof(ResourceInfoOptions, payload_address) == 0xc);
static_assert(sizeof(ResourceInfoOptionsSlot) == 8);
static_assert(sizeof(ResourceInfoInitialEntry) == 0x10);
static_assert(sizeof(ResourceInfoRequirements) == 8);
static_assert(offsetof(ResourceInfoOutput, allocation) == 8);
static_assert(offsetof(ResourceInfoOutput, mode) == 0x10);
static_assert(offsetof(ResourceInfoOutput, required_bytes) == 0x14);
static_assert(offsetof(ResourceInfoOutput, allocation_bytes) == 0x18);
static_assert(offsetof(ResourceInfoOutput, alignment) == 0x1c);
static_assert(offsetof(ResourceInfoOutput, kind) == 0x20);
static_assert(offsetof(ResourceInfoOutput, secondary_flags) == 0x24);
static_assert(offsetof(ResourceInfoOutput, classification) == 0x28);
static_assert(offsetof(ResourceInfoOutput, first) == 0x2c);
static_assert(offsetof(ResourceInfoOutput, levels) == 0x32);
static_assert(offsetof(ResourceInfoOutput, parameter_18) == 0x36);
static_assert(sizeof(ResourceInfoOutput) == 0x38);
static_assert(offsetof(ResourceInfoFrame, alignment) == 8);
static_assert(offsetof(ResourceInfoFrame, levels) == 0x10);
static_assert(offsetof(ResourceInfoFrame, level_count) == 0x24);
static_assert(offsetof(ResourceInfoFrame, services) == 0x28);
static_assert(offsetof(ResourceInfoFrame, output_index_or_destination) == 0x30);
static_assert(offsetof(ResourceInfoFrame, parameters) == 0x38);
static_assert(offsetof(ResourceInfoFrame, owner_or_processor) == 0x40);
static_assert(offsetof(ResourceInfoFrame, initial) == 0x48);
static_assert(offsetof(ResourceInfoFrame, temporary_descriptor) == 0x50);
static_assert(offsetof(ResourceInfoFrame, size_slot) == 0x80);
static_assert(offsetof(ResourceInfoFrame, offset) == 0x88);
static_assert(offsetof(ResourceInfoFrame, requirements) == 0x90);
static_assert(offsetof(ResourceInfoFrame, descriptor) == 0x98);
static_assert(offsetof(ResourceInfoFrame, saved_guard) == 0xb8);
static_assert(sizeof(ResourceInfoFrame) == 0xc0);

namespace resource_info_calls {
// Constructed call-site profiles only. The sixth descriptor-preparation GP is
// provably inherited incoming candidateB; its semantic consumption is unclosed.
RT_ORIGINAL(0x02fa6af0, prepare_descriptor,
            uint32_t(ResourceInfoRequirements *, ResourceInfoDescriptor *, ResourceInfoParameters *,
                     void *, ResourceInfoInitialEntry *, ResourceInfoOwner *));
RT_ORIGINAL(0x01489910, descriptor_levels, uint32_t(ResourceInfoDescriptor *));
RT_ORIGINAL(0x02abaef0, current_mode, uint32_t());
RT_ORIGINAL(0x014898f0, descriptor_classification, uint32_t(ResourceInfoDescriptor *));
RT_ORIGINAL(0x02abb4b0, needs_initial_copy, uint8_t(uint32_t));
RT_ORIGINAL(0x02a1a1b0, copy_initial_span, uintptr_t(uintptr_t, uintptr_t, uint64_t));
RT_ORIGINAL(0x02abadd0, storage_admitted, uint8_t(uintptr_t));
RT_ORIGINAL(0x02ab4230, process_initial_entry,
            uintptr_t(ResourceInfoProcessor *, ResourceInfoOutput *, ResourceInfoInitialEntry *,
                      uint64_t, uint64_t));
RT_ORIGINAL(0x02ab1930, finish_processing_first, uintptr_t(ResourceInfoProcessor *));
RT_ORIGINAL(0x02ab18c0, finish_processing_second, uintptr_t(ResourceInfoProcessor *));
RT_ORIGINAL(0x02abad70, alternate_copy_admitted, uint8_t(uintptr_t));
RT_ORIGINAL(0x014a2c40, calculate_level_offset,
            uint32_t(ResourceInfoField<uint64_t> *, void *, ResourceInfoDescriptor *, uint32_t, uint32_t));
RT_ORIGINAL(0x014a1e80, initialize_level_descriptor,
            uintptr_t(ResourceInfoLevelDescriptor *, ResourceInfoDescriptor *, uint32_t, uint32_t));
RT_ORIGINAL(0x014a3410, copy_initial_level,
            uint32_t(uintptr_t, uintptr_t, ResourceInfoLevelDescriptor *));
RT_ORIGINAL(0x02abb4a0, note_offset_failure, uintptr_t(uint32_t));
RT_ORIGINAL(0x02fbe4f8, fail_guard, void());
RT_GLOBAL(0x057e3be0, guard_pointer, const uint64_t *);
}  // namespace resource_info_calls
}  // namespace render

// Native entry keeps the original seventh-slot address live across callbacks.
// Explicit body supports a supplied live slot; it does not reproduce the entire
// native physical frame, extra ambient registers or returning-failure path.
extern "C" uint64_t bb_render_prepare_swap_chain_resource_info_body(
    render::ResourceInfoOutput *, render::ResourceInfoServices *, render::ResourceInfoParameters *,
    render::ResourceInfoInitialEntry *, render::ResourceInfoOwner *, render::ResourceInfoOwner *,
    const render::ResourceInfoOptionsSlot *);
extern "C" uint64_t bb_render_prepare_swap_chain_resource_info(
    render::ResourceInfoOutput *, render::ResourceInfoServices *, render::ResourceInfoParameters *,
    render::ResourceInfoInitialEntry *, render::ResourceInfoOwner *, render::ResourceInfoOwner *,
    render::ResourceInfoOptions *);
