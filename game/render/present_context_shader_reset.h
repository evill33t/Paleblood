// SPDX-License-Identifier: GPL-2.0-or-later
// Byte-backed state fields and exact requested fill spans for the context reset.
// Keep each original fill call separate; imported implementation effects are opaque.
#pragma once

#include "present_finish.h"
#include "present_context_section_forwarder.h"

namespace render {
struct ContextShaderResetGroup {
    PresentFinishField<uint64_t> words[5];
    uint8_t payload[0x610];
    uint8_t unknown_0x638[0x8];
};
static_assert(offsetof(ContextShaderResetGroup, payload) == 0x28);
static_assert(sizeof(ContextShaderResetGroup) == 0x640);
struct ContextShaderResetState {
    PresentFinishField<uint32_t> word_00;
    PresentFinishField<uint32_t> word_04;
    PresentFinishField<uint32_t> word_08;
    uint8_t unknown_0x0c[0x4];
    PresentFinishField<uint64_t> word_10;
    ContextShaderResetGroup groups[6];
    uint8_t unknown_0x2598[0x8];
    PresentFinishField<uint32_t> word_25a0;
    uint8_t unknown_0x25a4[0x4];
    PresentFinishField<uint64_t> word_25a8;
    uint8_t block_25b0[0x108];
    uint8_t unknown_0x26b8[0x8];
    uint8_t block_26c0[0x20e];
    uint8_t unknown_0x28ce[0x2];
    PresentFinishField<uint64_t> word_28d0;
    PresentFinishField<uint64_t> word_28d8;
    uint8_t unknown_0x28e0[0x7bc0];
    uint8_t block_a4a0[0xe00];
};
static_assert(offsetof(ContextShaderResetState, word_04) == 0x4);
static_assert(offsetof(ContextShaderResetState, word_08) == 0x8);
static_assert(offsetof(ContextShaderResetState, word_10) == 0x10);
static_assert(offsetof(ContextShaderResetState, groups) == 0x18);
static_assert(offsetof(ContextShaderResetState, word_25a0) == 0x25a0);
static_assert(offsetof(ContextShaderResetState, word_25a8) == 0x25a8);
static_assert(offsetof(ContextShaderResetState, block_25b0) == 0x25b0);
static_assert(offsetof(ContextShaderResetState, block_26c0) == 0x26c0);
static_assert(offsetof(ContextShaderResetState, word_28d0) == 0x28d0);
static_assert(offsetof(ContextShaderResetState, word_28d8) == 0x28d8);
static_assert(offsetof(ContextShaderResetState, block_a4a0) == 0xa4a0);
static_assert(sizeof(ContextShaderResetState) == 0xb2a0);
struct ContextShaderResetCommands {
    uint8_t unknown_0x00[0x10];
    uint8_t section_10[0x28];
    PresentFinishField<uint8_t> flags_38;
};
static_assert(offsetof(ContextShaderResetCommands, section_10) == 0x10);
static_assert(offsetof(ContextShaderResetCommands, flags_38) == 0x38);

namespace context_shader_reset_calls {
// Preserve the existing reset caller's two-argument call shape. The complete
// native section target overwrites incoming RSI before using it, so this context
// input is deliberately ignored. No pointer to this wrapper is stored or installed.
inline uintptr_t reset_section(void *section, PresentGpuContext *)
{
    return present_context_section_forwarder_source(
        reinterpret_cast<PresentContextSection *>(section));
}
// Actual title-libc PLT; no host memset or firmware-Libc substitution.
RT_ORIGINAL(0x02fbe688, fill_bytes, uintptr_t(void *, int32_t value, uint64_t bytes));
}  // namespace context_shader_reset_calls
// The first helper's result and the first eight fill results are ignored. The
// final original fill's complete opaque RAX is returned; no status is invented.
}  // namespace render
