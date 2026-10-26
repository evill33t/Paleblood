// SPDX-License-Identifier: GPL-2.0-or-later
// Clear only bit14, then OR the full input byte at that position. Neighboring
// bits retain their old values unless the shifted input sets them.
#include "record_fields.h"

extern "C" uint32_t bb_render_record_set_word_10_byte_at_bit14(render::RecordConstructorPrefix *record,
                                                              uint8_t value)
{
    constexpr uint32_t preserve = 0xffffbfff, byte_shift = 14;
    const uint32_t old_bits = record->pair_10.low.get() & preserve;
    record->pair_10.low.set(old_bits | (uint32_t(value) << byte_shift));
    return old_bits;
}
