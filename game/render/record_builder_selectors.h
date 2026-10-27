// SPDX-License-Identifier: GPL-2.0-or-later
// Scalar selectors used by the present-record builder. Both original tables have
// four guarded DWORD entries; their values remain read from the original image.
#pragma once

#include <cstdint>

#include "record_normalization.h"

namespace render {
constexpr uint32_t record_builder_selector_entries = 4;
using RecordBuilderSelectorValues = uint32_t[record_builder_selector_entries];
RT_GLOBAL(0x04c6fb08, record_builder_secondary_values, RecordBuilderSelectorValues);
RT_GLOBAL(0x04c6ff14, record_builder_scale_values, RecordBuilderSelectorValues);

RT_ORIGINAL(0x02ab7a20, choose_record_constructor_selector,
            uint32_t(uint32_t selector, uint32_t flag));

// The native one-byte RET stays unhooked. This inline form is only for observed
// builder calls that discard the ambient return; it does not promise preservation
// of full incoming RAX or the other machine-register details of that native entry.
inline void record_builder_error_noop(uint32_t) {}
}  // namespace render
