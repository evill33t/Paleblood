// SPDX-License-Identifier: GPL-2.0-or-later
// Flush the pending barrier/range before the event. Native mode zero skips the range
// reset, and a suppressed state returns without touching either group of fields.
#include "shader_preparation.h"

namespace {
constexpr uint32_t invalid_begin = UINT32_MAX;
constexpr uint32_t full_barrier = 0x28;
constexpr uint32_t small_barrier = 4;
constexpr uint32_t range_control_shift = 6;
constexpr uint32_t range_flags_shift = 18;
constexpr uint32_t range_flag_mask = 0x04000000;
constexpr uint32_t active_range_flag = 0x02000000;
constexpr uint32_t preliminary_event = 0x10;
constexpr uint32_t event51 = 0x33;
constexpr uint32_t event27 = 0x1b;
constexpr uint32_t event59 = 0x3b;
constexpr uint32_t event56 = 0x38;
}

extern "C" void bb_render_flush_shader_preparation(render::ShaderPreparationState *opaque_state,
                                                    render::RecordCommandBuffer *commands)
{
    using namespace render;
    auto *state = reinterpret_cast<ShaderPreparationFields *>(opaque_state);
    if (state->suppressed != 0) return;
    const uint32_t mode = state->mode.get();
    if (mode != 0) {
        if (mode == 1 || mode == 3)
            emit_shader_preparation_barrier(commands, full_barrier, 0);
        else if (mode == 7)
            emit_shader_preparation_barrier(commands, small_barrier, 0);
        if (mode == 2 || mode == 3) {
            const uint32_t second_begin = state->second_begin.get();
            const uint32_t first_begin = state->first_begin.get();
            const uint32_t begin = first_begin < second_begin ? first_begin : second_begin;
            if (begin != invalid_begin) {
                const uint32_t flags = state->range_flags.get();
                const uint32_t first_end = state->first_end.get();
                const uint32_t second_end = state->second_end.get();
                const uint32_t end = first_end > second_end ? first_end : second_end;
                const uint32_t control = flags << range_control_shift;
                const uint32_t range_bits = ((flags << range_flags_shift) & range_flag_mask) |
                    (static_cast<uint8_t>(flags) != 0 ? active_range_flag : 0);
                emit_shader_preparation_range(commands, begin, end - begin, control, 0, range_bits, 1);
            }
        }
        // Each native QWORD store updates a pair. This preserves pair publication if
        // the original command emitters retained an alias of this preparation state.
        const uint64_t reset_pair = invalid_begin;
        std::memcpy(&state->first_begin, &reset_pair, sizeof(reset_pair));
        std::memcpy(&state->second_begin, &reset_pair, sizeof(reset_pair));
        const uint64_t zero = 0;
        std::memcpy(&state->range_flags, &zero, sizeof(zero));
    }
    const uint32_t event = state->event.get();
    if (event - 1 <= 14) {
        const uint32_t argument = state->event_argument.get();
        const uint32_t count = state->event_count.get();
        uint32_t selected = 0;
        switch (event) {
        case 1: selected = preliminary_event; break;
        case 2: selected = event51; break;
        case 3:
            emit_shader_preparation_event(commands, preliminary_event, 0, 1);
            selected = event51; break;
        case 4: selected = event27; break;
        case 5:
            emit_shader_preparation_event(commands, preliminary_event, 0, 1);
            selected = event27; break;
        case 6: selected = event59; break;
        case 7:
            emit_shader_preparation_event(commands, preliminary_event, 0, 1);
            selected = event59; break;
        case 15: selected = event56; break;
        default: break;
        }
        if (selected != 0) emit_shader_preparation_event(commands, selected, argument, count);
    }
    const uint64_t zero = 0;
    std::memcpy(&state->event, &zero, sizeof(zero));
    state->event_count.set(1);
}
