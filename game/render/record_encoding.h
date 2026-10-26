// SPDX-License-Identifier: GPL-2.0-or-later
// The record encoding's first byte selects a bounded original metadata table.
// The numeric entries remain opaque until the geometry consumers are reconstructed.
#pragma once

#include <cstdint>

#include "runtime/original.h"

namespace render {
constexpr uint32_t record_encoding_metadata_entries = 59;
using RecordEncodingMetadataTable = uint32_t[record_encoding_metadata_entries];
RT_GLOBAL(0x04b2bdd0, record_encoding_metadata, RecordEncodingMetadataTable);
}  // namespace render
