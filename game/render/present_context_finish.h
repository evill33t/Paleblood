// SPDX-License-Identifier: GPL-2.0-or-later
// Only the embedded finishing entry addresses are known here. Anchor markers do not
// describe the helper-owned objects beyond their observed positions in the context.
#pragma once

#include "present_finish.h"

namespace render {
struct RecordCommandBuffer;
struct ShaderPreparationState;
struct ContextFinishAnchor { uint8_t opaque; };

struct ContextFinishPrefix {
    uint8_t unknown_0x0000[0x10];
    ContextFinishAnchor commands;
    uint8_t unknown_0x0011[0x3f];
    ContextFinishAnchor shader;
    uint8_t unknown_0x0051[0xb427];
    ContextFinishAnchor banks;
    uint8_t unknown_0xb479[0x9f];
    ContextFinishAnchor state_b518;
    uint8_t unknown_0xb519[0xf];
    ContextFinishAnchor state_b528;
    uint8_t unknown_0xb529[0x2f];
    ContextFinishAnchor preparation;
};
static_assert(offsetof(ContextFinishPrefix, commands) == 0x10);
static_assert(offsetof(ContextFinishPrefix, shader) == 0x50);
static_assert(offsetof(ContextFinishPrefix, banks) == 0xb478);
static_assert(offsetof(ContextFinishPrefix, state_b518) == 0xb518);
static_assert(offsetof(ContextFinishPrefix, state_b528) == 0xb528);
static_assert(offsetof(ContextFinishPrefix, preparation) == 0xb558);
static_assert(sizeof(ContextFinishPrefix) == 0xb559);

// The native frame zeros 72 bytes and places its saved guard immediately after them.
// Preserve that exposed relationship without initializing the remaining frame bytes.
struct alignas(32) ContextFinishScratch {
    uint8_t payload[0x48];
    PresentFinishField<uint64_t> saved_guard;
    uint8_t unknown_0x50[0x10];
};
static_assert(offsetof(ContextFinishScratch, payload) == 0x00);
static_assert(offsetof(ContextFinishScratch, saved_guard) == 0x48);
static_assert(offsetof(ContextFinishScratch, unknown_0x50) == 0x50);
static_assert(alignof(ContextFinishScratch) == 0x20);
static_assert(sizeof(ContextFinishScratch) == 0x60);

namespace context_finish_calls {
RT_ORIGINAL(0x02ad3070, prepare_context_finish,
            void(ContextFinishAnchor *, PresentGpuContext *, ContextFinishScratch *));
// Reuse the established preparation flush's two-pointer profile. Its existing
// source is on the shader progress branch; this namespace names the call here.
RT_ORIGINAL(0x02ab9850, flush_shader_preparation,
            void(ShaderPreparationState *, RecordCommandBuffer *));
RT_ORIGINAL(0x02acd190, finish_context_shader,
            void(ContextFinishAnchor *, PresentGpuContext *));
RT_ORIGINAL(0x02abba10, finish_context_state_b518, void(ContextFinishAnchor *));
RT_ORIGINAL(0x02ad3300, finish_context_state_b528, void(ContextFinishAnchor *));
RT_ORIGINAL(0x02ad2d20, finish_context_preparation, void(ContextFinishAnchor *));
}  // namespace context_finish_calls
}  // namespace render
