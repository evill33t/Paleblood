// SPDX-License-Identifier: GPL-2.0-or-later
// Draw from a supplied index address, then write a thread-trace marker when user PA is enabled.
// Query and packet writes can expose changed flags or cursor storage; retain the live reads.
#include "command_flags.h"
#include "indexed_draw.h"
#include "index_observe.h"
#include "record_step_user_pa_enabled.h"

extern "C" void bb_render_draw_indexed(render::PacketBuffer *commands, uint32_t index_count, uint64_t address)
{
    using namespace render;
    constexpr uint32_t required_words = 8;
    constexpr uint32_t draw_index_2_header = 0xc0042700;
    constexpr uint32_t predicate_bit = 1;
    constexpr uint32_t aligned_low_mask = 0xfffffffe;
    constexpr uint32_t word_bits = 32;
    constexpr uint32_t dma_index_source = 0;
    constexpr uint32_t event_write_header = 0xc0004600;
    constexpr uint32_t shader_type_bit = 2;
    constexpr uint32_t thread_trace_marker = 0x35;
    constexpr uint32_t nop_header = 0xc0001000;
    uintptr_t cursor;
    if (!reserve_words(commands, required_words, cursor)) return;
    const IndexObservation observation = observe_index_call(commands, cursor);
    const uint32_t flags = packet_flags(commands);
    auto *packet = reinterpret_cast<DrawIndexedPacket *>(cursor);
    packet->header.set(draw_index_2_header | (flags & predicate_bit));
    packet->max_size.set(index_count);
    packet->base_low.set(static_cast<uint32_t>(address) & aligned_low_mask);
    packet->base_high.set(static_cast<uint32_t>(address >> word_bits));
    packet->index_count.set(index_count);
    packet->initiator.set(dma_index_source);
    advance_record_control_cursor(commands, sizeof(DrawIndexedPacket));
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
    if (observation.available) {
        const uint64_t emitted_address = static_cast<uint64_t>(packet->base_high.get()) << word_bits | packet->base_low.get();
        rt_observe_draw_indexed(observation.buffer, observation.offset, packet->header.get(), packet->max_size.get(), emitted_address,
                           packet->index_count.get(), packet->initiator.get());
    }
}
