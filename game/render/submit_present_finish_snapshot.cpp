// SPDX-License-Identifier: GPL-2.0-or-later
// Emit required counter waits, then submit the nonempty command regions in order.
// Without optional commands the original initializer constructs an empty second region.
// The caller's flags argument is unused here; native guard RAX is an ignored return.
#include "present_finish_snapshot.h"

namespace {
using namespace render;
constexpr uint32_t counter_sign_bit = 0x80000000;
constexpr uint32_t full_counter_mask = 0xffffffff;
constexpr uint32_t initial_wait_operation = 1;
constexpr uint32_t target_wait_operation = 5;
constexpr uint32_t complete_word_mask = 0xfffffffc;

uint32_t cursor_low(const FinishCommandSnapshot &buffer)
{
    uint32_t value;
    std::memcpy(&value, buffer.cursor.bytes, sizeof(value));
    return value;
}

void prepare_finish_counter_operations(CommandFinishStateView *state, GpuCommandState *commands)
{
    FinishSubmissionOwner *owner = state->owner.get();
    FinishCounterSet *counters = owner->counters.get();
    FinishCounterWord *producer = counters->producer.get();
    FinishCounterWord *observed_pointer = counters->observed.get();
    const uint32_t target = producer->value.get() - 1;
    const uint32_t observed = observed_pointer->value.get();
    if (observed == target) return;

    // The label lies immediately before the two known counter pointers.
    struct CounterLabelView {
        uint8_t unknown_0x00[0x10];
        PresentFinishField<uintptr_t> label;
    };
    static_assert(offsetof(CounterLabelView, label) == 0x10);
    auto *label_state = reinterpret_cast<CounterLabelView *>(counters);
    auto *counter_commands = reinterpret_cast<CommandSubmissionContext *>(commands);
    if ((observed & counter_sign_bit) != 0 && (target & counter_sign_bit) == 0) {
        const uintptr_t label = label_state->label.get();
        finish_snapshot_calls::emit_counter_operation(
            counter_commands, label, full_counter_mask, initial_wait_operation, observed);
    }
    const uintptr_t label = label_state->label.get();
    finish_snapshot_calls::emit_counter_operation(
        counter_commands, label, full_counter_mask, target_wait_operation, target);
    // Both helpers may change this pointer. The captured counter-set object stays live.
    counters->observed.get()->value.set(target);
}

void submit_finish_regions(FinishSnapshotFrame &frame, uintptr_t first_begin,
                           uint32_t first_size, uintptr_t second_begin, uint32_t second_size)
{
    uint32_t count;
    if (first_size != 0 && second_size != 0) {
        frame.begins[0].set(first_begin);
        frame.sizes[0].set(first_size);
        frame.begins[1].set(second_begin);
        frame.sizes[1].set(second_size);
        count = 2;
    } else if (first_size != 0) {
        frame.begins[0].set(first_begin);
        frame.sizes[0].set(first_size);
        count = 1;
    } else if (second_size != 0) {
        frame.begins[0].set(second_begin);
        frame.sizes[0].set(second_size);
        count = 1;
    } else {
        return;
    }
    finish_snapshot_calls::submit_command_regions(count, frame.begins, frame.sizes, 0, 0);
}
}  // namespace

extern "C" void bb_render_submit_present_finish_snapshot(
    render::GpuCommandState *state_argument, render::GpuCommandState *optional_commands,
    render::FinishCommandSnapshot *snapshot, uint32_t)
{
    using namespace render;
    const uint64_t *guard_pointer = present_finish_guard_pointer.get();
    FinishSnapshotFrame frame;
    frame.saved_guard.set(read_present_finish_guard(guard_pointer));

    uintptr_t first_begin;
    uintptr_t second_begin;
    uint32_t first_cursor;
    uint32_t second_cursor;
    if (optional_commands != nullptr) {
        auto *state = reinterpret_cast<CommandFinishStateView *>(state_argument);
        prepare_finish_counter_operations(state, optional_commands);
        auto *buffer = reinterpret_cast<FinishCommandSnapshot *>(optional_commands);
        first_cursor = cursor_low(*buffer);
        first_begin = buffer->begin.get();
        first_cursor -= static_cast<uint32_t>(first_begin);
        second_cursor = cursor_low(*snapshot);
        second_begin = snapshot->begin.get();
    } else {
        // Only this auxiliary word is preinitialized. Other bytes that the original
        // initializer leaves alone remain untouched in the exposed local snapshot.
        frame.empty.word_28.set(0);
        initialize_command_buffer(reinterpret_cast<CommandBufferSnapshot *>(&frame.empty),
                                  0, 0, nullptr, 0);
        first_cursor = cursor_low(*snapshot);
        first_begin = snapshot->begin.get();
        first_cursor -= static_cast<uint32_t>(first_begin);
        second_cursor = cursor_low(frame.empty);
        second_begin = frame.empty.begin.get();
    }
    second_cursor -= static_cast<uint32_t>(second_begin);
    const uint32_t second_size = second_cursor & complete_word_mask;
    const uint32_t first_size = first_cursor & complete_word_mask;
    submit_finish_regions(frame, first_begin, first_size, second_begin, second_size);

    const uint64_t current_guard = read_present_finish_guard(guard_pointer);
    const uint64_t saved_guard = frame.saved_guard.get();
    check_present_finish_guard_values(current_guard, saved_guard,
                                     "render_submit_present_finish_snapshot");
    // Only the known borrowed relative frame bytes are modeled, not native stack
    // identity or arbitrary additional inputs to the opaque lower calls.
}
