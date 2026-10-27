// SPDX-License-Identifier: GPL-2.0-or-later
// Primary presentation binding and its command writers. These logical-void boundaries
// match callers that discard incidental native RAX; they do not synthesize a status.
// Byte-backed views preserve unaligned/overlapping ordinary owned memory. Native invalid
// pointers and count>32 stack corruption are outside this source's equivalence domain:
// the original primary frame has only 128 bytes and does not admit/reject that count.
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "secondary_calls.h"

namespace render {
constexpr std::size_t primary_shader_bank_count = 3;
constexpr std::size_t primary_shader_slot_count = 64;
constexpr std::size_t primary_shader_special_flag_index = 58;
constexpr std::size_t primary_shader_first_selector_index = 59;
constexpr std::size_t primary_shader_second_selector_index = 60;
constexpr std::size_t primary_shader_first_one_hot_slot = 58;
constexpr std::size_t primary_shader_second_one_hot_slot = 59;
constexpr std::size_t primary_shader_special_slot = 31;
constexpr std::size_t primary_shader_mask_flag_byte = 3;
constexpr uint8_t primary_shader_special_mask = 0x80;
constexpr uint64_t primary_shader_dirty_bit = 0x80000000;
constexpr uint64_t primary_shader_invalid = UINT64_MAX;
constexpr std::size_t shader_link_word_capacity = 32;

struct ShaderPrimaryPair {
    ShaderField<uint64_t> low;
    ShaderField<uint64_t> high;
};
struct ShaderPrimaryPairValue { uint64_t low, high; };
static_assert(sizeof(ShaderPrimaryPair) == 0x10);
static_assert(offsetof(ShaderPrimaryPair, high) == 0x08);
static_assert(sizeof(ShaderPrimaryPairValue) == sizeof(ShaderPrimaryPair));

struct ShaderPrimaryIndexMap { uint8_t indices[primary_shader_slot_count]; };
struct ShaderPrimarySlotBank { ShaderField<uint64_t> slots[primary_shader_slot_count]; };

// d2/d3/d4 overlap the last map, so they are accessed through that same byte storage.
struct ShaderPrimaryDescriptorView {
    ShaderField<uint64_t> masks[primary_shader_bank_count];
    ShaderPrimaryIndexMap maps[primary_shader_bank_count];
    ShaderField<uintptr_t> table_d8;
    uint8_t unknown_0xe0[0x10];
    ShaderPrimaryPair pair_f0;
    ShaderField<uintptr_t> program_100;
};
static_assert(offsetof(ShaderPrimaryDescriptorView, maps) == 0x18);
static_assert(offsetof(ShaderPrimaryDescriptorView, table_d8) == 0xd8);
static_assert(offsetof(ShaderPrimaryDescriptorView, pair_f0) == 0xf0);
static_assert(offsetof(ShaderPrimaryDescriptorView, program_100) == 0x100);
static_assert(sizeof(ShaderPrimaryDescriptorView) == 0x108);

// The one-byte source anchors describe addresses, not capacities of trailing arrays.
struct ShaderPrimaryProgramView {
    uint8_t unknown_0x00[0x3];
    uint8_t index_03;
    uint8_t unknown_0x04[0x4];
    uint8_t command_08;
    uint8_t unknown_0x09[0x1b];
    uint8_t index_24;
    uint8_t count_25;
    uint8_t unknown_0x26[0x2];
    uint8_t source_28;
    uint8_t unknown_0x29[0xf];
    uint8_t count_38;
    uint8_t unknown_0x39[0x3];
    uint8_t source_3c;
};
static_assert(offsetof(ShaderPrimaryProgramView, index_03) == 0x03);
static_assert(offsetof(ShaderPrimaryProgramView, command_08) == 0x08);
static_assert(offsetof(ShaderPrimaryProgramView, index_24) == 0x24);
static_assert(offsetof(ShaderPrimaryProgramView, count_25) == 0x25);
static_assert(offsetof(ShaderPrimaryProgramView, source_28) == 0x28);
static_assert(offsetof(ShaderPrimaryProgramView, count_38) == 0x38);
static_assert(offsetof(ShaderPrimaryProgramView, source_3c) == 0x3c);

struct ShaderPrimaryStageView {
    uint8_t unknown_0x00[0x4];
    ShaderField<uint32_t> flag_04;
    uint8_t unknown_0x08[0x20];
    ShaderPrimaryPair dirty_28;
    ShaderField<uint64_t> invalid_38;
    uint8_t unknown_0x40[0x20];
    ShaderField<uint64_t> masks[primary_shader_bank_count];
    ShaderPrimarySlotBank banks[primary_shader_bank_count];
};
static_assert(offsetof(ShaderPrimaryStageView, flag_04) == 0x04);
static_assert(offsetof(ShaderPrimaryStageView, dirty_28) == 0x28);
static_assert(offsetof(ShaderPrimaryStageView, invalid_38) == 0x38);
static_assert(offsetof(ShaderPrimaryStageView, masks) == 0x60);
static_assert(offsetof(ShaderPrimaryStageView, banks) == 0x78);
static_assert(sizeof(ShaderPrimarySlotBank) == 0x200);
static_assert(sizeof(ShaderPrimaryStageView) == 0x678);

struct ShaderPrimaryStateView {
    ShaderPrimaryStageView primary;
    uint8_t unknown_0x0678[0x1288];
    ShaderPrimaryStageView optional;
    uint8_t unknown_0x1f78[0x630];
    ShaderField<uint64_t> invalid_25a8;
    uint8_t unknown_0x25b0[0x7620];
    ShaderPrimaryPair primary_pair_9bd0;
    uint8_t unknown_0x9be0[0x4f0];
    ShaderPrimaryPair optional_pair_a0d0;
};
static_assert(offsetof(ShaderPrimaryStateView, optional) == 0x1900);
static_assert(offsetof(ShaderPrimaryStateView, invalid_25a8) == 0x25a8);
static_assert(offsetof(ShaderPrimaryStateView, primary_pair_9bd0) == 0x9bd0);
static_assert(offsetof(ShaderPrimaryStateView, optional_pair_a0d0) == 0xa0d0);
static_assert(sizeof(ShaderPrimaryStateView) == 0xa0e0);
static_assert(alignof(ShaderPrimaryStateView) == 1);

struct ShaderLinkInputPair {
    uint8_t key;
    uint8_t value;
    uint16_t word() const
    {
        uint16_t result;
        std::memcpy(&result, this, sizeof(result));
        return result;
    }
};
static_assert(sizeof(ShaderLinkInputPair) == 2);
static_assert(offsetof(ShaderLinkInputPair, value) == 1);
struct ShaderLinkCommandHeader {
    ShaderField<uint32_t> header;
    ShaderField<uint32_t> register_index;
};
static_assert(sizeof(ShaderLinkCommandHeader) == 8);
static_assert(offsetof(ShaderLinkCommandHeader, register_index) == 4);

RT_ORIGINAL(0x01475480, bind_primary_shader_commands,
            void(RecordCommandBuffer *, const void *, uint32_t));
RT_ORIGINAL(0x014753c0, bind_optional_shader_commands,
            void(RecordCommandBuffer *, const void *));
RT_ORIGINAL(0x014873a0, build_shader_link_words,
            void(void *, const void *, uint32_t, const void *, uint32_t));
RT_ORIGINAL(0x01475000, emit_shader_link_words,
            void(RecordCommandBuffer *, const void *, uint32_t));

// Opaque native writers: only these argument profiles and low32 return gates are known.
RT_ORIGINAL(0x02fc04d8, native_write_primary_shader,
            uint32_t(void *, uint32_t, const void *, uint32_t));
RT_ORIGINAL(0x02fc04b8, native_write_optional_shader,
            uint32_t(void *, uint32_t, const void *));
}  // namespace render
