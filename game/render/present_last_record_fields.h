// SPDX-License-Identifier: GPL-2.0-or-later
// Packed fields used by the last-record presentation path. Their numeric
// meaning is intentionally limited to the bits established by the leaf bodies.
#pragma once

#include "present_finish.h"

namespace render {
struct LastRecordIndexFields {
    uint8_t unknown_0x00[0x20];
    PresentFinishField<uint32_t> indices_20;
};
static_assert(offsetof(LastRecordIndexFields, indices_20) == 0x20);
static_assert(sizeof(LastRecordIndexFields) == 0x24);

struct LastRecordModeWord {
    PresentFinishField<uint32_t> word;
};
static_assert(offsetof(LastRecordModeWord, word) == 0);
static_assert(sizeof(LastRecordModeWord) == 0x4);

struct LastRecordOptionWord {
    PresentFinishField<uint32_t> word;
};
static_assert(offsetof(LastRecordOptionWord, word) == 0);
static_assert(sizeof(LastRecordOptionWord) == 0x4);

// Logical source for the eleven-byte native getter, which remains unhooked.
// All four results are retained; callers may explicitly test them for nonzero.
inline uint32_t read_present_last_record_option_source(const LastRecordOptionWord *record)
{
    constexpr uint32_t option_shift = 2;
    constexpr uint32_t option_mask = 0x3;
    return (record->word.get() >> option_shift) & option_mask;
}
}  // namespace render
