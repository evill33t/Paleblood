// SPDX-License-Identifier: GPL-2.0-or-later
// Derive a numeric normalization parameter. The selector argument is unused. Explicit
// fourth-argument flag bits take priority over the kind-dependent base and third argument.
#include <cstdint>

extern "C" uint32_t bb_render_record_derive_parameter(uint32_t kind, uint32_t,
                                                      uint32_t first, uint32_t second, uint32_t flags)
{
    constexpr uint32_t force_two = 0x20;
    constexpr uint32_t force_four = 0x40;
    constexpr uint32_t mode_three = 3, mode_four = 4;
    constexpr uint32_t default_base = 7, mode_four_base = 8, special_mode_three_base = 9;
    constexpr uint32_t extra_parameter = 3;
    if ((second & force_two) != 0) return 2;
    if ((second & force_four) != 0) return 4;
    uint32_t base = default_base;
    if (kind == mode_four) base = mode_four_base;
    else if (kind == mode_three && flags == mode_four) base = special_mode_three_base;
    return base + (first != 1 ? extra_parameter : 0);
}
