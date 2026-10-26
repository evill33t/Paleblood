// SPDX-License-Identifier: GPL-2.0-or-later
// State views used to flush changed presentation slot values. The table layouts describe
// native address strides; the count byte is not clamped to the declared slot-array lengths.
#pragma once

#include <limits>

#include "present_source.h"

namespace render {
struct PresentSlotBank {
    uintptr_t slots[9];
};
static_assert(sizeof(PresentSlotBank) == 0x48);

struct PresentSourcePreparation {
    uint8_t unknown_0x000[0x60];
    PresentSlotBank banks[2];
    uint8_t unknown_0x0f0[0x10];
    uint64_t pending_values[8];
    uint8_t unknown_0x140[0x6];
    uint8_t slot_count;
    uint8_t unknown_0x147;
    uint64_t cached_values[8];
    uint32_t flags;
};
static_assert(offsetof(PresentSourcePreparation, banks) == 0x60);
static_assert(offsetof(PresentSourcePreparation, pending_values) == 0x100);
static_assert(offsetof(PresentSourcePreparation, slot_count) == 0x146);
static_assert(offsetof(PresentSourcePreparation, cached_values) == 0x148);
static_assert(offsetof(PresentSourcePreparation, flags) == 0x188);

struct PresentSelectedSlot {
    uint8_t unknown_0x00[0x3c];
    uint32_t encoding;
};
static_assert(offsetof(PresentSelectedSlot, encoding) == 0x3c);
struct PresentCommandToken { uint8_t unknown_0x00; };
struct PresentCommandContextPrefix {
    uint8_t unknown_0x00[0x10];
    PresentCommandToken commands;
};
static_assert(offsetof(PresentCommandContextPrefix, commands) == 0x10);

RT_ORIGINAL(0x02564ed0, present_encoding_units, uint32_t(uint32_t encoding));
RT_ORIGINAL(0x01473840, emit_present_slot_value,
            void(PresentCommandToken *, uint32_t index, const uint64_t *value));
}  // namespace render
