// SPDX-License-Identifier: GPL-2.0-or-later
// Clear only bit13, then OR all eight input bits beginning there. Higher input
// bits can set neighboring fields, so this input must not become a bool.
#include "record_fields.h"

extern "C" uint32_t bb_render_record_set_word_10_byte_at_bit13(render::RecordConstructorPrefix *record,
                                                              uint8_t value)
{
    constexpr uint32_t preserve = 0xffffdfff, byte_shift = 13;
    const uint32_t old_bits = record->pair_10.low.get() & preserve;
    record->pair_10.low.set(old_bits | (uint32_t(value) << byte_shift));
    return old_bits;
}
