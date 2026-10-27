// SPDX-License-Identifier: GPL-2.0-or-later
// Emit the record step, then choose its trailing pair using the actual user-PA
// predicate. The service stays opaque; flags and cursor can change across that call.
#include "present_command_controls.h"
#include "record_step_variant.h"

extern "C" void bb_render_emit_present_record_step(render::RecordCommandBuffer *commands,
                                                   uint32_t mode)
{
    constexpr uint32_t required_words = 5;
    constexpr uint32_t step_header = 0xc0012d00;
    constexpr uint32_t step_flag = 1;
    constexpr uint32_t step_tail = 2;
    constexpr uint32_t user_pa_header = 0xc0004600;
    constexpr uint32_t user_pa_flag = 2;
    constexpr uint32_t user_pa_marker = 0x35;
    constexpr uint32_t fallback_header = 0xc0001000;
    uintptr_t cursor;
    if (!render::reserve_record_control_words(commands, required_words, cursor)) return;
    auto *prefix = reinterpret_cast<render::RecordControlCommandPrefix *>(commands);
    const uint32_t flags = prefix->flags.get();
    auto *packet = reinterpret_cast<render::RecordControlTriple *>(cursor);
    packet->header.set(step_header | (flags & step_flag));
    packet->first.set(mode);
    packet->second.set(step_tail);
    render::advance_record_control_cursor(commands, sizeof(render::RecordControlTriple));

    if (render::record_step_user_pa_enabled() != 0) {
        const uint32_t live_flags = prefix->flags.get();
        const uintptr_t next = prefix->command.cursor.get();
        render::write_record_control_pair(next, user_pa_header | (live_flags & user_pa_flag),
                                          user_pa_marker);
        render::advance_record_control_cursor(commands, sizeof(render::RecordControlPair));
    } else {
        const uintptr_t next = prefix->command.cursor.get();
        render::write_record_control_pair(next, fallback_header, 0);
        // This route publishes the captured next pointer; the user-PA route above
        // performs a live ADD instead. The distinction is observable under overlap.
        prefix->command.cursor.set(next + sizeof(render::RecordControlPair));
    }
}
