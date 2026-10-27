// SPDX-License-Identifier: GPL-2.0-or-later
// Return a drained node through the manager's original head-exchange helper, then
// pass it to the command packet helper. Both lower bodies remain opaque.
#pragma once

#include "present_finish.h"

namespace render {
struct FinishReleaseNode;

struct FinishNodeHead {
    PresentFinishField<uintptr_t> value;
};
struct FinishNodeManager {
    uint8_t unknown_0x00[0x98];
    FinishNodeHead head;
};
struct FinishReleaseNode {
    PresentFinishField<uint64_t> word_00;
    uint8_t unknown_0x08[0x20];
    PresentFinishField<uintptr_t> next;
};
static_assert(sizeof(uintptr_t) == sizeof(uint64_t));
static_assert(sizeof(FinishNodeHead) == 0x8);
static_assert(offsetof(FinishNodeManager, head) == 0x98);
static_assert(offsetof(FinishReleaseNode, word_00) == 0);
static_assert(offsetof(FinishReleaseNode, next) == 0x28);
static_assert(sizeof(FinishReleaseNode) == 0x30);

RT_ORIGINAL(0x0247ebc0, exchange_present_finish_node_head,
            uintptr_t(FinishNodeHead *, FinishReleaseNode *, uintptr_t expected));
RT_ORIGINAL(0x014778f0, emit_released_present_finish_node,
            uintptr_t(GpuCommandState *, uint32_t, uint32_t, FinishReleaseNode *,
                      uint32_t, uint32_t, uint32_t, uint32_t));

// The packet helper's final two parameters establish only the low DWORD of each
// eight-byte stack slot. Upper slot padding and other ambient inputs are unclaimed.
}  // namespace render
