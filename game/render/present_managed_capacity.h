// SPDX-License-Identifier: GPL-2.0-or-later
// Managed presentation capacity at the existing command-space callback boundary.
// These are explicit call-site profiles, not complete lower-helper ABIs. Extra
// inherited GP/SIMD inputs, allocator effects and native physical-frame aliases
// remain unclosed. Only the callback's stable low-byte result is reconstructed.
#pragma once

#include "present_command_finish.h"
#include "present_retry_forwarder.h"

namespace render {
RT_ORIGINAL(0x02aaf030, managed_present_capacity,
            uint8_t(GpuCommandState *, uint32_t requested_words, uintptr_t state_argument));

struct ManagedCapacityProcessorView {
    uint8_t unknown_0x00[0x08];
    PresentFinishField<uint64_t> slots;
};
static_assert(offsetof(ManagedCapacityProcessorView, slots) == 0x08);
static_assert(sizeof(ManagedCapacityProcessorView) == 0x10);

using ManagedCapacityAllocate = uintptr_t (*)(FinishBlockOwner *, uint64_t bytes,
                                             uint32_t alignment);
struct ManagedCapacityOwnerVtable {
    uint8_t unknown_0x00[0x58];
    PresentFinishField<ManagedCapacityAllocate> allocate;
};
static_assert(offsetof(ManagedCapacityOwnerVtable, allocate) == 0x58);
struct ManagedCapacityOwnerView {
    PresentFinishField<ManagedCapacityOwnerVtable *> vtable;
};
static_assert(offsetof(ManagedCapacityOwnerView, vtable) == 0x00);

// The pop helper borrows the acquired-buffer slot. Preserve its known surrounding
// fields so writes through that borrowed address remain visible to later reads.
// No unrelated native stack/spill geometry or zeroed snapshot bytes are implied.
struct alignas(8) ManagedCapacityFrame {
    PresentFinishField<uint64_t> saved_record;
    uintptr_t acquired_buffer;
    FinishCommandSnapshot snapshot;
    PresentFinishField<uint64_t> saved_guard;
};
static_assert(offsetof(ManagedCapacityFrame, saved_record) == 0x00);
static_assert(offsetof(ManagedCapacityFrame, acquired_buffer) == 0x08);
static_assert(offsetof(ManagedCapacityFrame, snapshot) == 0x10);
static_assert(offsetof(ManagedCapacityFrame, saved_guard) == 0x50);
static_assert(alignof(ManagedCapacityFrame) == 0x08);
static_assert(sizeof(ManagedCapacityFrame) == 0x58);

namespace managed_capacity_calls {
RT_ORIGINAL(0x02ad3d40, capture_managed_capacity_record,
            uint64_t(void *processor, uintptr_t begin));
RT_ORIGINAL(0x02ad3b80, pop_managed_capacity_buffer,
            uint32_t(void *processor, uintptr_t *acquired_buffer));
// The caller performs no explicit argument setup and ignores the return. This is
// not a claim that the uninspected lower body has no inherited register inputs.
inline void managed_capacity_retry_step()
{
    retry_present_capacity_source();
}
}  // namespace managed_capacity_calls
}  // namespace render
