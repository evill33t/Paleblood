// SPDX-License-Identifier: GPL-2.0-or-later
// Emit the full address value into two ordered DWORD payload fields. NULL remains an
// encoded address; the pointed-to storage is never read here.
#include "secondary_calls.h"

extern "C" void bb_render_emit_shader_address(render::RecordCommandBuffer *commands,
                                               uint32_t stage, uint32_t slot, const void *address)
{
    using namespace render;
    constexpr uint32_t words = 4;
    constexpr uint32_t header = 0xc0027600;
    uintptr_t cursor;
    if (!shader_reserve_words(commands, words, cursor)) return;
    const uint32_t flags = shader_packet(commands)->flags.get();
    const uint32_t base = shader_stage_base(stage);
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    auto *out = reinterpret_cast<ShaderPacketWords *>(cursor);
    out->header.set(header | (flags & shader_packet_flag_mask));
    out->register_index.set(base + slot - shader_register_origin);
    out->value_low.set(static_cast<uint32_t>(value));
    out->value_high.set(static_cast<uint32_t>(value >> shader_word_bits));
    shader_packet(commands)->cursor.set(shader_packet(commands)->cursor.get() +
                                       words * shader_packet_word_bytes);
}
