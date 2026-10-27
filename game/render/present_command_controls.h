// SPDX-License-Identifier: GPL-2.0-or-later
// Presentation command controls share byte-backed command storage. Native full RAX
// varies by route; these logical-void interfaces match the known consumer, which
// ignores the result. Packet order and live versus captured cursor updates matter.
#pragma once

#include "present_preparation_packets.h"

namespace render {
RT_ORIGINAL(0x01475da0, begin_record_commands, void(RecordCommandBuffer *, uint32_t mode));
RT_ORIGINAL(0x01473fe0, emit_record_pipeline_mode, void(RecordCommandBuffer *, uint32_t mode));
RT_ORIGINAL(0x01475370, emit_record_enable, void(RecordCommandBuffer *, uint32_t value));
RT_ORIGINAL(0x014741b0, emit_present_record_step, void(RecordCommandBuffer *, uint32_t mode));

// The original indexes this mapped storage with the entire uint32 mode. No array
// extent or bounds check is established; do not substitute a shortened copied table.
RT_GLOBAL(0x04b2bfa0, record_control_mode_words, const uint32_t);

struct RecordControlCommandPrefix {
    PreparationCommandPrefix command;
    PreparationField<uint32_t> flags;
};
static_assert(offsetof(RecordControlCommandPrefix, command) == 0x00);
static_assert(offsetof(RecordControlCommandPrefix, flags) == 0x28);
static_assert(sizeof(RecordControlCommandPrefix) == 0x2c);

struct RecordControlPair {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> value;
};
static_assert(offsetof(RecordControlPair, header) == 0x00);
static_assert(offsetof(RecordControlPair, value) == 0x04);
static_assert(sizeof(RecordControlPair) == 0x08);

struct RecordControlTriple {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> first;
    PreparationField<uint32_t> second;
};
static_assert(offsetof(RecordControlTriple, header) == 0x00);
static_assert(offsetof(RecordControlTriple, first) == 0x04);
static_assert(offsetof(RecordControlTriple, second) == 0x08);
static_assert(sizeof(RecordControlTriple) == 0x0c);

inline bool reserve_record_control_words(RecordCommandBuffer *commands, uint32_t required,
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
    // No capacity recheck follows a successful callback. Failure never reloads cursor.
    cursor = prefix->cursor.get();
    return true;
}

inline void write_record_control_pair(uintptr_t cursor, uint32_t header, uint32_t value)
{
    auto *packet = reinterpret_cast<RecordControlPair *>(cursor);
    packet->header.set(header);
    packet->value.set(value);
}

inline void advance_record_control_cursor(RecordCommandBuffer *commands, uintptr_t bytes)
{
    auto *prefix = preparation_commands(commands);
    // Some packets can overwrite this field. Their native ADD uses its value after
    // those stores, unlike the paths that publish from a previously captured cursor.
    prefix->cursor.set(prefix->cursor.get() + bytes);
}
}  // namespace render
