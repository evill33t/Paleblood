// SPDX-License-Identifier: GPL-2.0-or-later
#include "present_command_configuration.h"

extern "C" void bb_render_emit_record_configuration_register(
    render::RecordCommandBuffer *commands, uint32_t value)
{
    uintptr_t cursor;
    if (!render::reserve_present_register_packet(commands, cursor)) return;
    // The captured value is masked only after the callback's admission result.
    constexpr uint32_t register_index = 0x206u;
    constexpr uint32_t value_mask = 0x00000f3f;
    const uint32_t masked_value = value & value_mask;
    render::write_present_register_packet(commands, cursor, register_index, masked_value);
    // Known callers ignore native RAX. Failed callbacks leave its full value opaque.
}
