// SPDX-License-Identifier: GPL-2.0-or-later
// Pack the low halfwords of both coordinate pairs. The packet is ordered, then
// the native live cursor ADD observes any overlap with those stores.
#include "present_parameter_transforms.h"

extern "C" void bb_render_configure_record_extent(render::RecordCommandBuffer *commands,
                                                   uint32_t x, uint32_t y,
                                                   uint32_t end_x, uint32_t end_y)
{
    using namespace render;
    constexpr uint32_t required_words = 4;
    constexpr uint32_t header = 0xc0026900;
    constexpr uint32_t index = 0x0c;
    constexpr uint32_t halfword_mask = 0xffff;
    constexpr uint32_t halfword_shift = 16;
    uintptr_t cursor;
    if (!reserve_record_parameter_words(commands, required_words, cursor)) return;
    const uint32_t first = (x & halfword_mask) | (y << halfword_shift);
    const uint32_t second = (end_x & halfword_mask) | (end_y << halfword_shift);
    auto *packet = reinterpret_cast<RecordParameterPairPacket *>(cursor);
    packet->header.set(header);
    packet->index.set(index);
    packet->first.set(first);
    packet->second.set(second);
    auto *prefix = preparation_commands(commands);
    prefix->cursor.set(prefix->cursor.get() + sizeof(RecordParameterPairPacket));
}
