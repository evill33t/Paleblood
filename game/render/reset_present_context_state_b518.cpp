// SPDX-License-Identifier: GPL-2.0-or-later
// Reset the two full-QWORD fields and retain the native full-RAX sentinel result.
#include "present_context_reset.h"

namespace {
constexpr uint64_t state_b518_sentinel = 0xffffffff00000000;
}

extern "C" uint64_t bb_render_reset_present_context_state_b518(render::ContextStateB518 *state)
{
    state->word_00.set(0);
    state->word_08.set(state_b518_sentinel);
    return state_b518_sentinel;
}
