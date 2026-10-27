// SPDX-License-Identifier: GPL-2.0-or-later
// Write the indexed presentation register, preserving native slot wrapping and value mask.
#include "present_register_packets.h"

extern "C" void bb_render_emit_record_register_a(render::RecordCommandBuffer *commands,
                                                  uint32_t slot, uint32_t value)
{
    constexpr uint32_t index_origin = 0x1e0;
    constexpr uint32_t value_mask = 0xffff1fff;
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    render::write_present_register_packet(commands, cursor, slot + index_origin, value & value_mask);
}
