// SPDX-License-Identifier: GPL-2.0-or-later
// Byte-backed views for the presentation command-finishing path. List callbacks and
// snapshot submission remain named originals; resource and GPU meanings are unproved.
#pragma once

#include <bit>

#include "present_finish.h"

namespace render {
struct FinishDrainNode {
    uint8_t unknown_0x00[0x28];
    PresentFinishField<FinishDrainNode *> next;
};
static_assert(offsetof(FinishDrainNode, next) == 0x28);
static_assert(sizeof(FinishDrainNode) == 0x30);

struct FinishCommandSection;
struct FinishBlockOwner;
using FinishBlockRelease = void (*)(FinishBlockOwner *, void *block);
struct FinishBlockOwnerVtable {
    uint8_t unknown_0x00[0x70];
    PresentFinishField<FinishBlockRelease> release;
};
static_assert(offsetof(FinishBlockOwnerVtable, release) == 0x70);
struct FinishBlockOwner {
    PresentFinishField<FinishBlockOwnerVtable *> vtable;
};
static_assert(offsetof(FinishBlockOwner, vtable) == 0x00);

struct FinishCounterWord { PresentFinishField<uint32_t> value; };
struct FinishCounterSet {
    uint8_t unknown_0x00[0x18];
    PresentFinishField<FinishCounterWord *> producer;
    PresentFinishField<FinishCounterWord *> observed;
};
static_assert(offsetof(FinishCounterSet, producer) == 0x18);
static_assert(offsetof(FinishCounterSet, observed) == 0x20);
struct FinishSubmissionOwner {
    uint8_t unknown_0x0000[0xb728];
    PresentFinishField<FinishCounterSet *> counters;
};
static_assert(offsetof(FinishSubmissionOwner, counters) == 0xb728);

struct FinishCommandSnapshot {
    PresentFinishField<uintptr_t> begin;
    PresentFinishField<uintptr_t> limit;
    PresentFinishField<uintptr_t> cursor;
    PresentFinishField<uintptr_t> callback_bits;
    PresentFinishField<uintptr_t> callback_argument;
    PresentFinishField<uint64_t> word_28;
    PresentFinishField<uint64_t> word_30;
    PresentFinishField<uint64_t> word_38;
};
static_assert(offsetof(FinishCommandSnapshot, begin) == 0x00);
static_assert(offsetof(FinishCommandSnapshot, limit) == 0x08);
static_assert(offsetof(FinishCommandSnapshot, cursor) == 0x10);
static_assert(offsetof(FinishCommandSnapshot, callback_bits) == 0x18);
static_assert(offsetof(FinishCommandSnapshot, callback_argument) == 0x20);
static_assert(offsetof(FinishCommandSnapshot, word_28) == 0x28);
static_assert(offsetof(FinishCommandSnapshot, word_30) == 0x30);
static_assert(offsetof(FinishCommandSnapshot, word_38) == 0x38);
static_assert(sizeof(FinishCommandSnapshot) == 0x40);

struct CommandFinishStateView {
    FinishCommandSnapshot buffer;
    uint8_t section_40;
    uint8_t unknown_0x0041[0xb38f];
    PresentFinishField<void *> node_manager;
    PresentFinishField<FinishDrainNode *> bucket_origin;
    uint8_t unknown_0xb3e0[0x78];
    PresentFinishField<FinishDrainNode *> single_head;
    PresentFinishField<uint32_t> bucket_mask;
    uint8_t unknown_0xb464[0x2c];
    PresentFinishField<FinishSubmissionOwner *> owner;
    uint8_t unknown_0xb498[0x20];
    PresentFinishField<uintptr_t> last_reset_cursor;
    uint8_t unknown_0xb4c0[0x08];
    PresentFinishField<void *> item_processor;
    PresentFinishField<FinishBlockOwner *> block_owner;
    PresentFinishField<uintptr_t> block_head;
    PresentFinishField<uint64_t> word_b4e0;
    PresentFinishField<uint32_t> block_link_index;
    PresentFinishField<uint32_t> last_block_count;
    PresentFinishField<uint8_t> submission_flags;
};
static_assert(offsetof(CommandFinishStateView, buffer) == 0x00);
static_assert(offsetof(CommandFinishStateView, section_40) == 0x40);
static_assert(offsetof(CommandFinishStateView, node_manager) == 0xb3d0);
static_assert(offsetof(CommandFinishStateView, bucket_origin) == 0xb3d8);
static_assert(offsetof(CommandFinishStateView, single_head) == 0xb458);
static_assert(offsetof(CommandFinishStateView, bucket_mask) == 0xb460);
static_assert(offsetof(CommandFinishStateView, owner) == 0xb490);
static_assert(offsetof(CommandFinishStateView, last_reset_cursor) == 0xb4b8);
static_assert(offsetof(CommandFinishStateView, item_processor) == 0xb4c8);
static_assert(offsetof(CommandFinishStateView, block_owner) == 0xb4d0);
static_assert(offsetof(CommandFinishStateView, block_head) == 0xb4d8);
static_assert(offsetof(CommandFinishStateView, word_b4e0) == 0xb4e0);
static_assert(offsetof(CommandFinishStateView, block_link_index) == 0xb4e8);
static_assert(offsetof(CommandFinishStateView, last_block_count) == 0xb4ec);
static_assert(offsetof(CommandFinishStateView, submission_flags) == 0xb4f0);
static_assert(sizeof(CommandFinishStateView) == 0xb4f1);

// This native indexed bucket view overlaps the named fields above. A fixed array
// member would conceal those aliases; all addressing retains the native integer stride.
struct FinishBucketSlot { PresentFinishField<FinishDrainNode *> head; };
struct FinishBlockSlot { PresentFinishField<uintptr_t> value; };
static_assert(sizeof(FinishBucketSlot) == 0x08);
static_assert(sizeof(FinishBlockSlot) == 0x08);

struct alignas(8) PresentCommandFinishFrame {
    PresentFinishField<uint64_t> saved_bucket_index;
    GpuCommandState *command_argument;
    FinishCommandSnapshot snapshot;
    PresentFinishField<uint64_t> saved_guard;
};
static_assert(offsetof(PresentCommandFinishFrame, saved_bucket_index) == 0x00);
static_assert(offsetof(PresentCommandFinishFrame, command_argument) == 0x08);
static_assert(offsetof(PresentCommandFinishFrame, snapshot) == 0x10);
static_assert(offsetof(PresentCommandFinishFrame, saved_guard) == 0x50);
static_assert(sizeof(PresentCommandFinishFrame) == 0x58);

using FinishSpaceCallback = uint8_t (*)(GpuCommandState *, uint32_t, uintptr_t);
static_assert(sizeof(FinishSpaceCallback) == sizeof(uintptr_t));
RT_ORIGINAL(0x02aaa5a0, release_present_finish_node,
            void(void *manager, GpuCommandState **command_argument, FinishDrainNode *node));
RT_ORIGINAL(0x02acd3f0, prepare_present_finish_section,
            void(FinishCommandSection *, GpuCommandState *));
RT_ORIGINAL(0x02ad3c80, process_present_finish_item,
            void(void *processor, GpuCommandState *, uintptr_t item));
RT_ORIGINAL(0x02ab0b00, submit_present_finish_snapshot,
            void(GpuCommandState *, GpuCommandState *optional, FinishCommandSnapshot *, uint32_t flags));

// Relocated pointer storage is compared at runtime; the file's zero backing does not
// mean a zero/default callback. Its target body and possible side effects remain opaque.
RT_GLOBAL(0x057e43c8, present_finish_expected_callback, const uintptr_t);
}  // namespace render
