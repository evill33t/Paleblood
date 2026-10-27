// SPDX-License-Identifier: GPL-2.0-or-later
// Combine the observed three-bit mode and byte-sized value fields in one state packet.
#include "present_register_packets.h"

extern "C" void bb_render_emit_record_state_mode(render::RecordCommandBuffer *commands,
                                                  uint32_t mode, uint32_t value)
{
    constexpr uint32_t register_index = 0x202;
    constexpr uint32_t mode_shift = 4;
    constexpr uint32_t mode_mask = 0x70;
    constexpr uint32_t value_shift = 16;
    constexpr uint32_t value_mask = 0x00ff0000;
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    const uint32_t packed = ((value << value_shift) & value_mask) |
                            ((mode << mode_shift) & mode_mask);
    render::write_present_register_packet(commands, cursor, register_index, packed);
}
