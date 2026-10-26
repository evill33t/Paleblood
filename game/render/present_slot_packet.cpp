// SPDX-License-Identifier: GPL-2.0-or-later
// Emit a four-word presentation slot packet. Prefix writes precede the input value read;
// the input may alias them. Advance the live stored cursor after all packet stores.
#include <cstring>

#include "present_slot_packet.h"

namespace {
constexpr uint32_t packet_words = 4;
constexpr uint32_t command_word_bytes = 4;
constexpr uint32_t packet_header = 0xc0026900;
constexpr uint32_t slot_stride = 15;
constexpr uint32_t slot_register_base = 0x323;
}  // namespace

extern "C" void bb_render_present_slot_emit(render::SlotPacketBuffer *buffer, uint32_t index,
                                             const uint64_t *value)
{
    const uintptr_t available_bytes = buffer->limit - buffer->cursor;
    const uint32_t available_words = static_cast<uint32_t>(available_bytes / command_word_bytes);
    if (available_words < packet_words) {
        const uint8_t granted = buffer->space_callback(buffer, packet_words, buffer->callback_argument);
        if (granted == 0) return;
    }

    const uintptr_t cursor = buffer->cursor;
    const uint32_t register_index = index * slot_stride + slot_register_base;
    // Byte copies preserve aliasing between command storage, the input and the buffer fields.
    std::memcpy(reinterpret_cast<void *>(cursor + offsetof(render::PresentSlotPacket, header)),
                &packet_header, sizeof(packet_header));
    std::memcpy(reinterpret_cast<void *>(cursor + offsetof(render::PresentSlotPacket, register_index)),
                &register_index, sizeof(register_index));
    uint64_t loaded_value;
    std::memcpy(&loaded_value, value, sizeof(loaded_value));
    std::memcpy(reinterpret_cast<void *>(cursor + offsetof(render::PresentSlotPacket, value)),
                &loaded_value, sizeof(loaded_value));
    buffer->cursor += sizeof(render::PresentSlotPacket);
}
