// SPDX-License-Identifier: GPL-2.0-or-later
// Source form of the short native secondary getter used by the consumer.
// Only AL is defined as the result; native upper RAX is ambient. Entry stays unhooked.
#pragma once

#include "present_record_field_views.h"

namespace render {
inline uint8_t record_secondary_byte(const PresentRecordView *record)
{
    constexpr uint8_t flag_mask = 0x40;
    constexpr uint32_t flag_shift = 6;
    return static_cast<uint8_t>((present_record_fields(record)->flags_11.get() & flag_mask)
                                >> flag_shift);
}
}  // namespace render
