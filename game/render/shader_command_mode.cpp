// SPDX-License-Identifier: GPL-2.0-or-later
// Select one of the five admitted command modes. Room is requested before validating
// the selector, so an unsupported selector can still invoke the buffer callback.
#include "secondary_calls.h"

extern "C" void bb_render_select_shader_command_mode(render::RecordCommandBuffer *commands,
                                                      uint32_t mode)
{
    using namespace render;
    constexpr uint32_t words = 3;
    constexpr uint32_t header = 0xc0016900;
    constexpr uint32_t register_index = 0x2d5;
    uintptr_t cursor;
    if (!shader_reserve_words(commands, words, cursor)) return;
    switch (mode) {
    case 0: case 0x45: case 0xad: case 0xb0: case 0x200: break;
    default: return;
    }
    auto *out = reinterpret_cast<ShaderPacketWords *>(cursor);
    out->header.set(header);
    out->register_index.set(register_index);
    out->value_low.set(mode);
    shader_packet(commands)->cursor.set(cursor + words * shader_packet_word_bytes);
}
