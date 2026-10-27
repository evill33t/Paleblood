// SPDX-License-Identifier: GPL-2.0-or-later
// Initialize exactly one borrowed DWORD. A non-NULL output is cleared before
// selector admission, so rejected selectors still publish zero.
#include <cstring>

#include "record_descriptor_helpers.h"
#include "record_geometry_helpers.h"

extern "C" uint32_t bb_render_initialize_record_geometry_flags(
    uint32_t *output, uint32_t selector, uint32_t parameter)
{
    constexpr uint32_t selector_count = 12, first_selector = 1;
    constexpr uint32_t parameter_flag = 0x1000, first_mode = 2, second_mode = 4;
    constexpr uint32_t third_mode = 0x40, first_combination = 0x20, second_combination = 0x10;
    if (output == nullptr) return render::record_geometry_unsupported;
    const uint32_t zero = 0;
    std::memcpy(output, &zero, sizeof(zero));
    if (selector - first_selector >= selector_count) return render::record_geometry_unsupported;

    const uint32_t flag = parameter == 1 ? parameter_flag : 0;
    uint32_t value;
    switch (selector) {
    case 1:
    case 2: return 0;
    case 4: value = first_mode; break;
    case 5: value = second_mode; break;
    case 6: value = third_mode; break;
    case 7:
    case 10: value = flag; break;
    case 8:
    case 11: value = flag | first_combination; break;
    case 9:
    case 12: value = flag | second_combination; break;
    default: return render::record_geometry_unsupported;
    }
    std::memcpy(output, &value, sizeof(value));
    return 0;
}
