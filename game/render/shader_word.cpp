// SPDX-License-Identifier: GPL-2.0-or-later
// Append a scalar shader register write. Unlike the descriptor/address writers, the
// final cursor is derived from the captured output address rather than a live increment.
#include "secondary_calls.h"

extern "C" void bb_render_emit_shader_word(render::RecordCommandBuffer *commands,
                                            uint32_t stage, uint32_t slot, uint32_t value)
{
    using namespace render;
    constexpr uint32_t words = 3;
    constexpr uint32_t header = 0xc0017600;
    uintptr_t cursor;
    if (!shader_reserve_words(commands, words, cursor)) return;
    const uint32_t flags = shader_packet(commands)->flags.get();
    const uint32_t base = shader_stage_base(stage);
    auto *out = reinterpret_cast<ShaderPacketWords *>(cursor);
    out->header.set(header | (flags & shader_packet_flag_mask));
    out->register_index.set(base + slot - shader_register_origin);
    out->value_low.set(value);
    shader_packet(commands)->cursor.set(cursor + words * shader_packet_word_bytes);
}
