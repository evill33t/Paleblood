// SPDX-License-Identifier: GPL-2.0-or-later
// Command-buffer prefix and packet fields observed by the presentation slot writer.
#pragma once

#include <cstddef>
#include <cstdint>

namespace render {
struct SlotPacketBuffer;
using SlotPacketSpace = uint8_t (*)(SlotPacketBuffer *, uint32_t requested_words, uintptr_t argument);
struct SlotPacketBuffer {
    uint8_t unknown_0x00[0x8];
    uintptr_t limit;
    uintptr_t cursor;
    SlotPacketSpace space_callback;
    uintptr_t callback_argument;
};
static_assert(sizeof(uintptr_t) == 0x8);
static_assert(offsetof(SlotPacketBuffer, limit) == 0x8);
static_assert(offsetof(SlotPacketBuffer, cursor) == 0x10);
static_assert(offsetof(SlotPacketBuffer, space_callback) == 0x18);
static_assert(offsetof(SlotPacketBuffer, callback_argument) == 0x20);

struct PresentSlotPacket {
    uint32_t header;
    uint32_t register_index;
    uint64_t value;
};
static_assert(offsetof(PresentSlotPacket, header) == 0x0);
static_assert(offsetof(PresentSlotPacket, register_index) == 0x4);
static_assert(offsetof(PresentSlotPacket, value) == 0x8);
static_assert(sizeof(PresentSlotPacket) == 0x10);
}  // namespace render
