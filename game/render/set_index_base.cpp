// SPDX-License-Identifier: GPL-2.0-or-later
// Set INDEX_BASE from the aligned low 40 address bits used by the GPU library.
#include "command_reservation.h"
#include "index_state.h"
#include "packet_observation.h"

extern "C" void bb_render_set_index_base(render::PacketBuffer *commands, uint64_t address)
{
    using namespace render;
    constexpr uint32_t required_words = 3;
    constexpr uint32_t index_base_header = 0xc0012600;
    constexpr uint32_t aligned_low_word = 0xfffffffe;
    constexpr uint32_t high_address_byte = 0xff;
    constexpr uint32_t word_bits = 32;
    uintptr_t cursor;
    if (!reserve_words(commands, required_words, cursor)) return;
    const PacketObservation observation = observe_packet(commands, cursor);
    auto *packet = reinterpret_cast<IndexBasePacket *>(cursor);
    packet->header.set(index_base_header);
    packet->low.set(static_cast<uint32_t>(address) & aligned_low_word);
    packet->high.set(static_cast<uint32_t>(address >> word_bits) & high_address_byte);
    set_packet_cursor(commands, packet_cursor(commands) + required_words * packet_word_bytes);
    if (observation.available) {
        const uint32_t low = packet->low.get();
        const uint32_t high = packet->high.get();
        rt_observe_index_base(observation.buffer, observation.offset, (static_cast<uint64_t>(high) << word_bits) | low);
    }
}
