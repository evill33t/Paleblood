// SPDX-License-Identifier: GPL-2.0-or-later
// Append a shader descriptor with its native preamble and intermediate cursor publish.
// Source reads and destination writes retain their order when storage overlaps.
#include "secondary_calls.h"

extern "C" void bb_render_emit_shader_sampler16(render::RecordCommandBuffer *commands,
                                                uint32_t stage, uint32_t slot, const void *descriptor)
{
    using namespace render;
    constexpr uint32_t words = 8;
    constexpr uint32_t control = 0x68750006;
    constexpr uint32_t header = 0xc0047600;
    constexpr uint32_t remaining_bytes = 24;
    uintptr_t cursor;
    if (!shader_reserve_words(commands, words, cursor)) return;
    const uint32_t flags = shader_packet(commands)->flags.get();
    const uint32_t base = shader_stage_base(stage);
    shader_write_descriptor_header(commands, cursor, flags, base, slot, control, header);
    auto *out = reinterpret_cast<ShaderDescriptorPacket *>(cursor);
    const auto *source = static_cast<const ShaderDescriptorWords *>(descriptor);
    const uint64_t low = source->words[0].get();
    const uint64_t high = source->words[1].get();
    out->payload[1].set(high);
    out->payload[0].set(low);
    // Output may have overwritten the cursor: this is a live add, not captured cursor+size.
    shader_packet(commands)->cursor.set(shader_packet(commands)->cursor.get() + remaining_bytes);
}
