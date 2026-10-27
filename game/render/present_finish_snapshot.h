// SPDX-License-Identifier: GPL-2.0-or-later
// Snapshot finishing keeps counter operations and region submission in named originals.
// Only the low cursor DWORD is read when computing the native wrapped byte lengths.
#pragma once

#include "command_submit.h"
#include "present_command_finish.h"

namespace render {
struct alignas(8) FinishSnapshotFrame {
    PresentFinishField<uint32_t> sizes[2];
    PresentFinishField<uintptr_t> begins[2];
    FinishCommandSnapshot empty;
    PresentFinishField<uint64_t> saved_guard;
};
static_assert(offsetof(FinishSnapshotFrame, sizes) == 0x00);
static_assert(offsetof(FinishSnapshotFrame, begins) == 0x08);
static_assert(offsetof(FinishSnapshotFrame, empty) == 0x18);
static_assert(offsetof(FinishSnapshotFrame, saved_guard) == 0x58);
static_assert(sizeof(FinishSnapshotFrame) == 0x60);

namespace finish_snapshot_calls {
RT_ORIGINAL(0x01477b30, emit_counter_operation,
            void(CommandSubmissionContext *, uintptr_t label, uint32_t mask,
                 uint32_t operation, uint32_t reference));
RT_ORIGINAL(0x01489a10, submit_command_regions,
            void(uint32_t count, PresentFinishField<uintptr_t> *begins,
                 PresentFinishField<uint32_t> *sizes, uint32_t, uint32_t));
}  // namespace finish_snapshot_calls
}  // namespace render
