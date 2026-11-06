// SPDX-License-Identifier: GPL-2.0-or-later
// Draw using an index offset relative to INDEX_BASE, followed by a thread-trace marker or NOP.
// The caller passes offset before count; keep both DWORD values independent.
#include "command_flags.h"
#include "indexed_draw.h"
#include "packet_observation.h"
#include "record_step_user_pa_enabled.h"

extern "C" void bb_render_draw_indexed_offset(render::PacketBuffer *commands, uint32_t index_offset, uint32_t index_count)
{
    using namespace render;
    constexpr uint32_t required_words = 7;
    constexpr uint32_t draw_index_offset_2_header = 0xc0033500;
    constexpr uint32_t predicate_bit = 1;
    constexpr uint32_t dma_index_source = 0;
    constexpr uint32_t event_write_header = 0xc0004600;
    constexpr uint32_t shader_type_bit = 2;
    constexpr uint32_t thread_trace_marker = 0x35;
    constexpr uint32_t nop_header = 0xc0001000;
    uintptr_t cursor;
    if (!reserve_words(commands, required_words, cursor)) return;
    const PacketObservation observation = observe_packet(commands, cursor);
    const uint32_t flags = packet_flags(commands);
    auto *packet = reinterpret_cast<DrawIndexedOffsetPacket *>(cursor);
    packet->header.set(draw_index_offset_2_header | (flags & predicate_bit));
    packet->max_size.set(index_count);
    packet->index_offset.set(index_offset);
    packet->index_count.set(index_count);
    packet->initiator.set(dma_index_source);
    advance_record_control_cursor(commands, sizeof(DrawIndexedOffsetPacket));
    if (record_step_user_pa_enabled() != 0) {
        const uint32_t live_flags = packet_flags(commands);
        const uintptr_t next = packet_cursor(commands);
        write_record_control_pair(next, event_write_header | (live_flags & shader_type_bit), thread_trace_marker);
        advance_record_control_cursor(commands, sizeof(RecordControlPair));
    } else {
        const uintptr_t next = packet_cursor(commands);
        write_record_control_pair(next, nop_header, 0);
        // The NOP route publishes the captured pointer; the event route adds to live storage.
        set_packet_cursor(commands, next + sizeof(RecordControlPair));
    }
    if (observation.available)
        rt_observe_draw_index_offset_packet(observation.buffer, observation.offset, packet->header.get(), packet->max_size.get(),
                           packet->index_offset.get(), packet->index_count.get(), packet->initiator.get());
}
