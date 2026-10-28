// SPDX-License-Identifier: GPL-2.0-or-later
// Byte-backed reset views for three embedded presentation-context states. Padding
// remains untouched; scalar copies preserve live bits, including NaN payloads.
#pragma once

#include "present_finish.h"

namespace render {
struct ContextStateB518 {
    PresentFinishField<uint64_t> word_00;
    PresentFinishField<uint64_t> word_08;
};
static_assert(offsetof(ContextStateB518, word_00) == 0x00);
static_assert(offsetof(ContextStateB518, word_08) == 0x08);
static_assert(sizeof(ContextStateB518) == 0x10);

struct ContextStateB528 {
    PresentFinishField<uint32_t> invalid_00;
    PresentFinishField<uint32_t> invalid_04;
    PresentFinishField<uint32_t> scalar_08;
    PresentFinishField<uint32_t> scalar_0c;
    PresentFinishField<uint32_t> scalar_10;
    PresentFinishField<uint32_t> state_14;
    PresentFinishField<uint32_t> invalid_18;
    uint8_t unknown_0x1c[0x04];
    PresentFinishField<uint64_t> invalid_20;
    PresentFinishField<uint64_t> packed_28;
};
static_assert(offsetof(ContextStateB528, invalid_00) == 0x00);
static_assert(offsetof(ContextStateB528, invalid_04) == 0x04);
static_assert(offsetof(ContextStateB528, scalar_08) == 0x08);
static_assert(offsetof(ContextStateB528, scalar_0c) == 0x0c);
static_assert(offsetof(ContextStateB528, scalar_10) == 0x10);
static_assert(offsetof(ContextStateB528, state_14) == 0x14);
static_assert(offsetof(ContextStateB528, invalid_18) == 0x18);
static_assert(offsetof(ContextStateB528, invalid_20) == 0x20);
static_assert(offsetof(ContextStateB528, packed_28) == 0x28);
static_assert(sizeof(ContextStateB528) == 0x30);

struct ContextPreparationReset {
    PresentFinishField<uint32_t> invalid_00;
    uint8_t unknown_0x04[0x04];
    PresentFinishField<uint64_t> invalid_08;
    PresentFinishField<uint32_t> slot_indices[8];
    PresentFinishField<uint32_t> scalar_words[4];
    PresentFinishField<uint64_t> state_40;
    PresentFinishField<uint32_t> invalid_48;
    PresentFinishField<uint32_t> invalid_4c;
    PresentFinishField<uint32_t> state_50;
    uint8_t unknown_0x54[0x04];
    PresentFinishField<uint64_t> invalid_58;
    PresentFinishField<uint64_t> banks[2][9];
    PresentFinishField<uint32_t> invalid_f0;
    PresentFinishField<uint32_t> invalid_f4;
    uint8_t unknown_0xf8[0x06];
    PresentFinishField<uint8_t> state_fe;
    PresentFinishField<uint8_t> state_ff;
    uint8_t unknown_0x100[0x40];
    PresentFinishField<uint32_t> scalar_140;
    PresentFinishField<uint16_t> state_144;
    PresentFinishField<uint16_t> packed_146;
    PresentFinishField<uint64_t> slot_states[8];
    PresentFinishField<uint32_t> flags_188;
    uint8_t unknown_0x18c[0x04];
    PresentFinishField<uint64_t> invalid_190;
    PresentFinishField<uint64_t> invalid_198;
};
static_assert(offsetof(ContextPreparationReset, invalid_00) == 0x00);
static_assert(offsetof(ContextPreparationReset, invalid_08) == 0x08);
static_assert(offsetof(ContextPreparationReset, slot_indices) == 0x10);
static_assert(offsetof(ContextPreparationReset, scalar_words) == 0x30);
static_assert(offsetof(ContextPreparationReset, state_40) == 0x40);
static_assert(offsetof(ContextPreparationReset, invalid_48) == 0x48);
static_assert(offsetof(ContextPreparationReset, invalid_4c) == 0x4c);
static_assert(offsetof(ContextPreparationReset, state_50) == 0x50);
static_assert(offsetof(ContextPreparationReset, invalid_58) == 0x58);
static_assert(offsetof(ContextPreparationReset, banks) == 0x60);
static_assert(sizeof(ContextPreparationReset::banks[0]) == 0x48);
static_assert(offsetof(ContextPreparationReset, invalid_f0) == 0xf0);
static_assert(offsetof(ContextPreparationReset, invalid_f4) == 0xf4);
static_assert(offsetof(ContextPreparationReset, state_fe) == 0xfe);
static_assert(offsetof(ContextPreparationReset, state_ff) == 0xff);
static_assert(offsetof(ContextPreparationReset, scalar_140) == 0x140);
static_assert(offsetof(ContextPreparationReset, state_144) == 0x144);
static_assert(offsetof(ContextPreparationReset, packed_146) == 0x146);
static_assert(offsetof(ContextPreparationReset, slot_states) == 0x148);
static_assert(offsetof(ContextPreparationReset, flags_188) == 0x188);
static_assert(offsetof(ContextPreparationReset, invalid_190) == 0x190);
static_assert(offsetof(ContextPreparationReset, invalid_198) == 0x198);
static_assert(sizeof(ContextPreparationReset) == 0x1a0);

RT_GLOBAL(0x04b37e64, context_reset_scalar_word, uint32_t);
}  // namespace render
