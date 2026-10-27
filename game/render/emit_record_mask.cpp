// SPDX-License-Identifier: GPL-2.0-or-later
// Write the complete supplied presentation mask as one register packet.
#include "present_register_packets.h"

extern "C" void bb_render_emit_record_mask(render::RecordCommandBuffer *commands, uint32_t value)
{
    constexpr uint32_t register_index = 0x8e;
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    render::write_present_register_packet(commands, cursor, register_index, value);
}
