// SPDX-License-Identifier: GPL-2.0-or-later
// The swap-chain facade and its lower-level presentation state.
#pragma once

#include <cstddef>
#include <cstdint>

#include "runtime/original.h"

namespace render {

struct SwapChainState;

struct SwapChain {
    uint8_t unknown_0x0000[0x33d0];
    SwapChainState *state;
};
static_assert(offsetof(SwapChain, state) == 0x33d0);

// The second argument is forwarded at its original width, even though the facade validates
// only its low 32 bits as a mode. The callee also receives those low bits separately.
RT_ORIGINAL(0x02ad6e50, swap_chain_submit,
            int32_t(SwapChainState *, uintptr_t source, uint32_t mode));

}  // namespace render
