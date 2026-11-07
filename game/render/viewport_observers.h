// SPDX-License-Identifier: GPL-2.0-or-later
// Report final viewport and packed screen-scissor words through the canonical observer.
#pragma once

#include <cstring>

#include "packet_observation.h"
#include "present_parameter_transforms.h"

namespace render {
constexpr std::size_t viewport_axes = 3;

inline void record_viewport_observation(const PacketObservation &call,
                                        const RecordParameterPairPacket *depth,
                                        const RecordParameterTransformPacket *transform)
{
    if (!call.available) return;
    constexpr uint32_t depth_header = 0xc0026900, transform_header = 0xc0066900;
    constexpr uint32_t depth_first = 0xb4, depth_stride = 2, transform_first = 0x10f, transform_stride = 6;
    const uint32_t depth_index = depth->index.get();
    const uint32_t transform_index = transform->index.get();
    if (depth->header.get() != depth_header || transform->header.get() != transform_header ||
            depth_index < depth_first || (depth_index - depth_first) % depth_stride != 0) {
        std::fputs("viewport observer: final packets do not select a supported viewport\n", stderr);
        std::abort();
    }
    const uint32_t index = (depth_index - depth_first) / depth_stride;
    if (index >= RT_VIEWPORTS || transform_index != transform_first + index * transform_stride) {
        std::fputs("viewport observer: final depth and transform selections disagree\n", stderr);
        std::abort();
    }
    // Copy emitted bits rather than rereading borrowed inputs, including NaN payloads.
    const uint32_t minimum_word = depth->first.get(), maximum_word = depth->second.get();
    const uint32_t scale_words[viewport_axes] = {
        transform->x_scale.get(), transform->y_scale.get(), transform->z_scale.get()};
    const uint32_t offset_words[viewport_axes] = {
        transform->x_offset.get(), transform->y_offset.get(), transform->z_offset.get()};
    float minimum, maximum, scale[viewport_axes], offset[viewport_axes];
    std::memcpy(&minimum, &minimum_word, sizeof(minimum));
    std::memcpy(&maximum, &maximum_word, sizeof(maximum));
    std::memcpy(scale, scale_words, sizeof(scale));
    std::memcpy(offset, offset_words, sizeof(offset));
    rt_observe_viewport(call.buffer, call.offset, index, minimum, maximum, scale, offset);
}

inline void record_screen_scissor_observation(const PacketObservation &call,
                                              const RecordParameterPairPacket *packet)
{
    if (!call.available) return;
    constexpr uint32_t pair_header = 0xc0026900, values = 2;
    const uint32_t index = packet->index.get();
    if (packet->header.get() != pair_header ||
            index > RT_CONTEXT_REGISTERS_LAST - RT_CONTEXT_REGISTERS_FIRST - (values - 1)) {
        std::fputs("scissor observer: final packet is outside the supported context register span\n", stderr);
        std::abort();
    }
    const uint32_t first = packet->first.get(), second = packet->second.get();
    rt_observe_register(call.buffer, call.offset, RT_CONTEXT_REGISTERS_FIRST + index, first);
    rt_observe_register(call.buffer, call.offset, RT_CONTEXT_REGISTERS_FIRST + index + 1, second);
}
}  // namespace render
