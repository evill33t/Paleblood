// SPDX-License-Identifier: GPL-2.0-or-later
// Write the third presentation register retaining only its observed masked value bits.
#include "present_register_packets.h"

extern "C" void bb_render_emit_record_register_c(render::RecordCommandBuffer *commands,
                                                  uint32_t value)
{
    constexpr uint32_t register_index = 0x200;
    constexpr uint32_t value_mask = 0xc07007ff;
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    render::write_present_register_packet(commands, cursor, register_index, value & value_mask);
}
