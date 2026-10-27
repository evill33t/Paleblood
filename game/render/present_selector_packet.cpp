// SPDX-License-Identifier: GPL-2.0-or-later
// Emit or clear the prepared selector descriptor. Keep short-circuit tests and live
// source reads in order: packet output may overlap either descriptor or cursor storage.
#include "present_preparation_packets.h"

namespace {
bool has_selector_payload(const void *descriptor)
{
    return descriptor && (render::prepared_selector_first_word(descriptor) != 0 ||
                          render::prepared_selector_second_word(descriptor) != 0);
}
}  // namespace

extern "C" void bb_render_emit_prepared_selector_packet(render::RecordCommandBuffer *commands,
                                                      const void *descriptor)
{
    using namespace render;
    constexpr uint32_t clear_words = 6;
    constexpr uint32_t selector_words = 24;
    constexpr uint32_t selector_header = 0xc0086900;
    constexpr uint32_t selector_index = 0x10;
    constexpr uint32_t following_selector_index = 0x11;
    constexpr uint32_t parameter_2c_index = 0x0f;
    constexpr uint32_t parameter_20_index = 2;
    constexpr uint32_t parameter_24_index = 5;
    constexpr uint32_t parameter_28_index = 0x2af;

    const uint32_t required = has_selector_payload(descriptor) ? selector_words : clear_words;
    if (!preparation_reserve_words(commands, required)) return;

    auto *command = preparation_commands(commands);
    if (has_selector_payload(descriptor)) {
        const uintptr_t cursor = command->cursor.get();
        const auto *source = static_cast<const PreparationSelectorSource *>(descriptor);
        auto *packet = reinterpret_cast<PreparedSelectorPacket *>(cursor);
        packet->header.set(selector_header);
        packet->index.set(selector_index);
        packet->fourth.set(source->fourth.get());
        packet->third.set(source->third.get());
        const uint64_t first = source->first.get();
        const uint64_t second = source->second.get();
        packet->second.set(second);
        packet->first.set(first);

        const uintptr_t trailing_cursor = command->cursor.get();
        auto *trailing = reinterpret_cast<PreparedSelectorPacket *>(trailing_cursor);
        const uint32_t parameter_2c = source->parameter_2c.get();
        trailing->parameter_2c.header.set(preparation_single_word_header);
        trailing->parameter_2c.index.set(parameter_2c_index);
        trailing->parameter_2c.value.set(parameter_2c);
        const uint32_t parameter_20 = source->parameter_20.get();
        trailing->parameter_20.header.set(preparation_single_word_header);
        trailing->parameter_20.index.set(parameter_20_index);
        trailing->parameter_20.value.set(parameter_20);
        const uint32_t parameter_24 = source->parameter_24.get();
        trailing->parameter_24.header.set(preparation_single_word_header);
        trailing->parameter_24.index.set(parameter_24_index);
        trailing->parameter_24.value.set(parameter_24);
        const uint32_t parameter_28 = source->parameter_28.get();
        trailing->parameter_28.header.set(preparation_single_word_header);
        trailing->parameter_28.index.set(parameter_28_index);
        trailing->parameter_28.value.set(parameter_28);
        trailing->completion_header.set(preparation_completion_header);
        trailing->completion_word.set(source->parameter_30.get());
        command->cursor.set(trailing_cursor + sizeof(PreparedSelectorPacket));
    } else {
        const uintptr_t cursor = command->cursor.get();
        auto *packet = reinterpret_cast<PreparedSelectorClearPacket *>(cursor);
        packet->first.header.set(preparation_single_word_header);
        packet->first.index.set(selector_index);
        packet->first.value.set(0);
        packet->second.header.set(preparation_single_word_header);
        packet->second.index.set(following_selector_index);
        packet->second.value.set(0);
        command->cursor.set(cursor + sizeof(PreparedSelectorClearPacket));
    }
    // Logical void matches known ignored-return callers. Native RAX contains the final
    // cursor or an opaque failed-callback value; arbitrary register equality is unclaimed.
}
