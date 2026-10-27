// SPDX-License-Identifier: GPL-2.0-or-later
// Publish the scalar pair and interleaved borrowed triplets. All six source words
// are captured before output; native cursor reloads and live additions are retained.
#include "present_parameter_transforms.h"

extern "C" void bb_render_configure_record_transform(render::RecordCommandBuffer *commands,
                                                      uint32_t slot,
                                                      render::RecordConfigurationFloat3 *scale,
                                                      render::RecordConfigurationFloat3 *translation,
                                                      uint32_t, float first, float second)
{
    using namespace render;
    constexpr uint32_t required_words = 12;
    constexpr uint32_t pair_header = 0xc0026900;
    constexpr uint32_t pair_index_origin = 0xb4;
    constexpr uint32_t pair_index_stride = 2;
    constexpr uint32_t transform_header = 0xc0066900;
    constexpr uint32_t transform_index_origin = 0x10f;
    constexpr uint32_t transform_index_stride = 6;
    const uint32_t first_word = record_parameter_float_word(first);
    const uint32_t second_word = record_parameter_float_word(second);
    uintptr_t cursor;
    if (!reserve_record_parameter_words(commands, required_words, cursor)) return;

    const auto *scale_words = reinterpret_cast<const RecordParameterFloat3Words *>(scale);
    const auto *translation_words = reinterpret_cast<const RecordParameterFloat3Words *>(translation);
    const uint32_t scale_first = scale_words->first.get();
    const uint32_t scale_second = scale_words->second.get();
    const uint32_t translation_first = translation_words->first.get();
    const uint32_t translation_second = translation_words->second.get();
    const uint32_t scale_third = scale_words->third.get();
    const uint32_t translation_third = translation_words->third.get();
    auto *pair = reinterpret_cast<RecordParameterPairPacket *>(cursor);
    pair->header.set(pair_header);
    pair->index.set(slot * pair_index_stride + pair_index_origin);
    pair->first.set(first_word);
    pair->second.set(second_word);

    auto *prefix = preparation_commands(commands);
    // Native reloads the live cursor after the first stores, then uses that saved
    // value as the base of the second packet even if publishing cursor overlaps it.
    const uintptr_t live = prefix->cursor.get();
    const uintptr_t next = live + sizeof(RecordParameterPairPacket);
    prefix->cursor.set(next);
    auto *packet = reinterpret_cast<RecordParameterTransformPacket *>(next);
    packet->header.set(transform_header);
    packet->index.set(slot * transform_index_stride + transform_index_origin);
    packet->scale_first.set(scale_first);
    packet->translation_first.set(translation_first);
    packet->scale_second.set(scale_second);
    packet->translation_second.set(translation_second);
    packet->scale_third.set(scale_third);
    packet->translation_third.set(translation_third);
    prefix->cursor.set(prefix->cursor.get() + sizeof(RecordParameterTransformPacket));
}
