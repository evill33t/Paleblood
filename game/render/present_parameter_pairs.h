// SPDX-License-Identifier: GPL-2.0-or-later
// Presentation parameter pairs use captured command cursors and ordered DWORD stores.
// Float values are packet payload bits, without arithmetic or NaN/zero normalization.
#pragma once

#include "present_command_configuration.h"

namespace render {
struct ParameterFloatWordPacket {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> index;
    PreparationField<uint32_t> value_bits;
};
static_assert(offsetof(ParameterFloatWordPacket, header) == 0x00);
static_assert(offsetof(ParameterFloatWordPacket, index) == 0x04);
static_assert(offsetof(ParameterFloatWordPacket, value_bits) == 0x08);
static_assert(sizeof(ParameterFloatWordPacket) == 0x0c);
static_assert(sizeof(float) == sizeof(uint32_t));

struct ParameterFloatPairPacket {
    ParameterFloatWordPacket second_input;
    ParameterFloatWordPacket first_input;
};
static_assert(offsetof(ParameterFloatPairPacket, second_input) == 0x00);
static_assert(offsetof(ParameterFloatPairPacket, first_input) == 0x0c);
static_assert(sizeof(ParameterFloatPairPacket) == 0x18);

inline uint32_t parameter_float_bits(const float &value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

inline bool reserve_parameter_float_pair(RecordCommandBuffer *commands, uintptr_t &cursor)
{
    constexpr uint32_t required_words = 6;
    constexpr uint32_t word_bytes = sizeof(uint32_t);
    auto *prefix = preparation_commands(commands);
    const uintptr_t limit = prefix->limit.get();
    cursor = prefix->cursor.get();
    const uint32_t available = static_cast<uint32_t>((limit - cursor) / word_bytes);
    if (available >= required_words) return true;
    const uintptr_t argument = prefix->callback_argument.get();
    const auto callback = prefix->space_callback.get();
    if (callback(commands, required_words, argument) == 0) return false;
    cursor = prefix->cursor.get();
    return true;
}

inline void write_parameter_float_pair(RecordCommandBuffer *commands, uintptr_t cursor,
                                       uint32_t second_index, uint32_t second_bits,
                                       uint32_t first_index, uint32_t first_bits)
{
    auto *packet = reinterpret_cast<ParameterFloatPairPacket *>(cursor);
    packet->second_input.header.set(preparation_single_word_header);
    packet->second_input.index.set(second_index);
    packet->second_input.value_bits.set(second_bits);
    packet->first_input.header.set(preparation_single_word_header);
    packet->first_input.index.set(first_index);
    packet->first_input.value_bits.set(first_bits);
    // The native publishes only after all six stores, from the captured cursor even
    // when those stores overlap the command object's cursor word.
    preparation_commands(commands)->cursor.set(cursor + sizeof(ParameterFloatPairPacket));
}
// Known consumers ignore native returns. Successful RAX is the final cursor; failed
// callbacks leave opaque full RAX with AL zero. These functions claim logical void only,
// not incidental register, upper SIMD lane or physical stack-spill equivalence.
}  // namespace render
