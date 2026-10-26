// SPDX-License-Identifier: GPL-2.0-or-later
// Submission delegate: add counter operations to an optional command context, then submit
// that context followed by the saved region. Without that optional context, use the snapshot
// delegate. The by-value flip record occupies the guest's incoming stack, not a GP register.
#include "command_submit.h"

namespace {
using namespace render;

RT_ORIGINAL(0x01477b30, emit_counter_operation,
            void(CommandSubmissionContext *, uintptr_t label, uint32_t mask,
                 uint32_t operation, uint32_t reference));
constexpr uint32_t full_counter_mask = 0xffffffff;
constexpr uint32_t counter_sign_bit = 0x80000000;
constexpr uint32_t initial_wait_operation = 1, target_wait_operation = 5;

void prepare_counter_operations(CommandSubmissionContext *self, CommandSubmissionContext *commands)
{
    CommandCounterState *counters = self->owner->counters;
    const uint32_t target = *counters->producer_count - 1;
    const uint32_t observed = *counters->observed_count;
    if (observed == target) return;
    if ((observed & counter_sign_bit) != 0 && (target & counter_sign_bit) == 0)
        emit_counter_operation(commands, counters->wait_label, full_counter_mask,
                          initial_wait_operation, observed);
    emit_counter_operation(commands, counters->wait_label, full_counter_mask, target_wait_operation, target);
    // A helper may have changed the pointer, so read it again before storing the cached target.
    *counters->observed_count = target;
}

}  // namespace

extern "C" void bb_render_command_submit_batch(render::CommandSubmissionContext *self,
                                                render::CommandSubmissionContext *commands,
                                                const render::CommandBufferSnapshot *snapshot,
                                                render::FlipSubmission submission)
{
    if (commands == nullptr) {
        render::submit_snapshot(&submission, snapshot);
        return;
    }
    prepare_counter_operations(self, commands);
    render::submit_command_regions(commands->buffer, *snapshot, &submission,
                                   render::SubmitHandleArgument::packed_video_and_index);
}
