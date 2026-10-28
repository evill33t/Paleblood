// SPDX-License-Identifier: GPL-2.0-or-later
// Reset the embedded state without touching its padding. Load the scalar only
// after the first two stores, so an aliased global still supplies the live bits.
#include <limits>

#include "present_context_reset.h"

namespace {
constexpr uint32_t invalid_word = std::numeric_limits<uint32_t>::max();
constexpr uint64_t invalid_double_word = std::numeric_limits<uint64_t>::max();
constexpr uint64_t packed_state_sentinel = 0x0000ffff0000ffff;
}

extern "C" uint64_t bb_render_reset_present_context_state_b528(render::ContextStateB528 *state)
{
    state->invalid_00.set(invalid_word);
    state->invalid_04.set(invalid_word);
    const uint32_t scalar = render::context_reset_scalar_word.get();
    state->scalar_08.set(scalar);
    state->scalar_0c.set(scalar);
    state->scalar_10.set(scalar);
    state->state_14.set(0);
    state->invalid_18.set(invalid_word);
    state->invalid_20.set(invalid_double_word);
    state->packed_28.set(packed_state_sentinel);
    return packed_state_sentinel;
}
