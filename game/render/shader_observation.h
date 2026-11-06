// SPDX-License-Identifier: GPL-2.0-or-later
// Observe final shader packets through the canonical runtime bridge, without rereading descriptors.
#pragma once

#include <cstring>

#include "packet_observation.h"
#include "shader_packet_layout.h"

namespace render {
struct ShaderUserDataSelection {
    rt_shader_stage stage;
    unsigned slot;
};

inline ShaderUserDataSelection observed_shader_selection(uint32_t index, unsigned words)
{
    ShaderUserDataSelection selected{};
    bool found = false;
    for (unsigned stage = 0; stage < RT_SHADER_STAGES; ++stage) {
        const uint32_t base = shader_stage_base(stage) - shader_register_origin;
        if (index < base || index - base >= RT_USER_DATA_SLOTS ||
            words > RT_USER_DATA_SLOTS - (index - base)) continue;
        if (found) {
            std::fputs("shader observer: ambiguous user-data register window\n", stderr);
            std::abort();
        }
        selected = {static_cast<rt_shader_stage>(stage), index - base};
        found = true;
    }
    if (!found) {
        std::fputs("shader observer: packet does not select a supported user-data span\n", stderr);
        std::abort();
    }
    return selected;
}

template <unsigned Words>
inline void record_shader_words(const PacketObservation &observation, const Field<uint32_t> *register_index,
                                const void *published_words)
{
    static_assert(Words > 0 && Words <= RT_USER_DATA_MAX_WORDS);
    if (!observation.available) return;
    // Publication can change the selection as well as the payload; use the final register.
    const auto selected = observed_shader_selection(register_index->get(), Words);
    uint32_t values[Words];
    std::memcpy(values, published_words, sizeof(values));
    rt_observe_user_data(observation.buffer, observation.offset, selected.stage, selected.slot, values, Words);
}

inline void record_shader_mode(const PacketObservation &observation, const ShaderPacketWords *packet)
{
    if (!observation.available) return;
    const uint32_t index = packet->register_index.get();
    if (index > RT_CONTEXT_REGISTERS_LAST - RT_CONTEXT_REGISTERS_FIRST) {
        std::fputs("shader observer: context packet index is outside the context register range\n", stderr);
        std::abort();
    }
    rt_observe_register(observation.buffer, observation.offset, RT_CONTEXT_REGISTERS_FIRST + index,
                        packet->value_low.get());
}
}  // namespace render
