// SPDX-License-Identifier: GPL-2.0-or-later
// Complete two ordered dirty passes using cached control bits and live command fields.
// The known consumer ignores the native sentinel/delegate RAX; this logical-void profile
// preserves its memory/call effects without inventing a numeric status.
#include "present_shader_cleanup.h"

namespace {
constexpr uint32_t first_dirty_pass = 0x80;
constexpr uint32_t second_dirty_pass = 0x10;
constexpr uint32_t counted_event_mask = 0x102;
constexpr uint32_t broad_event_mask = 0x60;
constexpr uint32_t active_preparation = 1;
constexpr uint32_t counted_event = 5;
constexpr uint32_t broad_event = 0xf;
constexpr uint32_t ordinary_event = 1;
constexpr uint32_t unchanged_mode = UINT32_MAX;

void finish_dirty_pass(render::ShaderCleanupCommands *commands, bool counted, bool broad)
{
    const uint32_t mode = commands->preparation_mode.get() | active_preparation;
    commands->preparation_mode.set(mode);
    uint32_t event = commands->preparation_event.get();
    if (counted) {
        event |= counted_event;
        commands->preparation_event.set(event);
        commands->event_count.set(0);
    } else {
        if (broad) {
            event |= broad_event;
            // Retain this second store: the native code publishes the saved mode again
            // before the event, including when the command/state views overlap.
            commands->preparation_mode.set(mode);
        } else {
            event |= ordinary_event;
        }
        commands->preparation_event.set(event);
    }
}
}

extern "C" void bb_render_finish_record_shader(render::RecordShaderState *shader,
                                                render::RecordCommandBuffer *commands)
{
    using namespace render;
    auto *state = reinterpret_cast<ShaderCleanupState *>(shader);
    auto *context = reinterpret_cast<ShaderCleanupCommands *>(commands);
    const uint32_t dirty = state->dirty_flags.get();
    const bool counted = (dirty & counted_event_mask) != 0;
    const bool broad = (dirty & broad_event_mask) != 0;
    if ((dirty & first_dirty_pass) != 0) finish_dirty_pass(context, counted, broad);
    if ((dirty & second_dirty_pass) != 0) finish_dirty_pass(context, counted, broad);
    state->dirty_flags.set(0);
    // Read only after clearing dirty flags and all command writes, since these views alias.
    const uint32_t mode = state->command_mode.get();
    if (mode != unchanged_mode) select_shader_command_mode(commands, mode);
}
