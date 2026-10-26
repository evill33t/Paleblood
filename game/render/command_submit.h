// SPDX-License-Identifier: GPL-2.0-or-later
// Original command-buffer views shared by the submission delegates. GPU command contents
// remain opaque; these fields describe only the observed cursors, callbacks and counters.
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "runtime/original.h"

namespace render {

struct CommandSubmissionContext;
constexpr uint32_t command_word_bytes = 4;
using CommandSpaceCallback = void (*)(CommandSubmissionContext *, uintptr_t zero, uintptr_t argument);

struct CommandBufferSnapshot {
    uintptr_t begin;
    uintptr_t limit;
    uintptr_t cursor;
    CommandSpaceCallback space_callback;
    uintptr_t callback_argument;
    uint64_t unknown_0x28;
    uint64_t unknown_0x30;
    uint32_t capacity_words;
    uint32_t unknown_0x3c;
};
static_assert(offsetof(CommandBufferSnapshot, begin) == 0x0);
static_assert(offsetof(CommandBufferSnapshot, limit) == 0x8);
static_assert(offsetof(CommandBufferSnapshot, cursor) == 0x10);
static_assert(offsetof(CommandBufferSnapshot, space_callback) == 0x18);
static_assert(offsetof(CommandBufferSnapshot, callback_argument) == 0x20);
static_assert(offsetof(CommandBufferSnapshot, unknown_0x28) == 0x28);
static_assert(offsetof(CommandBufferSnapshot, capacity_words) == 0x38);
static_assert(offsetof(CommandBufferSnapshot, unknown_0x3c) == 0x3c);
static_assert(sizeof(CommandBufferSnapshot) == 0x40);

struct CommandCounterState {
    uint8_t unknown_0x00[0x10];
    uintptr_t wait_label;
    const uint32_t *producer_count;
    uint32_t *observed_count;
};
static_assert(offsetof(CommandCounterState, wait_label) == 0x10);
static_assert(offsetof(CommandCounterState, producer_count) == 0x18);
static_assert(offsetof(CommandCounterState, observed_count) == 0x20);

struct CommandSubmissionOwner {
    uint8_t unknown_0x0000[0xb728];
    CommandCounterState *counters;
};
static_assert(offsetof(CommandSubmissionOwner, counters) == 0xb728);

struct CommandSubmissionContext {
    CommandBufferSnapshot buffer;
    uint8_t unknown_0x0040[0xb450];
    CommandSubmissionOwner *owner;
};
static_assert(offsetof(CommandSubmissionContext, buffer) == 0x0);
static_assert(offsetof(CommandSubmissionContext, owner) == 0xb490);

// A trivial aggregate of this size is passed in memory by the guest's AMD64 ABI, after the
// three register parameters. Padding has no reconstructed value; only named fields are initialized.
struct FlipSubmission {
    uint32_t video_out;
    uint32_t buffer_index;
    uint32_t mode;
    uint64_t flip_arg;
    uint8_t flags;
};
static_assert(std::is_trivially_copyable_v<FlipSubmission>);
static_assert(alignof(FlipSubmission) == 0x8 && sizeof(FlipSubmission) == 0x20);
static_assert(offsetof(FlipSubmission, video_out) == 0x0);
static_assert(offsetof(FlipSubmission, buffer_index) == 0x4);
static_assert(offsetof(FlipSubmission, mode) == 0x8);
static_assert(offsetof(FlipSubmission, flip_arg) == 0x10);
static_assert(offsetof(FlipSubmission, flags) == 0x18);

RT_ORIGINAL(0x01471270, initialize_command_buffer,
            void(CommandBufferSnapshot *, uintptr_t begin, uint32_t size,
                 CommandSpaceCallback callback, uintptr_t callback_argument));

RT_ORIGINAL(0x02ab0e30, submit_snapshot,
            void(const FlipSubmission *, const CommandBufferSnapshot *));

enum class SubmitHandleArgument {
    packed_video_and_index,
    video_only,
};

// Submit only nonempty regions, in the supplied order. The native batch delegate forwards
// the complete handle/index word, while the snapshot delegate zeroextends just the handle.
void submit_command_regions(const CommandBufferSnapshot &first, const CommandBufferSnapshot &second,
                            const FlipSubmission *submission, SubmitHandleArgument handle_argument);

}  // namespace render
