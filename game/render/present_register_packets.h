// SPDX-License-Identifier: GPL-2.0-or-later
// Three-DWORD register packets used by the presentation-record consumer. Successful
// native returns contain the final cursor; failed callbacks leave opaque full RAX.
// These logical-void profiles match the known consumer's ignored-return boundaries.
#pragma once

#include "present_preparation_packets.h"

namespace render {
RT_ORIGINAL(0x01473090, emit_record_register_a,
            void(RecordCommandBuffer *, uint32_t slot, uint32_t value));
RT_ORIGINAL(0x01472fd0, emit_record_mask, void(RecordCommandBuffer *, uint32_t value));
RT_ORIGINAL(0x014733a0, emit_record_register_c, void(RecordCommandBuffer *, uint32_t value));
RT_ORIGINAL(0x01473400, emit_record_register_b, void(RecordCommandBuffer *, uint32_t value));
RT_ORIGINAL(0x01473320, emit_record_state_mode,
            void(RecordCommandBuffer *, uint32_t mode, uint32_t value));

inline bool reserve_present_register_packet(RecordCommandBuffer *commands, uintptr_t &cursor)
{
    constexpr uint32_t required_words = 3;
    constexpr uint32_t word_bytes = sizeof(uint32_t);
    auto *prefix = preparation_commands(commands);
    const uintptr_t limit = prefix->limit.get();
    cursor = prefix->cursor.get();
    const uint32_t available = static_cast<uint32_t>((limit - cursor) / word_bytes);
    if (available >= required_words) return true;
    const uintptr_t argument = prefix->callback_argument.get();
    const auto callback = prefix->space_callback.get();
    if (callback(commands, required_words, argument) == 0) return false;
    // Only the callback-success route reloads cursor. There is no capacity recheck.
    cursor = prefix->cursor.get();
    return true;
}

inline void write_present_register_packet(RecordCommandBuffer *commands, uintptr_t cursor,
                                          uint32_t index, uint32_t value)
{
    auto *packet = reinterpret_cast<PreparationWordPacket *>(cursor);
    packet->header.set(preparation_single_word_header);
    packet->index.set(index);
    packet->value.set(value);
    // Output can overlap command storage. Always publish from the captured cursor,
    // rather than reloading a cursor word overwritten by those three ordered stores.
    preparation_commands(commands)->cursor.set(cursor + sizeof(PreparationWordPacket));
}
}  // namespace render
