// SPDX-License-Identifier: GPL-2.0-or-later
// Preserve both float representations across capacity callbacks, writing the second
// input before the first. The values are never converted or compared numerically.
#include "present_parameter_pairs.h"

extern "C" void bb_render_configure_record_ratio_pair(render::RecordCommandBuffer *commands,
                                                       float first, float second)
{
    constexpr uint32_t second_index = 0x2fa;
    constexpr uint32_t first_index = 0x2fc;
    const uint32_t second_bits = render::parameter_float_bits(second);
    const uint32_t first_bits = render::parameter_float_bits(first);
    uintptr_t cursor;
    if (!render::reserve_parameter_float_pair(commands, cursor)) return;
    render::write_parameter_float_pair(commands, cursor, second_index, second_bits,
                                        first_index, first_bits);
}
