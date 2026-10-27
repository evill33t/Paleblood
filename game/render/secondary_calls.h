// SPDX-License-Identifier: GPL-2.0-or-later
// Original call boundaries used by the presentation shader's binding pass. These are
// observed argument profiles; ignored machine return registers have no status contract.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "present_record_consumer.h"

namespace render {

struct ShaderPreparationState;
struct ShaderUploadArena;

RT_ORIGINAL(0x01476030, select_shader_command_mode, void(RecordCommandBuffer *, uint32_t));
RT_ORIGINAL(0x014761a0, emit_shader_descriptor16,
            void(RecordCommandBuffer *, uint32_t, uint32_t, const void *));
RT_ORIGINAL(0x01476240, emit_shader_descriptor32,
            void(RecordCommandBuffer *, uint32_t, uint32_t, const void *));
RT_ORIGINAL(0x014762f0, emit_shader_sampler16,
            void(RecordCommandBuffer *, uint32_t, uint32_t, const void *));
RT_ORIGINAL(0x01476390, emit_shader_address,
            void(RecordCommandBuffer *, uint32_t, uint32_t, const void *));
RT_ORIGINAL(0x01476420, emit_shader_word,
            void(RecordCommandBuffer *, uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x02aaddc0, allocate_shader_upload,
            uint8_t *(ShaderUploadArena *, RecordCommandBuffer **, uint64_t));
RT_ORIGINAL(0x02ab9850, flush_shader_preparation,
            void(ShaderPreparationState *, RecordCommandBuffer *));
RT_ORIGINAL(0x02fbe578, shader_copy_bytes, void *(void *, const void *, uint64_t));

// Byte fields permit unaligned and overlapping native views, including command output
// that overlaps the cursor or source descriptors. Unknown storage has no inferred type.
template <typename T> struct ShaderField {
    uint8_t bytes[sizeof(T)];
    T get() const { T value; std::memcpy(&value, bytes, sizeof(value)); return value; }
    void set(T value) { std::memcpy(bytes, &value, sizeof(value)); }
};

using ShaderSpaceCallback = uint8_t (*)(RecordCommandBuffer *, uint32_t, uintptr_t);
struct ShaderPacketPrefix {
    ShaderField<uintptr_t> begin;
    ShaderField<uintptr_t> limit;
    ShaderField<uintptr_t> cursor;
    ShaderField<ShaderSpaceCallback> space_callback;
    ShaderField<uintptr_t> callback_argument;
    ShaderField<uint32_t> flags;
};
static_assert(offsetof(ShaderPacketPrefix, begin) == 0x00);
static_assert(offsetof(ShaderPacketPrefix, limit) == 0x08);
static_assert(offsetof(ShaderPacketPrefix, cursor) == 0x10);
static_assert(offsetof(ShaderPacketPrefix, space_callback) == 0x18);
static_assert(offsetof(ShaderPacketPrefix, callback_argument) == 0x20);
static_assert(offsetof(ShaderPacketPrefix, flags) == 0x28);
static_assert(sizeof(ShaderPacketPrefix) == 0x2c);

constexpr uint32_t shader_packet_word_bytes = 4;
constexpr uint32_t shader_packet_flag_mask = 2;
constexpr uint32_t shader_register_origin = 0x2c00;
constexpr uint32_t shader_word_bits = 32;
RT_GLOBAL(0x04cd15c0, shader_stage_register_bases, const uint16_t);

inline ShaderPacketPrefix *shader_packet(RecordCommandBuffer *commands)
{
    return reinterpret_cast<ShaderPacketPrefix *>(commands);
}

inline bool shader_reserve_words(RecordCommandBuffer *commands, uint32_t requested,
                                  uintptr_t &cursor)
{
    auto *packet = shader_packet(commands);
    const uintptr_t limit = packet->limit.get();
    cursor = packet->cursor.get();
    const uint64_t available_bytes = limit - cursor;
    const uint32_t available_words = static_cast<uint32_t>(available_bytes / shader_packet_word_bytes);
    if (available_words >= requested) return true;
    const auto argument = packet->callback_argument.get();
    const auto callback = packet->space_callback.get();
    if (callback(commands, requested, argument) == 0) return false;
    cursor = packet->cursor.get();
    return true;
}

inline uint32_t shader_stage_base(uint32_t stage)
{
    // There is no native stage admission check. Preserve the indexed original data read.
    const uintptr_t address = reinterpret_cast<uintptr_t>(shader_stage_register_bases.address()) +
                              uint64_t(stage) * sizeof(uint16_t);
    uint16_t value;
    std::memcpy(&value, reinterpret_cast<const void *>(address), sizeof(value));
    return value;
}

struct ShaderPacketWords {
    ShaderField<uint32_t> header;
    ShaderField<uint32_t> register_index;
    ShaderField<uint32_t> value_low;
    ShaderField<uint32_t> value_high;
};
static_assert(offsetof(ShaderPacketWords, header) == 0x00);
static_assert(offsetof(ShaderPacketWords, register_index) == 0x04);
static_assert(offsetof(ShaderPacketWords, value_low) == 0x08);
static_assert(offsetof(ShaderPacketWords, value_high) == 0x0c);

struct ShaderDescriptorPacket {
    ShaderField<uint32_t> preamble;
    ShaderField<uint32_t> descriptor_control;
    ShaderField<uint32_t> header;
    ShaderField<uint32_t> register_index;
    ShaderField<uint64_t> payload[4];
};
static_assert(offsetof(ShaderDescriptorPacket, preamble) == 0x00);
static_assert(offsetof(ShaderDescriptorPacket, descriptor_control) == 0x04);
static_assert(offsetof(ShaderDescriptorPacket, header) == 0x08);
static_assert(offsetof(ShaderDescriptorPacket, register_index) == 0x0c);
static_assert(offsetof(ShaderDescriptorPacket, payload) == 0x10);

struct ShaderDescriptorWords { ShaderField<uint64_t> words[4]; };

inline void shader_write_descriptor_header(RecordCommandBuffer *commands, uintptr_t cursor,
                                            uint32_t flags, uint32_t stage_base, uint32_t slot,
                                            uint32_t control, uint32_t header)
{
    constexpr uint32_t preamble = 0xc0001000;
    constexpr uint32_t preamble_bytes = 8;
    auto *out = reinterpret_cast<ShaderDescriptorPacket *>(cursor);
    out->preamble.set(preamble);
    out->descriptor_control.set(control);
    shader_packet(commands)->cursor.set(cursor + preamble_bytes);
    out->header.set(header | (flags & shader_packet_flag_mask));
    out->register_index.set(stage_base + slot - shader_register_origin);
}

}  // namespace render
