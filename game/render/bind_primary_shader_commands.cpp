// SPDX-License-Identifier: GPL-2.0-or-later
// Request room for the primary descriptor writer and advance only after its low32 success.
#include "present_shader_primary.h"

extern "C" void bb_render_bind_primary_shader_commands(render::RecordCommandBuffer *commands,
                                                         const void *descriptor,
                                                         uint32_t modifier)
{
    using namespace render;
    constexpr uint32_t words = 29;
    uintptr_t cursor;
    if (!shader_reserve_words(commands, words, cursor)) return;
    if (native_write_primary_shader(reinterpret_cast<void *>(cursor), words,
                                    descriptor, modifier) != 0) return;
    // The original adds to the live field, including changes made by the native writer.
    auto *packet = shader_packet(commands);
    packet->cursor.set(packet->cursor.get() + uint64_t(words) * shader_packet_word_bytes);
}
