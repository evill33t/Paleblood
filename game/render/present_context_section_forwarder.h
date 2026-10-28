// SPDX-License-Identifier: GPL-2.0-or-later
// The complete five-byte native entry is one E9 to emit_present_context_section.
// Keep the unhooked original identity separate from this useful inline source form.
#pragma once

#include "present_context_section.h"

namespace render {
using PresentContextSectionForwarder = uint64_t (*)(PresentContextSection *);
RT_ORIGINAL(0x02abb6c0, original_present_context_section_forwarder,
            uint64_t(PresentContextSection *));

// This is the one-GP profile established by the complete target. It neither
// invents a two-input reset contract nor claims preservation of ambient machine
// inputs at the imported/callback boundaries. Never install its compiled address
// in place of the original five-byte entry.
inline uint64_t present_context_section_forwarder_source(PresentContextSection *section)
{
    return emit_present_context_section(section);
}

inline PresentContextSectionForwarder original_present_context_section_forwarder_address()
{
    return reinterpret_cast<PresentContextSectionForwarder>(
        original_present_context_section_forwarder.address());
}
}  // namespace render
