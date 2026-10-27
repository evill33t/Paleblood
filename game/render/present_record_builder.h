// SPDX-License-Identifier: GPL-2.0-or-later
// Builder-owned leading payload fields supplement the shared preparation view.
// The builder delegates all writes to the 44-byte record prefix; its caller's
// remaining four scratch bytes have no initialized-value promise.
#pragma once

#include <cstddef>
#include <cstdint>

#include "present_source.h"
#include "record_builder_selectors.h"
#include "record_fields.h"
#include "record_descriptor_offsets.h"
#include "record_descriptor_helpers.h"

namespace render {
struct PresentBuilderPayloadPrefix {
    uint8_t unknown_00[8];
    GeometryTotal base_08;
    uint8_t unknown_10[0x10];
    uint8_t mode_20;
    uint8_t parameter_21;
    uint8_t parameter_22;
    uint8_t parameter_23;
    GeometryWord flags_24;
    GeometryWord supplied_28;
};
static_assert(sizeof(PresentBuilderPayloadPrefix) == 0x2c);
static_assert(alignof(PresentBuilderPayloadPrefix) == 1);
static_assert(offsetof(PresentBuilderPayloadPrefix, base_08) == 0x08);
static_assert(offsetof(PresentBuilderPayloadPrefix, mode_20) == 0x20);
static_assert(offsetof(PresentBuilderPayloadPrefix, parameter_21) == 0x21);
static_assert(offsetof(PresentBuilderPayloadPrefix, parameter_22) == 0x22);
static_assert(offsetof(PresentBuilderPayloadPrefix, parameter_23) == 0x23);
static_assert(offsetof(PresentBuilderPayloadPrefix, flags_24) == 0x24);
static_assert(offsetof(PresentBuilderPayloadPrefix, supplied_28) == 0x28);
static_assert(offsetof(PresentSourcePayloadView, unknown_0x00) == 0);

RT_ORIGINAL(0x025710a0, record_normalize_mode2,
            void(NormalizedRecordMode2 *, const RecordMode2Input *, const uint32_t *, uint32_t));
RT_ORIGINAL(0x025711d0, record_normalize_mode3,
            void(NormalizedRecordMode3 *, const RecordMode3Input *, const uint32_t *, uint32_t));
RT_ORIGINAL(0x02571340, record_normalize_mode4,
            void(NormalizedRecordMode4 *, const RecordMode4Input *, const uint32_t *, uint32_t));
RT_ORIGINAL(0x01485f90, record_constructor,
            uint64_t(RecordConstructorPrefix *, uint32_t first, uint32_t second, uint32_t multiplier,
                     uint32_t encoding, uint32_t classification, uint32_t first_selector,
                     uint32_t second_selector, uint64_t *first_auxiliary, uint64_t *second_auxiliary));
RT_ORIGINAL(0x01487ee0, initialize_pair_record_descriptor,
            uint64_t(RecordDescriptorPrefix *, uint32_t first, uint32_t second, uint32_t third,
                     uint32_t count, uint32_t encoding, uint32_t classification,
                     uint32_t level, uint8_t flag));
RT_ORIGINAL(0x01488590, initialize_triple_record_descriptor,
            uint64_t(RecordDescriptorPrefix *, uint32_t first, uint32_t second, uint32_t third,
                     uint32_t count, uint32_t encoding, uint32_t classification));
}  // namespace render
