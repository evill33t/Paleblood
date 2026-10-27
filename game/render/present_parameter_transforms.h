// SPDX-License-Identifier: GPL-2.0-or-later
// Command packet views for the presentation transform and extent writers. Borrowed
// triplets are read after admission; stores can overlap command cursor storage.
#pragma once

#include "present_command_configuration.h"

namespace render {
inline bool reserve_record_parameter_words(RecordCommandBuffer *commands, uint32_t required,
                                           uintptr_t &cursor)
{
    constexpr uint32_t word_bytes = sizeof(uint32_t);
    auto *prefix = preparation_commands(commands);
    const uintptr_t limit = prefix->limit.get();
    cursor = prefix->cursor.get();
    const uint32_t available = static_cast<uint32_t>((limit - cursor) / word_bytes);
    if (available >= required) return true;
    const uintptr_t argument = prefix->callback_argument.get();
    const auto callback = prefix->space_callback.get();
    if (callback(commands, required, argument) == 0) return false;
    cursor = prefix->cursor.get();
    return true;
}

struct RecordParameterFloat3Words {
    PreparationField<uint32_t> first;
    PreparationField<uint32_t> second;
    PreparationField<uint32_t> third;
};
static_assert(offsetof(RecordParameterFloat3Words, first) == 0x00);
static_assert(offsetof(RecordParameterFloat3Words, second) == 0x04);
static_assert(offsetof(RecordParameterFloat3Words, third) == 0x08);
static_assert(sizeof(RecordParameterFloat3Words) == 0x0c);

struct RecordParameterPairPacket {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> index;
    PreparationField<uint32_t> first;
    PreparationField<uint32_t> second;
};
static_assert(offsetof(RecordParameterPairPacket, header) == 0x00);
static_assert(offsetof(RecordParameterPairPacket, index) == 0x04);
static_assert(offsetof(RecordParameterPairPacket, first) == 0x08);
static_assert(offsetof(RecordParameterPairPacket, second) == 0x0c);
static_assert(sizeof(RecordParameterPairPacket) == 0x10);

struct RecordParameterTransformPacket {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> index;
    PreparationField<uint32_t> scale_first;
    PreparationField<uint32_t> translation_first;
    PreparationField<uint32_t> scale_second;
    PreparationField<uint32_t> translation_second;
    PreparationField<uint32_t> scale_third;
    PreparationField<uint32_t> translation_third;
};
static_assert(offsetof(RecordParameterTransformPacket, header) == 0x00);
static_assert(offsetof(RecordParameterTransformPacket, index) == 0x04);
static_assert(offsetof(RecordParameterTransformPacket, scale_first) == 0x08);
static_assert(offsetof(RecordParameterTransformPacket, translation_first) == 0x0c);
static_assert(offsetof(RecordParameterTransformPacket, scale_second) == 0x10);
static_assert(offsetof(RecordParameterTransformPacket, translation_second) == 0x14);
static_assert(offsetof(RecordParameterTransformPacket, scale_third) == 0x18);
static_assert(offsetof(RecordParameterTransformPacket, translation_third) == 0x1c);
static_assert(sizeof(RecordParameterTransformPacket) == 0x20);

inline uint32_t record_parameter_float_word(float value)
{
    uint32_t word;
    std::memcpy(&word, &value, sizeof(word));
    return word;
}
}  // namespace render
