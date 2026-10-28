// SPDX-License-Identifier: GPL-2.0-or-later
// Swap-chain setup's observed storage and call-site profiles. VideoOut formats,
// lower construction semantics and manager implementations remain original.
#pragma once

#include "present_finish.h"

namespace render {
struct SetupObject;
struct SetupServices;
struct SetupResourceSlot;
struct SetupOwner;
using SetupAllocate = uintptr_t (*)(SetupOwner *, uint64_t);
// Allocation+58 also has provable inherited RDX/RCX values at this call site.
// The fourth value's semantic consumption remains unproved.
using SetupAllocate58 = uintptr_t (*)(SetupOwner *, uint64_t, uint64_t, uint64_t);
using SetupRelease = void (*)(SetupOwner *, uintptr_t);
struct SetupOwnerVtable {
    uint8_t unknown_0x00[0x50];
    PresentFinishField<SetupAllocate> allocate_50;
    PresentFinishField<SetupAllocate58> allocate_58;
    uint8_t unknown_0x60[0x10];
    PresentFinishField<SetupRelease> release_70;
};
struct SetupOwner { PresentFinishField<SetupOwnerVtable *> vtable; };
static_assert(offsetof(SetupOwnerVtable, allocate_50) == 0x50);
static_assert(offsetof(SetupOwnerVtable, allocate_58) == 0x58);
static_assert(offsetof(SetupOwnerVtable, release_70) == 0x70);
struct SetupOwnerSlot { PresentFinishField<SetupOwner *> value; };
struct SetupObjectSlot { PresentFinishField<SetupObject *> value; };
static_assert(sizeof(SetupOwnerSlot) == 8);

struct SetupRecord {
    PresentFinishField<uint64_t> words[5];
    PresentFinishField<uint32_t> tail_28;
};
static_assert(offsetof(SetupRecord, tail_28) == 0x28);
static_assert(sizeof(SetupRecord) == 0x2c);

struct SetupInfo {
    PresentFinishField<uint32_t> first;
    PresentFinishField<uint32_t> second;
    uint8_t unknown_0x08[8];
    PresentFinishField<uint32_t> selector_10;
    uint8_t unknown_0x14[8];
    PresentFinishField<uint32_t> parameter_1c;
    PresentFinishField<uint32_t> parameter_20;
    PresentFinishField<uint32_t> flags_24;
    PresentFinishField<uint32_t> buffer_count;
    uint8_t unknown_0x2c[0x10];
    PresentFinishField<uint32_t> override_3c;
    PresentFinishField<uint32_t> parameter_40;
    uint8_t unknown_0x44[4];
};
static_assert(offsetof(SetupInfo, selector_10) == 0x10);
static_assert(offsetof(SetupInfo, parameter_1c) == 0x1c);
static_assert(offsetof(SetupInfo, parameter_20) == 0x20);
static_assert(offsetof(SetupInfo, flags_24) == 0x24);
static_assert(offsetof(SetupInfo, buffer_count) == 0x28);
static_assert(offsetof(SetupInfo, override_3c) == 0x3c);
static_assert(offsetof(SetupInfo, parameter_40) == 0x40);
static_assert(sizeof(SetupInfo) == 0x48);

struct SetupCachedRecord {
    PresentFinishField<uint64_t> zero_00;
    PresentFinishField<uintptr_t> storage_08;
    PresentFinishField<uint32_t> mode_10;
    PresentFinishField<uint32_t> quantity_14;
    uint8_t unknown_0x18[8];
    PresentFinishField<uint8_t> bytes_20[4];
    PresentFinishField<uint32_t> zero_24;
    PresentFinishField<uint32_t> unit_28;
    PresentFinishField<uint16_t> first_2c;
    PresentFinishField<uint16_t> second_2e;
    PresentFinishField<uint16_t> one_30;
    PresentFinishField<uint8_t> one_32;
    PresentFinishField<uint8_t> selector_33;
    PresentFinishField<uint16_t> parameter_34;
    PresentFinishField<uint16_t> parameter_36;
};
static_assert(offsetof(SetupCachedRecord, storage_08) == 8);
static_assert(offsetof(SetupCachedRecord, mode_10) == 0x10);
static_assert(offsetof(SetupCachedRecord, quantity_14) == 0x14);
static_assert(offsetof(SetupCachedRecord, bytes_20) == 0x20);
static_assert(offsetof(SetupCachedRecord, zero_24) == 0x24);
static_assert(offsetof(SetupCachedRecord, unit_28) == 0x28);
static_assert(offsetof(SetupCachedRecord, first_2c) == 0x2c);
static_assert(offsetof(SetupCachedRecord, parameter_36) == 0x36);
static_assert(sizeof(SetupCachedRecord) == 0x38);

constexpr std::size_t setup_curve_entries = 0x401;
struct SetupCurveOutput {
    uint8_t unknown_0x00[0x38];
    PresentFinishField<float> triplets[setup_curve_entries][3];
};
static_assert(offsetof(SetupCurveOutput, triplets) == 0x38);
struct SetupState {
    uint8_t record_storage[0x2c0];
    PresentFinishField<SetupOwner *> manager_2c0;
    PresentFinishField<SetupOwner *> allocator_2c8;
    PresentFinishField<uintptr_t> storage_2d0;
    PresentFinishField<uint64_t> service_2d8;
    PresentFinishField<uint64_t> zero_2e0;
    PresentFinishField<uintptr_t> last_2e8;
    PresentFinishField<int32_t> video_handle;
    uint8_t unknown_0x2f4[4];
    PresentFinishField<uint64_t> event_queue;
    PresentFinishField<uint64_t> info_copy[9];
    SetupCachedRecord cached_record;
    SetupCurveOutput curve;
};
static_assert(offsetof(SetupState, manager_2c0) == 0x2c0);
static_assert(offsetof(SetupState, allocator_2c8) == 0x2c8);
static_assert(offsetof(SetupState, storage_2d0) == 0x2d0);
static_assert(offsetof(SetupState, service_2d8) == 0x2d8);
static_assert(offsetof(SetupState, zero_2e0) == 0x2e0);
static_assert(offsetof(SetupState, last_2e8) == 0x2e8);
static_assert(offsetof(SetupState, video_handle) == 0x2f0);
static_assert(offsetof(SetupState, event_queue) == 0x2f8);
static_assert(offsetof(SetupState, info_copy) == 0x300);
static_assert(offsetof(SetupState, cached_record) == 0x348);
static_assert(offsetof(SetupState, curve) == 0x380);

// These spans retain relative borrowed storage, not inferred callee write sizes.
struct SetupVideoAttribute { uint8_t bytes[0x2c]; };
struct SetupParameters { PresentFinishField<uint32_t> words[12]; };
struct SetupChannel {
    PresentFinishField<uintptr_t> data;
    PresentFinishField<uint32_t> bytes;
    PresentFinishField<uint32_t> zero;
};
static_assert(sizeof(SetupParameters) == 0x30);
static_assert(sizeof(SetupChannel) == 0x10);
struct SetupTemporaryAllocator {
    PresentFinishField<const void *> vtable;
    PresentFinishField<uintptr_t> token;
};
struct SetupTemporaryVtable {
    uint8_t unknown_0x00[0x10];
    uint8_t address_point;
};
static_assert(offsetof(SetupTemporaryVtable, address_point) == 0x10);
struct SetupAuxiliarySettings { uint8_t bytes[0x20]; };
struct SetupQueueName { char bytes[0xf]; };

namespace swap_setup_calls {
// Explicit call-site inputs/return use only; unknown ambient inputs and lower
// effects remain unclaimed. None of these lower bodies is reconstructed here.
// Six known incoming GP wires, not a claim of formal lower-function arity.
RT_ORIGINAL(0x0247c4e0, create_object,
            SetupObject *(uint32_t, uint32_t, SetupOwner *, SetupServices *, SetupInfo *, SetupOwner *));
RT_ORIGINAL(0x02adbcb0, initialize_object, void(SetupObject *, void *));
RT_ORIGINAL(0x02adbcc0, destroy_object, void(SetupObject *));
RT_ORIGINAL(0x02ab7310, map_selector, uint32_t(PresentFinishField<uint32_t> *, uint32_t));
RT_ORIGINAL(0x0149cb30, resolve_unit,
            uint32_t(PresentFinishField<uint32_t> *, uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x02abb4a0, note_unit_failure, void(uint32_t));
RT_ORIGINAL(0x02ab7a20, first_parameter, uint32_t(uint32_t, uint32_t));
RT_ORIGINAL(0x02ab7ab0, second_parameter, uint32_t(uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x01485f90, construct_record,
            uint64_t(SetupRecord *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                     uint32_t, uint32_t, uint64_t, uint64_t));
RT_ORIGINAL(0x02abadd0, storage_admitted, uint8_t(uintptr_t));
RT_ORIGINAL(0x02fbe688, fill_storage, void *(uintptr_t, uint32_t, uint64_t));
RT_ORIGINAL(0x02abaeb0, encode_address, uint32_t(uintptr_t));
RT_ORIGINAL(0x01486c50, set_record_address, void(SetupRecord *, uintptr_t));
RT_ORIGINAL(0x01486ca0, get_record_address, uint32_t(SetupRecord *));
RT_ORIGINAL(0x025676b0, storage_service, uint64_t(SetupServices *));
RT_ORIGINAL(0x02571c60, clear_curve, void(SetupCurveOutput *));
RT_ORIGINAL(0x02572290, clear_cached_record, void(SetupCachedRecord *));
RT_ORIGINAL(0x02fbf568, video_open, int32_t(uint32_t, uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x02fbf578, video_flip_rate, int32_t(int32_t, uint32_t));
RT_ORIGINAL(0x02fbf588, create_event_queue,
            int32_t(PresentFinishField<uint64_t> *, const char *));
RT_ORIGINAL(0x02fbf598, add_flip_event, int32_t(uint64_t, int32_t, uintptr_t));
RT_ORIGINAL(0x02fbf5a8, set_video_attribute,
            void(SetupVideoAttribute *, uint32_t, uint32_t, uint32_t,
                 uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x02fbf5b8, delete_event_queue, void(uint64_t));
RT_ORIGINAL(0x02fbf5c8, video_close, void(int32_t));
RT_ORIGINAL(0x02fbf5d8, register_video_buffers,
            int32_t(int32_t, uint32_t, const void *, uint32_t, SetupVideoAttribute *));
// RSI is a borrowed output-QWORD slot; RCX is a full optional array pointer.
// This setup call supplies nullptr for the optional array.
RT_ORIGINAL(0x02565f90, initialize_cached_record,
            uint32_t(SetupServices *, SetupResourceSlot *, SetupParameters *, void *,
                     SetupOwner *, SetupOwner *, uint64_t));
RT_ORIGINAL(0x02abaef0, current_storage_mode, uint32_t());
RT_ORIGINAL(0x0247d5b0, capture_upload_token, uintptr_t());
RT_ORIGINAL(0x0247d4b0, allocate_upload, uintptr_t(SetupTemporaryAllocator *, uint64_t));
RT_ORIGINAL(0x02567890, curve_service, uintptr_t(SetupServices *));
RT_ORIGINAL(0x02571790, initialize_curve,
            uint32_t(SetupCurveOutput *, uintptr_t, const SetupAuxiliarySettings *,
                     SetupChannel *, SetupOwner *, SetupOwner *, uint64_t));
RT_ORIGINAL(0x0247d5f0, release_upload_token, void(uintptr_t));
RT_GLOBAL(0x04c707dc, queue_name, SetupQueueName);
RT_GLOBAL(0x04c707d8, curve_multiplier, float);
RT_GLOBAL(0x04c707ec, curve_settings, SetupAuxiliarySettings);
RT_GLOBAL(0x056b49e0, temporary_vtable, SetupTemporaryVtable);
}  // namespace swap_setup_calls
}  // namespace render
