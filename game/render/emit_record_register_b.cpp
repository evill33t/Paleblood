// SPDX-License-Identifier: GPL-2.0-or-later
// Write the second presentation register's low twenty-four value bits.
#include "present_register_packets.h"

extern "C" void bb_render_emit_record_register_b(render::RecordCommandBuffer *commands,
                                                  uint32_t value)
{
    constexpr uint32_t register_index = 0x10b;
    constexpr uint32_t value_mask = 0x00ffffff;
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    render::write_present_register_packet(commands, cursor, register_index, value & value_mask);
}
