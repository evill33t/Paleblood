// SPDX-License-Identifier: GPL-2.0-or-later
// Run the presentation context's seven finishing calls in native order. The scratch
// stays live through them, and the final guard read uses the captured guard pointer.
#include "present_context_finish.h"

extern "C" void bb_render_finish_present_context(render::PresentGpuContext *context)
{
    using namespace render;
    auto *prefix = reinterpret_cast<ContextFinishPrefix *>(context);
    const uint64_t *guard_pointer = present_finish_guard_pointer.get();
    ContextFinishScratch scratch;
    scratch.saved_guard.set(read_present_finish_guard(guard_pointer));
    std::memset(scratch.payload, 0, sizeof(scratch.payload));
    ContextFinishAnchor *preparation = &prefix->preparation;
    context_finish_calls::prepare_context_finish(preparation, context, &scratch);
    auto *commands = reinterpret_cast<GpuCommandState *>(&prefix->commands);
    context_finish_calls::flush_shader_preparation(
        reinterpret_cast<ShaderPreparationState *>(&prefix->banks),
        reinterpret_cast<RecordCommandBuffer *>(commands));
    context_finish_calls::finish_context_shader(&prefix->shader, context);
    finish_present_commands(commands);
    context_finish_calls::finish_context_state_b518(&prefix->state_b518);
    context_finish_calls::finish_context_state_b528(&prefix->state_b528);
    context_finish_calls::finish_context_preparation(preparation);
    const uint64_t current_guard = read_present_finish_guard(guard_pointer);
    const uint64_t saved_guard = scratch.saved_guard.get();
    check_present_finish_guard_values(current_guard, saved_guard, "render_finish_present_context");
}
