// SPDX-License-Identifier: GPL-2.0-or-later
// Finish pending command lists, retain live block/counter reads, then conditionally
// submit a byte-exact cursor snapshot. The known caller ignores native guard RAX.
#include "present_command_finish.h"

namespace {
using namespace render;

FinishBucketSlot *bucket_slot(CommandFinishStateView *state, uint64_t index)
{
    const uintptr_t origin = reinterpret_cast<uintptr_t>(state) +
                             offsetof(CommandFinishStateView, bucket_origin);
    return reinterpret_cast<FinishBucketSlot *>(origin + index * sizeof(FinishBucketSlot));
}

uintptr_t block_word(uintptr_t block, uint64_t index)
{
    const uintptr_t address = block + index * sizeof(FinishBlockSlot);
    return reinterpret_cast<const FinishBlockSlot *>(address)->value.get();
}

void check_command_finish_guard(PresentCommandFinishFrame &frame, const uint64_t *pointer)
{
    const uint64_t current = read_present_finish_guard(pointer);
    const uint64_t expected = frame.saved_guard.get();
    check_present_finish_guard_values(current, expected, "finish_present_commands (0x02aaf700)");
}

void drain_nodes(CommandFinishStateView *state, FinishDrainNode *node,
                 PresentCommandFinishFrame &frame)
{
    while (node != nullptr) {
        FinishDrainNode *next = node->next.get();
        void *manager = state->node_manager.get();
        release_present_finish_node(manager, &frame.command_argument, node);
        node = next;
    }
}

void process_blocks(CommandFinishStateView *state, GpuCommandState *commands)
{
    uintptr_t block = state->block_head.get();
    if (block == 0) return;
    const uint32_t link_index = state->block_link_index.get();
    do {
        uint64_t item_index = 0;
        for (;;) {
            // The link is reread on every condition; callbacks can replace it and
            // the count of the last block. Neither value is cached for this loop.
            const uintptr_t link = block_word(block, link_index);
            const uint64_t count = link != 0 ? link_index : state->last_block_count.get();
            if (item_index >= count) break;
            void *processor = state->item_processor.get();
            const uintptr_t item = block_word(block, item_index);
            process_present_finish_item(processor, commands, item);
            state->last_reset_cursor.set(state->buffer.cursor.get());
            ++item_index;
        }
        block = block_word(block, link_index);
    } while (block != 0);
}

void release_one_block(FinishBlockOwner *owner, uintptr_t block)
{
    FinishBlockOwnerVtable *vtable = owner->vtable.get();
    const auto release = vtable->release.get();
    release(owner, reinterpret_cast<void *>(block));
}

void release_blocks(CommandFinishStateView *state)
{
    FinishBlockOwner *owner = state->block_owner.get();
    if (owner == nullptr) return;
    const uint32_t link_index = state->block_link_index.get();
    uintptr_t block = state->block_head.get();
    if (block == 0) return;
    uintptr_t next = block_word(block, link_index);
    release_one_block(owner, block);
    block = next;
    while (block != 0) {
        owner = state->block_owner.get();
        next = block_word(block, link_index);
        release_one_block(owner, block);
        block = next;
    }
}

void copy_snapshot_reverse(FinishCommandSnapshot &out, const FinishCommandSnapshot &in)
{
    out.word_38.set(in.word_38.get());
    out.word_30.set(in.word_30.get());
    out.word_28.set(in.word_28.get());
    out.callback_argument.set(in.callback_argument.get());
    out.callback_bits.set(in.callback_bits.get());
    out.cursor.set(in.cursor.get());
    const uintptr_t begin = in.begin.get();
    const uintptr_t limit = in.limit.get();
    out.limit.set(limit);
    out.begin.set(begin);
}

void finish_snapshot(CommandFinishStateView *state, GpuCommandState *commands,
                     PresentCommandFinishFrame &frame)
{
    const uint8_t flags = state->submission_flags.get();
    frame.snapshot.word_28.set(0);
    FinishSubmissionOwner *owner = state->owner.get();
    FinishCounterSet *counters = owner->counters.get();
    FinishCounterWord *producer = counters->producer.get();
    FinishCounterWord *observed = counters->observed.get();
    const uint32_t expected = producer->value.get() - 1;
    const uint32_t actual = observed->value.get();
    GpuCommandState *optional = nullptr;
    if (actual == expected) {
        copy_snapshot_reverse(frame.snapshot, state->buffer);
    } else {
        constexpr uint32_t minimum_words = 7;
        constexpr uint32_t word_bytes = sizeof(uint32_t);
        const uintptr_t limit = state->buffer.limit.get();
        const uintptr_t cursor = state->buffer.cursor.get();
        const uint32_t available = static_cast<uint32_t>((limit - cursor) / word_bytes);
        if (available < minimum_words) {
            const uintptr_t argument = state->buffer.callback_argument.get();
            const uintptr_t callback_bits = state->buffer.callback_bits.get();
            FinishSpaceCallback callback;
            std::memcpy(&callback, &callback_bits, sizeof(callback));
            // This request is zero, not the seven-word capacity threshold. Its AL
            // result is ignored and this path performs no delegate or cursor reset.
            callback(commands, 0, argument);
            return;
        }
        copy_snapshot_reverse(frame.snapshot, state->buffer);
        state->buffer.begin.set(cursor);
        optional = commands;
    }
    submit_present_finish_snapshot(commands, optional, &frame.snapshot, flags);
    const uintptr_t cursor = state->buffer.cursor.get();
    state->buffer.begin.set(cursor);
    state->last_reset_cursor.set(cursor);
}
}  // namespace

extern "C" void bb_render_finish_present_commands(render::GpuCommandState *commands)
{
    using namespace render;
    PresentCommandFinishFrame frame;
    const uint64_t *initial_guard_pointer = present_finish_guard_pointer.get();
    frame.saved_guard.set(read_present_finish_guard(initial_guard_pointer));
    auto *state = reinterpret_cast<CommandFinishStateView *>(commands);
    state->last_reset_cursor.set(state->buffer.cursor.get());
    if (state->block_head.get() == 0) {
        check_command_finish_guard(frame, initial_guard_pointer);
        return;
    }
    const uintptr_t callback_bits = state->buffer.callback_bits.get();
    const uintptr_t expected_callback = present_finish_expected_callback.get();
    if (callback_bits != expected_callback) {
        check_command_finish_guard(frame, initial_guard_pointer);
        return;
    }

    frame.command_argument = commands;
    uint32_t remaining = state->bucket_mask.get();
    if (remaining != 0) {
        do {
            const uint32_t index = std::countr_zero(remaining);
            frame.saved_bucket_index.set(index);
            const uint32_t bit = uint32_t(1) << index;
            FinishDrainNode *head = bucket_slot(state, index)->head.get();
            drain_nodes(state, head, frame);
            // A callback receives adjacent exposed frame storage. Reload the saved
            // index just as the native does, then clear this captured bucket's head.
            bucket_slot(state, frame.saved_bucket_index.get())->head.set(nullptr);
            remaining -= bit;
        } while (remaining != 0);
        state->bucket_mask.set(0);
    }
    FinishDrainNode *single = state->single_head.get();
    drain_nodes(state, single, frame);
    state->single_head.set(nullptr);
    auto *section = reinterpret_cast<FinishCommandSection *>(&state->section_40);
    prepare_present_finish_section(section, commands);
    process_blocks(state, commands);
    release_blocks(state);
    state->last_block_count.set(0);
    state->word_b4e0.set(0);
    state->block_head.set(0);
    finish_snapshot(state, commands, frame);
    const uint64_t *current_guard_pointer = present_finish_guard_pointer.get();
    check_command_finish_guard(frame, current_guard_pointer);
    // The exposed local frame preserves known relative fields, not native machine
    // frame/register identity. Returning guard-failure imports stop in the shared helper.
}
