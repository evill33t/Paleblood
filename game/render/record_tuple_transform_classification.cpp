// SPDX-License-Identifier: GPL-2.0-or-later
// Transform selected classifications after scaling metadata by their original
// group. Multiplication and the following shift wrap at 32 bits before comparison.
#include "record_tuple.h"

extern "C" uint32_t bb_render_record_tuple_transform_classification(uint32_t classification, uint32_t metadata)
{
    constexpr uint32_t admitted_classifications = 0x1cf1;
    constexpr uint32_t scale_shift = 6;
    constexpr uint32_t transformation_threshold = 0x2001;
    constexpr uint32_t upper_threshold = 0x4010;
    const uint32_t index = classification - render::record_tuple_first_classification;
    if (index >= render::record_tuple_group_entries || ((admitted_classifications >> index) & 1) == 0)
        return classification;
    const uint32_t scaled = (metadata * render::record_tuple_groups.get()[index]) << scale_shift;
    if (scaled < transformation_threshold) return classification;
    switch (classification) {
    case 3: return 3;
    case 7: return 4;
    case 8: return scaled < upper_threshold ? 7 : 4;
    case 9: return 5;
    case 10: return 6;
    case 13: return 12;
    case 14: return scaled < upper_threshold ? 13 : 12;
    case 15: return 11;
    default: return classification;
    }
}
