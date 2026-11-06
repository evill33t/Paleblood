// SPDX-License-Identifier: GPL-2.0-or-later
// Indexed draws carry either their own address or an offset relative to INDEX_BASE.
#pragma once
#include "present_command_controls.h"

namespace render {
struct DrawIndexedPacket {
    Field<uint32_t> header;
    Field<uint32_t> max_size;
    Field<uint32_t> base_low;
    Field<uint32_t> base_high;
    Field<uint32_t> index_count;
    Field<uint32_t> initiator;
};
static_assert(offsetof(DrawIndexedPacket, header) == 0x00);
static_assert(offsetof(DrawIndexedPacket, max_size) == 0x04);
static_assert(offsetof(DrawIndexedPacket, base_low) == 0x08);
static_assert(offsetof(DrawIndexedPacket, base_high) == 0x0c);
static_assert(offsetof(DrawIndexedPacket, index_count) == 0x10);
static_assert(offsetof(DrawIndexedPacket, initiator) == 0x14);
static_assert(sizeof(DrawIndexedPacket) == 0x18);
struct DrawIndexedOffsetPacket {
    Field<uint32_t> header;
    Field<uint32_t> max_size;
    Field<uint32_t> index_offset;
    Field<uint32_t> index_count;
    Field<uint32_t> initiator;
};
static_assert(offsetof(DrawIndexedOffsetPacket, header) == 0x00);
static_assert(offsetof(DrawIndexedOffsetPacket, max_size) == 0x04);
static_assert(offsetof(DrawIndexedOffsetPacket, index_offset) == 0x08);
static_assert(offsetof(DrawIndexedOffsetPacket, index_count) == 0x0c);
static_assert(offsetof(DrawIndexedOffsetPacket, initiator) == 0x10);
static_assert(sizeof(DrawIndexedOffsetPacket) == 0x14);
} // namespace render
extern "C" void bb_render_draw_indexed(render::PacketBuffer *, uint32_t index_count, uint64_t address);
extern "C" void bb_render_draw_indexed_offset(render::PacketBuffer *, uint32_t index_offset, uint32_t index_count);
