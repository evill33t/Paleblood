// SPDX-License-Identifier: GPL-2.0-or-later
// NULL descriptors are forwarded to the optional writer; they are not read here.
#include "present_shader_primary.h"

extern "C" void bb_render_bind_optional_shader_commands(render::RecordCommandBuffer *commands,
                                                          const void *descriptor)
{
    using namespace render;
    constexpr uint32_t words = 40;
    uintptr_t cursor;
    if (!shader_reserve_words(commands, words, cursor)) return;
    if (native_write_optional_shader(reinterpret_cast<void *>(cursor), words, descriptor) != 0) return;
    auto *packet = shader_packet(commands);
    packet->cursor.set(packet->cursor.get() + uint64_t(words) * shader_packet_word_bytes);
}
