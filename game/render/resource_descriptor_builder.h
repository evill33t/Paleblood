// SPDX-License-Identifier: GPL-2.0-or-later
// Descriptor preparation used by resource-info initialization. Lower functions
// remain originals; these views describe observed wires, not GPU formats.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/original.h"

namespace render {
template <typename T> struct ResourceDescriptorField {
    uint8_t bytes[sizeof(T)];
    T get() const { T value; std::memcpy(&value, bytes, sizeof(value)); return value; }
    void set(T value) { std::memcpy(bytes, &value, sizeof(value)); }
};
struct ResourceDescriptorParameters { uint8_t bytes[0x30]; };
struct ResourceDescriptorOptional { ResourceDescriptorField<uint32_t> value; };
struct ResourceDescriptorStorage { uint8_t bytes[0x20]; };
struct ResourceDescriptorLevel { uint8_t bytes[0x30]; };
struct ResourceDescriptorRequirements { ResourceDescriptorField<uint64_t> packed; };
struct ResourceDescriptorPair {
    ResourceDescriptorField<uint32_t> quantity;
    ResourceDescriptorField<uint32_t> alignment;
    void set_packed(uint64_t value) { std::memcpy(this, &value, sizeof(value)); }
};
struct ResourceDescriptorNormalized {
    ResourceDescriptorField<uint32_t> first;
    ResourceDescriptorField<uint32_t> second;
    ResourceDescriptorField<uint32_t> level_count;
    ResourceDescriptorField<uint32_t> quantity;
    ResourceDescriptorField<uint32_t> selector;
    ResourceDescriptorField<uint32_t> first_parameter;
    ResourceDescriptorField<uint32_t> second_parameter;
    uint8_t unknown_1c[4];
    ResourceDescriptorField<uint32_t> flags;
    uint8_t unknown_24[4];
    ResourceDescriptorField<uint32_t> secondary_flags;
    uint8_t unknown_2c[4];
    ResourceDescriptorField<uint32_t> first_derived;
    ResourceDescriptorField<uint32_t> second_derived;
    uint8_t unknown_38[8];
    ResourceDescriptorField<uint8_t> option_byte;
    uint8_t unknown_41[3];
    ResourceDescriptorField<uint32_t> status;
};
struct ResourceDescriptorGeometry {
    ResourceDescriptorField<uint64_t> first_pair;
    ResourceDescriptorField<uint32_t> third;
    uint8_t unknown_0c[0x14];
    ResourceDescriptorField<uint64_t> constructor_tail_pair;
    ResourceDescriptorField<uint32_t> constructor_tail_word;
    uint8_t unknown_2c[0xc];
};
struct ResourceDescriptorDestinationOrLimit {
    uint8_t bytes[8];
    ResourceDescriptorStorage *destination() const
    {
        ResourceDescriptorStorage *value;
        std::memcpy(&value, bytes, sizeof(value));
        return value;
    }
    void set_destination(ResourceDescriptorStorage *value)
    { std::memcpy(bytes, &value, sizeof(value)); }
    uint32_t limit() const
    {
        uint32_t value;
        std::memcpy(&value, bytes, sizeof(value));
        return value;
    }
    void set_limit(uint32_t value)
    { std::memcpy(bytes, &value, sizeof(value)); }
};
struct ResourceDescriptorOutgoingSlot {
    uint8_t bytes[8];
    void set_word(uint32_t value) { std::memcpy(bytes, &value, sizeof(value)); }
    void set_pointer(void *value) { std::memcpy(bytes, &value, sizeof(value)); }
};
// Relative to native post-alignment RSP. Explicit outgoing bytes preserve known
// stores within this exposed view, not a compiler's physical argument frame.
// All other bytes remain uninitialized until a native store/original writes them.
struct alignas(32) ResourceDescriptorFrame {
    ResourceDescriptorOutgoingSlot outgoing[4];
    uint8_t unknown_20[0x10];
    ResourceDescriptorField<uint64_t> second_descriptor_result;
    ResourceDescriptorField<uint64_t> primary_descriptor_result;
    uint8_t unknown_40[4];
    ResourceDescriptorField<uint32_t> predicate_word;
    ResourceDescriptorField<uint64_t> second_descriptor_alignment;
    ResourceDescriptorDestinationOrLimit descriptor_or_limit;
    ResourceDescriptorField<ResourceDescriptorRequirements *> requirements_output;
    ResourceDescriptorPair constructor_pair;
    ResourceDescriptorPair first_auxiliary_pair;
    ResourceDescriptorNormalized normalized;
    ResourceDescriptorLevel second_level;
    uint8_t unknown_e8[4];
    ResourceDescriptorField<uint32_t> optional_second;
    ResourceDescriptorField<uint32_t> optional_first;
    ResourceDescriptorField<uint32_t> optional_unit;
    ResourceDescriptorField<uint64_t> optional_total;
    ResourceDescriptorGeometry geometry;
    uint8_t unknown_138[0x18];
    ResourceDescriptorLevel first_level;
    ResourceDescriptorStorage second_temporary;
    ResourceDescriptorStorage first_temporary;
    ResourceDescriptorField<uint64_t> saved_guard;
    uint8_t unknown_1c8[0x18];
};
static_assert(sizeof(uintptr_t) == 8);
static_assert(sizeof(ResourceDescriptorParameters) == 0x30);
static_assert(sizeof(ResourceDescriptorRequirements) == 8);
static_assert(sizeof(ResourceDescriptorPair) == 8);
static_assert(sizeof(ResourceDescriptorStorage) == 0x20);
static_assert(sizeof(ResourceDescriptorLevel) == 0x30);
static_assert(offsetof(ResourceDescriptorNormalized, flags) == 0x20);
static_assert(offsetof(ResourceDescriptorNormalized, secondary_flags) == 0x28);
static_assert(offsetof(ResourceDescriptorNormalized, first_derived) == 0x30);
static_assert(offsetof(ResourceDescriptorNormalized, second_derived) == 0x34);
static_assert(offsetof(ResourceDescriptorNormalized, option_byte) == 0x40);
static_assert(offsetof(ResourceDescriptorNormalized, status) == 0x44);
static_assert(sizeof(ResourceDescriptorNormalized) == 0x48);
static_assert(offsetof(ResourceDescriptorGeometry, constructor_tail_pair) == 0x20);
static_assert(offsetof(ResourceDescriptorGeometry, constructor_tail_word) == 0x28);
static_assert(sizeof(ResourceDescriptorGeometry) == 0x38);
static_assert(offsetof(ResourceDescriptorFrame, second_descriptor_result) == 0x30);
static_assert(offsetof(ResourceDescriptorFrame, primary_descriptor_result) == 0x38);
static_assert(offsetof(ResourceDescriptorFrame, predicate_word) == 0x44);
static_assert(offsetof(ResourceDescriptorFrame, second_descriptor_alignment) == 0x48);
static_assert(offsetof(ResourceDescriptorFrame, descriptor_or_limit) == 0x50);
static_assert(offsetof(ResourceDescriptorFrame, requirements_output) == 0x58);
static_assert(offsetof(ResourceDescriptorFrame, constructor_pair) == 0x60);
static_assert(offsetof(ResourceDescriptorFrame, first_auxiliary_pair) == 0x68);
static_assert(offsetof(ResourceDescriptorFrame, normalized) == 0x70);
static_assert(offsetof(ResourceDescriptorFrame, second_level) == 0xb8);
static_assert(offsetof(ResourceDescriptorFrame, optional_second) == 0xec);
static_assert(offsetof(ResourceDescriptorFrame, optional_first) == 0xf0);
static_assert(offsetof(ResourceDescriptorFrame, optional_unit) == 0xf4);
static_assert(offsetof(ResourceDescriptorFrame, optional_total) == 0xf8);
static_assert(offsetof(ResourceDescriptorFrame, geometry) == 0x100);
static_assert(offsetof(ResourceDescriptorFrame, first_level) == 0x150);
static_assert(offsetof(ResourceDescriptorFrame, second_temporary) == 0x180);
static_assert(offsetof(ResourceDescriptorFrame, first_temporary) == 0x1a0);
static_assert(offsetof(ResourceDescriptorFrame, saved_guard) == 0x1c0);
static_assert(sizeof(ResourceDescriptorFrame) == 0x1e0);

namespace resource_descriptor_calls {
// These are constructed call-site profiles. Inherited first-call R8/R9 are
// retained explicitly without claiming the normalizer formally consumes them.
RT_ORIGINAL(0x025711d0, normalize,
            void(ResourceDescriptorNormalized *, const ResourceDescriptorParameters *,
                 const ResourceDescriptorOptional *, uint32_t, uintptr_t, uintptr_t));
// The first native call enters with RAX=0x40, so every returned bit above
// AL is zero; the later call reads only AL. Consume a byte here so compiler
// scratch-register contents cannot enter the saved 32-bit predicate word.
RT_ORIGINAL(0x02abb4f0, select_auxiliary, uint8_t(uint32_t, uint32_t));
RT_ORIGINAL(0x02ab7a20, choose_constructor_parameter, uint32_t(uint32_t, uint32_t));
RT_ORIGINAL(0x02ab7ab0, combine_parameters, uint32_t(uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x01485f90, construct_record,
            uint64_t(ResourceDescriptorGeometry *, uint32_t, uint32_t, uint32_t, uint32_t,
                     uint32_t, uint32_t, uint32_t, ResourceDescriptorPair *, ResourceDescriptorPair *));
RT_ORIGINAL(0x01488d90, convert_constructor_record,
            uintptr_t(ResourceDescriptorStorage *, ResourceDescriptorGeometry *, uint32_t));
RT_ORIGINAL(0x01487ee0, initialize_pair_descriptor,
            uint64_t(ResourceDescriptorStorage *, uint32_t, uint32_t, uint32_t, uint32_t,
                     uint32_t, uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x014a1e80, initialize_level,
            uint32_t(ResourceDescriptorLevel *, ResourceDescriptorStorage *, uint32_t, uint32_t));
RT_ORIGINAL(0x0149ce10, calculate_geometry,
            uint32_t(ResourceDescriptorGeometry *, ResourceDescriptorLevel *));
RT_ORIGINAL(0x0149e2d0, calculate_first_auxiliary,
            uint32_t(ResourceDescriptorField<uint64_t> *, ResourceDescriptorField<uint32_t> *,
                     ResourceDescriptorField<uint32_t> *, ResourceDescriptorField<uint32_t> *,
                     uint64_t, uint64_t, uint32_t, uint32_t));
// This is the distinct279B target, not the previously recovered109e400 helper.
// Only this constructed eight-argument prefix is promised; extras are unclosed.
RT_ORIGINAL(0x0149e570, calculate_second_auxiliary,
            uint32_t(ResourceDescriptorField<uint64_t> *, ResourceDescriptorField<uint32_t> *,
                     ResourceDescriptorField<uint32_t> *, ResourceDescriptorField<uint32_t> *,
                     uint64_t, uint64_t, uint32_t, uint32_t));
RT_ORIGINAL(0x02abb4a0, note_failure, uintptr_t(uint32_t));
RT_ORIGINAL(0x02fbe4f8, fail_guard, void());
RT_GLOBAL(0x057e3be0, guard_pointer, const uint64_t *);
}  // namespace resource_descriptor_calls
}  // namespace render

// One six-GP observed caller profile. Normal returns promise zeroextended32
// status. Extra ambient registers, native physical frames and returning guard-
// failure continuation remain unclaimed. No incoming stack argument is consumed.
extern "C" uint64_t bb_render_prepare_resource_descriptor(
    render::ResourceDescriptorRequirements *, render::ResourceDescriptorStorage *,
    const render::ResourceDescriptorParameters *, const render::ResourceDescriptorOptional *,
    uintptr_t inherited_initial, uintptr_t inherited_owner);
