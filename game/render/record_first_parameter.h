// SPDX-License-Identifier: GPL-2.0-or-later
// The native getter reads exactly one unaligned halfword and zero-extends to DWORD.
// Its five-byte entry stays unhooked; the reconstructed consumer uses this inline form.
#pragma once

#include "present_record_field_views.h"

namespace render {
inline uint32_t record_first_parameter(const PresentRecordView *record)
{
    return present_record_fields(record)->first_18.get();
}
}  // namespace render
