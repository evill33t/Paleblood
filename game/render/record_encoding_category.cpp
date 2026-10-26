// SPDX-License-Identifier: GPL-2.0-or-later
// Decode the encoding's category nibble. Rejected nibbles leave the optional
// output untouched; accepted categories can be queried without an output.
#include <cstdint>
#include <cstring>

extern "C" uint8_t bb_render_record_encoding_category(const uint32_t *encoding, uint32_t *output)
{
    constexpr uint32_t category_shift = 8;
    constexpr uint32_t category_mask = 0xf;
    uint32_t packed;
    std::memcpy(&packed, encoding, sizeof(packed));
    uint32_t category;
    switch ((packed >> category_shift) & category_mask) {
    case 0: category = 0; break;
    case 1: category = 1; break;
    case 4: category = 4; break;
    case 5: category = 5; break;
    case 7: category = 7; break;
    case 9: category = 6; break;
    default: return 0;
    }
    if (output != nullptr) std::memcpy(output, &category, sizeof(category));
    return 1;
}
