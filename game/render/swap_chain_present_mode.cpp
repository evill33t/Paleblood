// SPDX-License-Identifier: GPL-2.0-or-later
// Swap-chain facade called by GXSwapChainCtrl: reject unsupported modes before reading the
// object, then forward to the lower-level state. Preserve the complete incoming second
// argument; the original checks its low 32 bits but forwards all 64 bits unchanged.
#include "swap_chain.h"

namespace {
constexpr uint32_t last_present_mode = 4;
constexpr int32_t invalid_present_mode = 8;
}  // namespace

extern "C" int32_t bb_render_swap_chain_present_mode(render::SwapChain *self, uintptr_t source_mode)
{
    const auto mode = static_cast<uint32_t>(source_mode);
    if (mode > last_present_mode) return invalid_present_mode;
    return render::swap_chain_submit(self->state, source_mode, mode);
}
