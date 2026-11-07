// SPDX-License-Identifier: GPL-2.0-or-later
// Reserve 40 dwords, call sceGnmSetPsShader and advance the live cursor only on status zero.
#include "shader_gnm_calls.h"

extern "C" void bb_render_set_ps_shader(render::PacketBuffer *commands,
    const void *descriptor)
{
    using namespace render;
    constexpr uint32_t words = 40;
    uintptr_t cursor;
    if (!reserve_words(commands, words, cursor)) return;
    if (sceGnmSetPsShader(reinterpret_cast<void *>(cursor), words, descriptor) != 0) return;
    set_packet_cursor(commands, packet_cursor(commands) + uint64_t(words) * shader_packet_word_bytes);
}
