// SPDX-License-Identifier: GPL-2.0-or-later
// Begin the presentation record with its mode-derived two-word command. The table
// read follows capacity admission and uses the full input, before masking the output.
#include "present_command_controls.h"

extern "C" void bb_render_begin_record_commands(render::RecordCommandBuffer *commands,
                                                uint32_t mode)
{
    constexpr uint32_t required_words = 2;
    constexpr uint32_t header = 0xc0004600;
    constexpr uint32_t mode_mask = 0x3f;
    constexpr uint32_t table_mask = 0x0f;
    constexpr uint32_t table_shift = 8;
    uintptr_t cursor;
    if (!render::reserve_record_control_words(commands, required_words, cursor)) return;

    const uintptr_t entry = reinterpret_cast<uintptr_t>(render::record_control_mode_words.address())
                            + static_cast<uintptr_t>(mode) * sizeof(uint32_t);
    uint32_t table_word;
    std::memcpy(&table_word, reinterpret_cast<const void *>(entry), sizeof(table_word));
    const uint32_t encoded = (mode & mode_mask) | ((table_word & table_mask) << table_shift);
    render::write_record_control_pair(cursor, header, encoded);
    render::advance_record_control_cursor(commands, sizeof(render::RecordControlPair));
}
