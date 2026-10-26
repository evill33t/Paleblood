// SPDX-License-Identifier: GPL-2.0-or-later
// Convert the derived normalization code into a tuple for the original layout helper.
// Unsupported codes return before touching the output. Both original jump tables are
// expressed as a single selector switch; no original table bytes are retained here.
#include "record_parameters.h"

namespace {
constexpr uint32_t unsupported_parameter = 0x80d30000;
constexpr uint32_t fixed_classification = 4;
constexpr uint32_t paired_classification = 3;
constexpr uint32_t small_classification = 2;
constexpr uint32_t large_classification = 4;
struct ParameterTuple {
    uint32_t classification;
    uint32_t first;
    uint32_t second;
};
}  // namespace

extern "C" uint32_t bb_render_record_apply_mapping(uint32_t *output, uint32_t derived,
                                                   uint32_t mapping, uint32_t parameter)
{
    const uint32_t variable_classification = parameter > 1 ? large_classification : small_classification;
    ParameterTuple tuple;
    switch (derived) {
    case 1: tuple = {fixed_classification, 0, 0}; break;
    case 2: tuple = {fixed_classification, 0, 1}; break;
    case 4: tuple = {fixed_classification, 2, 2}; break;
    case 5: tuple = {fixed_classification, 4, 2}; break;
    case 6: tuple = {fixed_classification, 64, 1}; break;
    case 7: case 10: tuple = {variable_classification, 0, 1}; break;
    case 8: case 11: tuple = {paired_classification, 32, 4}; break;
    case 9: case 12: tuple = {variable_classification, 16, 1}; break;
    default: return unsupported_parameter;
    }
    return render::apply_record_parameter_tuple(output, tuple.classification, tuple.first,
                                               mapping, parameter, tuple.second);
}
