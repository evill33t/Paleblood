// SPDX-License-Identifier: GPL-2.0-or-later
// Emit the header first and copy admitted payload words backwards, preserving overlap.
#include "present_shader_primary.h"

extern "C" void bb_render_emit_shader_link_words(render::RecordCommandBuffer *commands,
                                                  const void *source, uint32_t count)
{
    using namespace render;
    constexpr uint32_t header_words = 2;
    constexpr unsigned count_shift = 16;
    constexpr uint32_t command_header = 0xc0006900;
    constexpr uint32_t register_index = 0x191;
    const uint32_t requested = count + header_words;
    uintptr_t cursor;
    if (!shader_reserve_words(commands, requested, cursor)) return;
    auto *header = reinterpret_cast<ShaderLinkCommandHeader *>(cursor);
    header->header.set((count << count_shift) | command_header);
    header->register_index.set(register_index);
    if (count != 0 && count <= shader_link_word_capacity) {
        const auto *words = static_cast<const ShaderField<uint32_t> *>(source);
        auto *payload = reinterpret_cast<ShaderField<uint32_t> *>(cursor + sizeof(*header));
        for (uint32_t i = count; i != 0;) {
            --i;
            const uint32_t value = words[i].get();
            payload[i].set(value);
        }
    }
    // Unlike the descriptor writers, this uses the captured destination, not a live ADD.
    shader_packet(commands)->cursor.set(cursor + uint64_t(requested) * shader_packet_word_bytes);
}
