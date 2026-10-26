// SPDX-License-Identifier: GPL-2.0-or-later
// Pack the exact combined selector key. The second selector is not nibble-masked,
// and the final special flag depends on the original full first selector.
#include "record_constructor_helpers.h"

extern "C" uint32_t bb_render_record_constructor_pack_pair(uint32_t first_selector, uint32_t second_selector)
{
    constexpr uint32_t selector_shift = 4;
    constexpr uint32_t default_code = 0x4400;
    constexpr uint32_t ordinary_flags = 0x220000, special_flags = 0x228000, special_bit = 0x40000;
    constexpr uint32_t special_selector = 4;
    const uint32_t key = (first_selector << selector_shift) | second_selector;
    uint32_t code;
    switch (key) {
    case 0x10: code = 0x442c; break;
    case 0x11: code = 0x442f; break;
    case 0x20: code = 0x442d; break;
    case 0x21: code = 0x4430; break;
    case 0x22: code = 0x4431; break;
    case 0x30: code = 0x442e; break;
    case 0x31: code = 0x4433; break;
    case 0x32: code = 0x4435; break;
    case 0x33: code = 0x4436; break;
    case 0x40: code = 0x4432; break;
    case 0x41: code = 0x4434; break;
    case 0x42: code = 0x4437; break;
    case 0x43: code = 0x4438; break;
    default: code = default_code; break;
    }
    return code | (first_selector == special_selector ? special_flags | special_bit : ordinary_flags);
}
