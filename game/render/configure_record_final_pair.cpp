// SPDX-License-Identifier: GPL-2.0-or-later
// Emit the final two parameter payloads in second-input/first-input order, retaining
// raw float bits and the captured-cursor publication after both packets.
#include "present_parameter_pairs.h"

extern "C" void bb_render_configure_record_final_pair(render::RecordCommandBuffer *commands,
                                                       float first, float second)
{
    constexpr uint32_t second_index = 0x2fb;
    constexpr uint32_t first_index = 0x2fd;
    const uint32_t second_bits = render::parameter_float_bits(second);
    const uint32_t first_bits = render::parameter_float_bits(first);
    uintptr_t cursor;
    if (!render::reserve_parameter_float_pair(commands, cursor)) return;
    render::write_parameter_float_pair(commands, cursor, second_index, second_bits,
                                        first_index, first_bits);
}
