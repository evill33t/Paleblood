// SPDX-License-Identifier: GPL-2.0-or-later
// Emit or clear one prepared record. The descriptor test is repeated after admission,
// and trailing fields use a fresh cursor because packet writes may overlap its storage.
#include "present_preparation_packets.h"

extern "C" void bb_render_emit_prepared_record_packet(render::RecordCommandBuffer *commands,
                                                    uint32_t slot, const void *descriptor)
{
    using namespace render;
    constexpr uint32_t clear_words = 3;
    constexpr uint32_t record_words = 15;
    constexpr uint32_t slot_stride = 15;
    constexpr uint32_t record_index = 0x318;
    constexpr uint32_t clear_index = 0x31c;
    constexpr uint32_t record_header = 0xc00b6900;

    const uint32_t required = descriptor && prepared_record_payload_word(descriptor) != 0
                                  ? record_words : clear_words;
    if (!preparation_reserve_words(commands, required)) return;

    auto *command = preparation_commands(commands);
    if (descriptor && prepared_record_payload_word(descriptor) != 0) {
        const uintptr_t cursor = command->cursor.get();
        const auto *source = static_cast<const PreparationRecordSource *>(descriptor);
        auto *packet = reinterpret_cast<PreparedRecordPacket *>(cursor);
        packet->header.set(record_header);
        packet->index.set(slot * slot_stride + record_index);
        packet->tail.set(source->tail.get());
        packet->fifth.set(source->fifth.get());
        packet->fourth.set(source->fourth.get());
        packet->third.set(source->third.get());
        const uint64_t first = source->first.get();
        const uint64_t second = source->second.get();
        packet->second.set(second);
        packet->first.set(first);

        const uintptr_t trailing_cursor = command->cursor.get();
        auto *trailing = reinterpret_cast<PreparedRecordPacket *>(trailing_cursor);
        trailing->completion_header.set(preparation_completion_header);
        const auto *completion = static_cast<const PreparationRecordCompletionSource *>(descriptor);
        trailing->completion_word.set(completion->word.get());
        command->cursor.set(trailing_cursor + sizeof(PreparedRecordPacket));
    } else {
        const uintptr_t cursor = command->cursor.get();
        auto *packet = reinterpret_cast<PreparationWordPacket *>(cursor);
        packet->header.set(preparation_single_word_header);
        packet->index.set(slot * slot_stride + clear_index);
        packet->value.set(0);
        command->cursor.set(cursor + sizeof(PreparationWordPacket));
    }
    // Logical void matches the known ignored-return caller profile. Native full RAX
    // differs by path; no raw-register or callback-register equivalence is claimed.
}
