// SPDX-License-Identifier: GPL-2.0-or-later
// The presentation consumer supplies an enable value, but native code writes the
// whole DWORD. Preserve it and the live cursor ADD after the two ordered stores.
#include "present_command_controls.h"

extern "C" void bb_render_emit_record_enable(render::RecordCommandBuffer *commands,
                                              uint32_t value)
{
    constexpr uint32_t required_words = 2;
    constexpr uint32_t header = 0xc0002f00;
    uintptr_t cursor;
    if (!render::reserve_record_control_words(commands, required_words, cursor)) return;
    render::write_record_control_pair(cursor, header, value);
    render::advance_record_control_cursor(commands, sizeof(render::RecordControlPair));
}
