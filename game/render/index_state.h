// SPDX-License-Identifier: GPL-2.0-or-later
// The GPU library's index-state setters, called before indexed draws.
#pragma once

#include "command_packets.h"
#include "byte_field.h"

namespace render {
struct IndexWordPacket {
    Field<uint32_t> header;
    Field<uint32_t> value;
};
struct IndexBasePacket {
    Field<uint32_t> header;
    Field<uint32_t> low;
    Field<uint32_t> high;
};
static_assert(offsetof(IndexWordPacket, header) == 0x00);
static_assert(offsetof(IndexWordPacket, value) == 0x04);
static_assert(offsetof(IndexBasePacket, header) == 0x00);
static_assert(offsetof(IndexBasePacket, low) == 0x04);
static_assert(offsetof(IndexBasePacket, high) == 0x08);
static_assert(sizeof(IndexWordPacket) == 2 * packet_word_bytes);
static_assert(sizeof(IndexBasePacket) == 3 * packet_word_bytes);
}  // namespace render

extern "C" void bb_render_set_index_type(render::PacketBuffer *, uint32_t);
extern "C" void bb_render_set_index_base(render::PacketBuffer *, uint64_t);
extern "C" void bb_render_set_index_buffer_size(render::PacketBuffer *, uint32_t);
