// SPDX-License-Identifier: GPL-2.0-or-later
// Geometry descriptors and aggregate sizes derived from packed records. Lower
// helpers retain their partial-output and ignored-status behavior.
#pragma once

#include <cstdint>

#include "record_descriptor_accessors.h"
#include "record_constructor_helpers.h"
#include "record_geometry_helpers.h"

namespace render {
RT_ORIGINAL(0x014a1e80, initialize_record_level_descriptor,
            uint32_t(RecordGeometryDescriptor *output, const RecordDescriptorPrefix *record,
                     uint32_t level, uint32_t index));
RT_ORIGINAL(0x0149cc60, convert_record_descriptor_classification,
            uint32_t(GeometryWord *output, uint32_t original, uint32_t requested));
RT_ORIGINAL(0x014a2c40, calculate_record_descriptor_level_offset,
            uint32_t(uint64_t *offset, uint64_t *total, const RecordDescriptorPrefix *record,
                     uint32_t level, uint32_t index));
}  // namespace render
