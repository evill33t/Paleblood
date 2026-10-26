// SPDX-License-Identifier: GPL-2.0-or-later
// Original numeric groups shared by the tuple classifier and transform.
// Their role in physical resource layout remains opaque.
#pragma once

#include <cstdint>

#include "runtime/original.h"

namespace render {
constexpr uint32_t record_tuple_first_classification = 3;
constexpr uint32_t record_tuple_group_entries = 13;
using RecordTupleGroups = uint32_t[record_tuple_group_entries];
RT_GLOBAL(0x04b2cb60, record_tuple_groups, RecordTupleGroups);
}  // namespace render
