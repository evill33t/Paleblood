// SPDX-License-Identifier: GPL-2.0-or-later
// Retain the acquired-buffer capture, partial block publications and reverse
// snapshot copy of the original managed command-space callback. No growth fallback,
// timeout, input guard or rollback policy is supplied beyond the observed body.
#include "present_managed_capacity.h"

namespace {
using namespace render;
constexpr uintptr_t block_slot_bytes = sizeof(uint64_t);
constexpr uint64_t command_word_bytes = sizeof(uint32_t);
constexpr uint32_t allocation_alignment = sizeof(uint64_t);

FinishBlockSlot *managed_capacity_block_slot(uintptr_t block, uint32_t index)
{
    const uintptr_t address = block + static_cast<uintptr_t>(index) * block_slot_bytes;
    return reinterpret_cast<FinishBlockSlot *>(address);
}

void copy_snapshot_word(uint8_t (&destination)[sizeof(uint64_t)],
                        const uint8_t (&source)[sizeof(uint64_t)])
{
    uint8_t captured[sizeof(uint64_t)];
    std::memcpy(captured, source, sizeof(captured));
    std::memcpy(destination, captured, sizeof(captured));
}

void capture_managed_capacity_snapshot(FinishCommandSnapshot &snapshot,
                                       const FinishCommandSnapshot &buffer)
{
    // Keep each read/store pair ordered, and preserve unknown word representations.
    copy_snapshot_word(snapshot.word_38.bytes, buffer.word_38.bytes);
    copy_snapshot_word(snapshot.word_30.bytes, buffer.word_30.bytes);
    copy_snapshot_word(snapshot.word_28.bytes, buffer.word_28.bytes);
    copy_snapshot_word(snapshot.callback_argument.bytes, buffer.callback_argument.bytes);
    copy_snapshot_word(snapshot.callback_bits.bytes, buffer.callback_bits.bytes);
    copy_snapshot_word(snapshot.cursor.bytes, buffer.cursor.bytes);
    uint8_t captured_begin[sizeof(uint64_t)];
    uint8_t captured_limit[sizeof(uint64_t)];
    std::memcpy(captured_begin, buffer.begin.bytes, sizeof(captured_begin));
    std::memcpy(captured_limit, buffer.limit.bytes, sizeof(captured_limit));
    std::memcpy(snapshot.limit.bytes, captured_limit, sizeof(captured_limit));
    std::memcpy(snapshot.begin.bytes, captured_begin, sizeof(captured_begin));
}

uint8_t finish_managed_capacity(ManagedCapacityFrame &frame, uint8_t result)
{
    // Every normal-result exit reloads the global pointer. Read the exposed saved
    // guard only after the current guard, since lower calls can write through frame.
    const uint64_t *guard_pointer = present_finish_guard_pointer.get();
    const uint64_t current = read_present_finish_guard(guard_pointer);
    const uint64_t saved = frame.saved_guard.get();
    check_present_finish_guard_values(current, saved, "render_managed_present_capacity");
    return result;
}
}  // namespace

extern "C" uint8_t bb_render_managed_present_capacity(render::GpuCommandState *buffer,
                                                      uint32_t, uintptr_t state_argument)
{
    using namespace render;
    auto *state = reinterpret_cast<CommandFinishStateView *>(state_argument);
    auto *buffer_view = reinterpret_cast<CommandFinishStateView *>(buffer);
    ManagedCapacityFrame frame;
    const uint64_t *initial_guard_pointer = present_finish_guard_pointer.get();
    frame.saved_guard.set(read_present_finish_guard(initial_guard_pointer));

    void *processor = state->item_processor.get();
    const uintptr_t begin = buffer_view->buffer.begin.get();
    frame.saved_record.set(managed_capacity_calls::capture_managed_capacity_record(processor, begin));
    const uint64_t slots = reinterpret_cast<ManagedCapacityProcessorView *>(processor)->slots.get();
    frame.acquired_buffer = 0;
    uint32_t pop_result = managed_capacity_calls::pop_managed_capacity_buffer(
        processor, &frame.acquired_buffer);
    while (pop_result != 0) {
        managed_capacity_calls::managed_capacity_retry_step();
        pop_result = managed_capacity_calls::pop_managed_capacity_buffer(
            processor, &frame.acquired_buffer);
    }

    const uint64_t capacity_words = (slots * block_slot_bytes - block_slot_bytes) /
                                    command_word_bytes;
    // The acquired QWORD is captured before any allocator call can mutate frame.
    const uintptr_t acquired = frame.acquired_buffer;
    const uint32_t link_index = state->block_link_index.get();
    const uint64_t allocation_bytes = static_cast<uint64_t>(link_index) * block_slot_bytes +
                                      block_slot_bytes;
    const bool had_head = state->block_head.get() != 0;
    uint32_t count = 0;
    bool allocate_block = !had_head;
    if (had_head) {
        count = state->last_block_count.get();
        allocate_block = count == link_index;
    }

    uintptr_t block;
    if (allocate_block) {
        FinishBlockOwner *owner = state->block_owner.get();
        auto *owner_view = reinterpret_cast<ManagedCapacityOwnerView *>(owner);
        ManagedCapacityOwnerVtable *vtable = owner_view->vtable.get();
        const auto allocate = vtable->allocate.get();
        block = allocate(owner, allocation_bytes, allocation_alignment);
        if (block == 0) return finish_managed_capacity(frame, 0);
        managed_capacity_block_slot(block, link_index)->value.set(0);
        if (had_head) {
            // Native reloads the tail after clearing the new block's link slot.
            const uintptr_t tail = state->word_b4e0.get();
            managed_capacity_block_slot(tail, link_index)->value.set(block);
        } else {
            state->block_head.set(block);
        }
        state->word_b4e0.set(block);
        state->last_block_count.set(0);
        count = 0;
    } else {
        block = state->word_b4e0.get();
    }

    const uintptr_t slot_offset = static_cast<uintptr_t>(count) * block_slot_bytes;
    state->last_block_count.set(count + 1u);
    const uintptr_t slot_address = block + slot_offset;
    // The count publication precedes this admission; there is no rollback on zero.
    if (slot_address == 0) return finish_managed_capacity(frame, 0);
    reinterpret_cast<FinishBlockSlot *>(slot_address)->value.set(frame.saved_record.get());

    capture_managed_capacity_snapshot(frame.snapshot, buffer_view->buffer);
    buffer_view->buffer.begin.set(acquired);
    buffer_view->buffer.cursor.set(acquired);
    buffer_view->buffer.limit.set(acquired + capacity_words * command_word_bytes);
    const uint32_t flags = state->submission_flags.get();
    submit_present_finish_snapshot(reinterpret_cast<GpuCommandState *>(state_argument), buffer,
                                   &frame.snapshot, flags);
    const uintptr_t fresh_cursor = buffer_view->buffer.cursor.get();
    buffer_view->buffer.begin.set(fresh_cursor);
    state->last_reset_cursor.set(fresh_cursor);
    return finish_managed_capacity(frame, 1);
}
