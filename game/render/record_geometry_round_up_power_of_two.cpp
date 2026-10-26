// SPDX-License-Identifier: GPL-2.0-or-later
// Round upward in the native uint32 domain. Zero and values whose next power
// exceeds 32 bits deliberately wrap to zero rather than saturating.
#include <cstdint>

extern "C" uint32_t bb_render_record_geometry_round_up_power_of_two(uint32_t value)
{
    uint32_t spread = value - 1;
    spread |= spread >> 1;
    spread |= spread >> 2;
    spread |= spread >> 4;
    spread |= spread >> 8;
    spread |= spread >> 16;
    return spread + 1;
}
