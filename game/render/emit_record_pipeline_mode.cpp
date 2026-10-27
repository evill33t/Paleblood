// SPDX-License-Identifier: GPL-2.0-or-later
// Publish the presentation pipeline mode with captured-cursor arithmetic, including
// when output overlaps command storage. Only the low six input bits reach the packet.
#include "present_command_controls.h"

extern "C" void bb_render_emit_record_pipeline_mode(render::RecordCommandBuffer *commands,
                                                     uint32_t mode)
{
    constexpr uint32_t required_words = 3;
    constexpr uint32_t header = 0xc0017900;
    constexpr uint32_t index = 0x242;
    constexpr uint32_t mode_mask = 0x3f;
    uintptr_t cursor;
    if (!render::reserve_record_control_words(commands, required_words, cursor)) return;
    auto *packet = reinterpret_cast<render::PreparationWordPacket *>(cursor);
    packet->header.set(header);
    packet->index.set(index);
    packet->value.set(mode & mode_mask);
    render::preparation_commands(commands)->cursor.set(cursor + sizeof(render::PreparationWordPacket));
}
