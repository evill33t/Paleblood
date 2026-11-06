// SPDX-License-Identifier: GPL-2.0-or-later
// Set the raw INDEX_TYPE word used by later indexed draws.
#include "command_reservation.h"
#include "index_state.h"
#include "packet_observation.h"

extern "C" void bb_render_set_index_type(render::PacketBuffer *commands, uint32_t value)
{
    using namespace render;
    constexpr uint32_t required_words = 2;
    constexpr uint32_t index_type_header = 0xc0002a00;
    uintptr_t cursor;
    if (!reserve_words(commands, required_words, cursor)) return;
    const PacketObservation observation = observe_packet(commands, cursor);
    auto *packet = reinterpret_cast<IndexWordPacket *>(cursor);
    packet->header.set(index_type_header);
    packet->value.set(value);
    set_packet_cursor(commands, packet_cursor(commands) + required_words * packet_word_bytes);
    if (observation.available)
        rt_observe_index_type(observation.buffer, observation.offset,
                           packet->value.get());
}
