// SPDX-License-Identifier: GPL-2.0-or-later
// Draw from inline index bytes. The count controls final-word clearing, independently of copy size.
// The indexed drawing wrapper uses this path to place data beside the command packets.
#include "command_flags.h"
#include "indexed_draw_inline.h"
#include "packet_observation.h"
#include "record_step_user_pa_enabled.h"
#include "byte_copy_calls.h"

extern "C" void bb_render_draw_index_inline(render::PacketBuffer *commands, uint32_t index_count,
                                                const void *source, uint32_t byte_count)
{
    using namespace render;
    constexpr uint32_t inline_data_prefix_words = 10;
    constexpr uint32_t word_rounding_bias = packet_word_bytes - 1;
    constexpr uint32_t draw_index_2_header = 0xc0042700;
    constexpr uint32_t predicate_bit = 1;
    constexpr uint32_t aligned_low_mask = 0xfffffffe;
    constexpr uint32_t word_bits = 32;
    constexpr uint32_t dma_index_source = 0;
    constexpr uint32_t event_write_header = 0xc0004600;
    constexpr uint32_t shader_type_bit = 2;
    constexpr uint32_t thread_trace_marker = 0x35;
    constexpr uint32_t nop_header = 0xc0001000;
    constexpr uint32_t nop_count_shift = 16;
    constexpr uint32_t inline_data_tag = 0x68752000; // Fixed NOP payload; its meaning is not established.
    constexpr uint32_t inline_nop_prefix_words = 2;
    const uint32_t inline_words = (byte_count + word_rounding_bias) / packet_word_bytes;
    const uint32_t required_words = inline_words + inline_data_prefix_words;
    uintptr_t cursor;
    if (!reserve_words(commands, required_words, cursor)) return;
    const uintptr_t data_start = cursor + inline_data_prefix_words * packet_word_bytes;
    const PacketObservation observation = observe_packet(commands, cursor);
    const uint32_t flags = packet_flags(commands);
    auto *packet = reinterpret_cast<DrawIndexedPacket *>(cursor);
    packet->header.set(draw_index_2_header | (flags & predicate_bit));
    packet->max_size.set(index_count);
    packet->base_low.set(static_cast<uint32_t>(data_start) & aligned_low_mask);
    packet->base_high.set(static_cast<uint32_t>(data_start >> word_bits));
    packet->index_count.set(index_count);
    packet->initiator.set(dma_index_source);
    advance_record_control_cursor(commands, sizeof(DrawIndexedPacket));
    uintptr_t tail;
    if (record_step_user_pa_enabled() != 0) {
        const uint32_t live_flags = packet_flags(commands);
        const uintptr_t event_cursor = packet_cursor(commands);
        write_record_control_pair(event_cursor, event_write_header | (live_flags & shader_type_bit), thread_trace_marker);
        // Event stores can overwrite cursor. The following NOP uses the value after those stores.
        tail = packet_cursor(commands);
    } else {
        tail = packet_cursor(commands);
        write_record_control_pair(tail, nop_header, 0);
        // This route retains its captured tail even when the NOP overwrites cursor.
    }
    write_record_control_pair(tail + sizeof(RecordControlPair),
                              nop_header | (inline_words << nop_count_shift), inline_data_tag);
    const uint32_t inline_packet_words = inline_words + inline_nop_prefix_words;
    const uintptr_t end = tail + sizeof(RecordControlPair) +
                          static_cast<uintptr_t>(inline_packet_words) * packet_word_bytes;
    set_packet_cursor(commands, end);
    // Publishing first matters if this word, or the copy destination, overlaps the prefix.
    if (index_count != 0) Field<uint32_t>::write(reinterpret_cast<void *>(end - packet_word_bytes), 0);
    copy_bytes(reinterpret_cast<void *>(data_start), source, static_cast<uint64_t>(byte_count));
    if (observation.available) {
        const uint64_t emitted_address = static_cast<uint64_t>(packet->base_high.get()) << word_bits | packet->base_low.get();
        rt_observe_draw_indexed(observation.buffer, observation.offset, packet->header.get(), packet->max_size.get(),
                           emitted_address, packet->index_count.get(), packet->initiator.get());
    }
}
