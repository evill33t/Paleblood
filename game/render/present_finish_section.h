// SPDX-License-Identifier: GPL-2.0-or-later
// Complete source form of the short command-section reset used during finishing.
// The native twelve-byte entry stays unhooked; its second caller argument is unused.
#pragma once

#include "present_finish.h"

namespace render {
struct PresentFinishSectionReset {
    uint8_t unknown_0x0000[0xb360];
    PresentFinishField<uint64_t> word_b360;
};
static_assert(offsetof(PresentFinishSectionReset, word_b360) == 0xb360);
static_assert(sizeof(PresentFinishSectionReset) == 0xb368);

RT_ORIGINAL(0x02acd3f0, original_present_finish_section_reset,
            void(PresentFinishSectionReset *, GpuCommandState *));

inline void reset_present_finish_section_source(PresentFinishSectionReset *section,
                                                GpuCommandState *)
{
    section->word_b360.set(0);
}
}  // namespace render
