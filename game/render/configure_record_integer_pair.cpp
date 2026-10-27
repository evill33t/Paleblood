// SPDX-License-Identifier: GPL-2.0-or-later
// Pack bits two through eight of both integer inputs, with native 32-bit shift wrapping.
#include "present_parameter_pairs.h"

extern "C" void bb_render_configure_record_integer_pair(render::RecordCommandBuffer *commands,
                                                         uint32_t first, uint32_t second)
{
    constexpr uint32_t register_index = 0x8d;
    constexpr uint32_t first_mask = 0x1fc;
    constexpr uint32_t second_shift = 16;
    constexpr uint32_t second_mask = 0x01fc0000;
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    const uint32_t packed = (first & first_mask) | ((second << second_shift) & second_mask);
    render::write_present_register_packet(commands, cursor, register_index, packed);
}
