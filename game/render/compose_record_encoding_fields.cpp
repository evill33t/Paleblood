// SPDX-License-Identifier: GPL-2.0-or-later
// Assemble six scalar fields into an encoding word. The first field is a byte;
// the other fields are masked independently rather than checked or normalized.
#include "record_descriptor_accessors.h"

extern "C" uint32_t bb_render_compose_record_encoding_fields(
    uint8_t first, uint32_t second, uint32_t third,
    uint32_t fourth, uint32_t fifth, uint32_t sixth)
{
    constexpr uint32_t second_mask = 15, component_mask = 7;
    constexpr unsigned second_shift = 8, third_shift = 12, fourth_shift = 15;
    constexpr unsigned fifth_shift = 18, sixth_shift = 21;
    return uint32_t(first) | ((second & second_mask) << second_shift)
        | ((third & component_mask) << third_shift) | ((fourth & component_mask) << fourth_shift)
        | ((fifth & component_mask) << fifth_shift) | ((sixth & component_mask) << sixth_shift);
}
