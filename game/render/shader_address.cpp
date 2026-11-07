// SPDX-License-Identifier: GPL-2.0-or-later
// Reserve four command words and write the address as two consecutive SET_SH_REG values.
#include "command_flags.h"
#include "shader_packet_layout.h"
#include "shader_observation.h"

extern "C" void bb_render_emit_shader_address(render::PacketBuffer *commands,
                                               uint32_t stage, uint32_t slot, const void *address)
{
    using namespace render;
    constexpr uint32_t words = 4;
    constexpr uint32_t payload_words = 2;
    constexpr uint32_t set_sh_reg_2_values_header = 0xc0027600;
    uintptr_t cursor;
    if (!reserve_words(commands, words, cursor)) return;
    const auto observation = observe_packet(commands, cursor);
    const uint32_t flags = packet_flags(commands);
    const uint32_t base = shader_stage_base(stage);
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    auto *out = reinterpret_cast<ShaderPacketWords *>(cursor);
    out->header.set(set_sh_reg_2_values_header | (flags & shader_packet_flag_mask));
    out->register_index.set(base + slot - shader_register_origin);
    out->value_low.set(static_cast<uint32_t>(value));
    out->value_high.set(static_cast<uint32_t>(value >> shader_word_bits));
    set_packet_cursor(commands, packet_cursor(commands) +
                                       words * shader_packet_word_bytes);
    record_shader_words<payload_words>(observation, &out->register_index, &out->value_low);
}
