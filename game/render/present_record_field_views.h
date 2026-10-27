// SPDX-License-Identifier: GPL-2.0-or-later
// Narrow read views for presentation-record getters. The consumer also copies
// wider record words, so byte-backed fields retain unaligned and overlapping reads.
#pragma once

#include "present_preparation_packets.h"

namespace render {
struct PresentRecordFieldPrefix {
    uint8_t unknown_0x00[0x11];
    PreparationField<uint8_t> flags_11;
    uint8_t unknown_0x12[0x2];
    PreparationField<uint32_t> options_14;
    PreparationField<uint16_t> first_18;
    PreparationField<uint16_t> second_1a;
};
static_assert(offsetof(PresentRecordFieldPrefix, flags_11) == 0x11);
static_assert(offsetof(PresentRecordFieldPrefix, options_14) == 0x14);
static_assert(offsetof(PresentRecordFieldPrefix, first_18) == 0x18);
static_assert(offsetof(PresentRecordFieldPrefix, second_1a) == 0x1a);
static_assert(sizeof(PresentRecordFieldPrefix) == 0x1c);

inline const PresentRecordFieldPrefix *present_record_fields(const PresentRecordView *record)
{
    return reinterpret_cast<const PresentRecordFieldPrefix *>(record);
}
}  // namespace render
