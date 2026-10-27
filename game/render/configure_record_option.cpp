// SPDX-License-Identifier: GPL-2.0-or-later
// Pack the two low option bits after capacity admission and publish the captured cursor.
#include "present_command_configuration.h"

extern "C" void bb_render_configure_record_option(render::RecordCommandBuffer *commands,
                                                   uint32_t first, uint32_t second)
{
    constexpr uint32_t register_index = 0x292;
    constexpr uint32_t first_mask = 1;
    constexpr uint32_t second_mask = 2;
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    const uint32_t packed = (first & first_mask) | ((second + second) & second_mask);
    render::write_present_register_packet(commands, cursor, register_index, packed);
}
