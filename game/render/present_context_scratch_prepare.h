// SPDX-License-Identifier: GPL-2.0-or-later
// Two nine-slot banks and the record prefix inspected before context finishing.
// The resource delegates retain explicit call-site profiles; their bodies are opaque.
#pragma once

#include "present_finish.h"

namespace render {
struct ContextScratchResource {
    uint8_t unknown_0x00[0x18];
    uint8_t payload_18[0x23];
    PresentFinishField<uint8_t> flags_3b;
};
static_assert(offsetof(ContextScratchResource, payload_18) == 0x18);
static_assert(offsetof(ContextScratchResource, flags_3b) == 0x3b);

struct ContextScratchParameters {
    PresentFinishField<uint32_t> first;
    PresentFinishField<uint32_t> second;
};
static_assert(sizeof(ContextScratchParameters) == 0x8);
struct ContextScratchRecord {
    uint8_t unknown_0x00[0x8];
    PresentFinishField<ContextScratchResource *> resource;
    uint8_t descriptor_10[0x30];
    PresentFinishField<uint32_t> type_40;
    uint8_t unknown_0x44[0x4];
    ContextScratchParameters parameters_48;
    uint8_t unknown_0x50[0x4];
    PresentFinishField<uint32_t> parameter_54;
    PresentFinishField<uint32_t> parameter_58;
    PresentFinishField<uint32_t> word_5c;
};
static_assert(offsetof(ContextScratchRecord, resource) == 0x8);
static_assert(offsetof(ContextScratchRecord, descriptor_10) == 0x10);
static_assert(offsetof(ContextScratchRecord, type_40) == 0x40);
static_assert(offsetof(ContextScratchRecord, parameters_48) == 0x48);
static_assert(offsetof(ContextScratchRecord, parameter_54) == 0x54);
static_assert(offsetof(ContextScratchRecord, parameter_58) == 0x58);
static_assert(offsetof(ContextScratchRecord, word_5c) == 0x5c);

struct ContextScratchBank {
    PresentFinishField<uintptr_t> slots[8];
    PresentFinishField<uintptr_t> last;
};
static_assert(offsetof(ContextScratchBank, last) == 0x40);
static_assert(sizeof(ContextScratchBank) == 0x48);
struct ContextScratchPrepareState {
    uint8_t unknown_0x00[0x60];
    ContextScratchBank banks[2];
    uint8_t unknown_0xf0[0x98];
    PresentFinishField<uint32_t> flags_188;
};
static_assert(offsetof(ContextScratchPrepareState, banks) == 0x60);
static_assert(offsetof(ContextScratchPrepareState, flags_188) == 0x188);
static_assert(sizeof(uintptr_t) == sizeof(uint64_t));
struct ContextScratchSnapshot {
    PresentFinishField<uintptr_t> slots[9];
};
static_assert(sizeof(ContextScratchSnapshot) == 0x48);
struct alignas(8) ContextScratchPrepareFrame {
    PresentFinishField<uint32_t> first_parameter;
    PresentFinishField<uint32_t> second_parameter;
    PresentFinishField<PresentGpuContext *> context;
};
static_assert(offsetof(ContextScratchPrepareFrame, second_parameter) == 0x4);
static_assert(offsetof(ContextScratchPrepareFrame, context) == 0x8);
static_assert(sizeof(ContextScratchPrepareFrame) == 0x10);

namespace context_scratch_calls {
// The already-closed state body reads only these two GP inputs; its third native
// incoming register is not set by this caller and is not represented as zero.
RT_ORIGINAL(0x02ad1f80, refresh_state,
            void(ContextScratchPrepareState *, PresentGpuContext *));
RT_ORIGINAL(0x02ab5a70, submit_slot,
            uintptr_t(PresentGpuContext *, void *descriptor, void *resource_data,
                      uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x02ab6810, submit_last,
            uintptr_t(PresentGpuContext *, void *descriptor, void *resource_data,
                      uint32_t, uint32_t, uint32_t));
}  // namespace context_scratch_calls
// Known caller ignores RAX. This source nevertheless returns the selected ninth
// pointer on skips, and the full original tail-delegate RAX when it is called.
// Extra ambient inputs and native physical frame/tail-stack identity are unclaimed.
}  // namespace render
