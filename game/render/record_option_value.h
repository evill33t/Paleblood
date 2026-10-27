// SPDX-License-Identifier: GPL-2.0-or-later
// Native BEXTR uses a DWORD load with start15/length2. Express that fixed extraction
// without a host BMI requirement. The twelve-byte entry remains unhooked.
#pragma once

#include "present_record_field_views.h"

namespace render {
inline uint32_t record_option_value(const PresentRecordView *record)
{
    constexpr uint32_t field_shift = 15;
    constexpr uint32_t field_mask = 3;
    return (present_record_fields(record)->options_14.get() >> field_shift) & field_mask;
}
}  // namespace render
