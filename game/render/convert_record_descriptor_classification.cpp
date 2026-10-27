// SPDX-License-Identifier: GPL-2.0-or-later
// Keep an already matching classification, otherwise select the category's
// conversion. Only successful paths publish the single borrowed output word.
#include "record_descriptor_offsets.h"

extern "C" uint32_t bb_render_convert_record_descriptor_classification(
    render::GeometryWord *output, uint32_t original, uint32_t requested)
{
    using namespace render;
    constexpr uint32_t unsupported = record_geometry_unsupported;
    constexpr uint32_t paired_request = 2, first_paired_result = 9, second_paired_result = 13;
    constexpr uint32_t fixed_result = 5, first_mapped_request = 2, mapped_request_count = 12;
    constexpr uint32_t first_paired_category = 0, second_paired_category = 1;
    constexpr uint32_t fixed_category = 2, mapped_category = 4;
    // These are the semantic outputs of the finite branches, not native jump offsets.
    constexpr uint32_t mapped_results[mapped_request_count] = {
        13, 19, 14, 16, 17, 20, unsupported, unsupported, unsupported, 18, 15, 21
    };
    if (output == nullptr) return unsupported;
    uint32_t actual;
    uint32_t status = get_record_geometry_classification(&actual, original);
    if (status != 0) return status;
    if (actual == requested) {
        output->set(original);
        return 0;
    }
    uint32_t category;
    status = get_record_geometry_category(&category, original);
    if (status != 0) return status;
    uint32_t converted;
    switch (category) {
    case first_paired_category:
        if (requested != paired_request) return unsupported;
        converted = first_paired_result;
        break;
    case second_paired_category:
        if (requested != paired_request) return unsupported;
        converted = second_paired_result;
        break;
    case fixed_category:
        converted = fixed_result;
        break;
    case mapped_category: {
        const uint32_t index = requested - first_mapped_request;
        if (index >= mapped_request_count) return unsupported;
        converted = mapped_results[index];
        if (converted == unsupported) return unsupported;
        break;
    }
    default:
        return unsupported;
    }
    output->set(converted);
    return 0;
}
