// SPDX-License-Identifier: GPL-2.0-or-later
// Emit the context section through its original hardware-state import. Only
// incoming RDI is read locally; incoming RSI is overwritten before use.
// Callbacks/imports retain their original addresses and opaque service effects.
#pragma once

#include "present_finish.h"

namespace render {
struct PresentContextSection;

// This wire captures complete callback RAX, although only AL controls admission.
// An AL-zero failure returns that full value unchanged. This is not a claim about
// ambient upper bits of reconstructed callbacks whose own profile closes only AL.
using PresentContextSectionCapacity = uint64_t (*)(PresentContextSection *,
                                                   uint32_t requested_words,
                                                   uintptr_t argument);
struct PresentContextSection {
    uint8_t unknown_0x00[0x08];
    PresentFinishField<uintptr_t> limit;
    PresentFinishField<uintptr_t> cursor;
    PresentFinishField<PresentContextSectionCapacity> capacity;
    PresentFinishField<uintptr_t> capacity_argument;
};
static_assert(sizeof(uintptr_t) == sizeof(uint64_t));
static_assert(offsetof(PresentContextSection, limit) == 0x08);
static_assert(offsetof(PresentContextSection, cursor) == 0x10);
static_assert(offsetof(PresentContextSection, capacity) == 0x18);
static_assert(offsetof(PresentContextSection, capacity_argument) == 0x20);
static_assert(sizeof(PresentContextSection) == 0x28);

RT_ORIGINAL(0x01472870, emit_present_context_section, uint64_t(PresentContextSection *));
namespace context_section_calls {
// Exact PLT/GOT/NID binding: sceGnmDrawInitDefaultHardwareState200. The caller
// forwards full RSI, then consumes only returned EAX. No import body, extra
// ambient arguments or native service behavior is supplied by this declaration.
RT_ORIGINAL(0x02fc04a8, draw_init_default_hardware_state_200,
            uint32_t(uintptr_t destination, uint64_t available_words));
}  // namespace context_section_calls

// Valid ordinary nonconcurrent backing memory and callable original bindings are
// required. No new capacity guard/fallback, rollback, ambient GP/SIMD/flags or
// physical-frame/fault equivalence is implied. The known full result is either
// callback RAX on AL-zero failure or zero-extended import EAX shifted left two.
}  // namespace render
