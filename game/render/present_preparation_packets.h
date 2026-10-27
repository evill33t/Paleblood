// SPDX-License-Identifier: GPL-2.0-or-later
// Ordered preparation packets used by the presentation-record consumer. Byte fields
// preserve unaligned storage and overlap with descriptors or the live command cursor.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/original.h"

namespace render {
struct RecordCommandBuffer;
struct PresentRecordView;
struct RecordPreparationState;

RT_ORIGINAL(0x014738b0, emit_prepared_record_packet,
            void(RecordCommandBuffer *, uint32_t slot, const void *descriptor));
RT_ORIGINAL(0x014739b0, emit_prepared_selector_packet,
            void(RecordCommandBuffer *, const void *descriptor));

// Short getter boundaries consume a borrowed pointer and return its full DWORD value.

template <typename T> struct PreparationField {
    uint8_t bytes[sizeof(T)];
    T get() const { T value; std::memcpy(&value, bytes, sizeof(value)); return value; }
    void set(T value) { std::memcpy(bytes, &value, sizeof(value)); }
};

using PreparationSpaceCallback = uint8_t (*)(RecordCommandBuffer *, uint32_t, uintptr_t);
struct PreparationCommandPrefix {
    PreparationField<uintptr_t> begin;
    PreparationField<uintptr_t> limit;
    PreparationField<uintptr_t> cursor;
    PreparationField<PreparationSpaceCallback> space_callback;
    PreparationField<uintptr_t> callback_argument;
};
static_assert(offsetof(PreparationCommandPrefix, begin) == 0x00);
static_assert(offsetof(PreparationCommandPrefix, limit) == 0x08);
static_assert(offsetof(PreparationCommandPrefix, cursor) == 0x10);
static_assert(offsetof(PreparationCommandPrefix, space_callback) == 0x18);
static_assert(offsetof(PreparationCommandPrefix, callback_argument) == 0x20);
static_assert(sizeof(PreparationCommandPrefix) == 0x28);

inline PreparationCommandPrefix *preparation_commands(RecordCommandBuffer *commands)
{
    return reinterpret_cast<PreparationCommandPrefix *>(commands);
}

inline bool preparation_reserve_words(RecordCommandBuffer *commands, uint32_t required)
{
    constexpr uint32_t word_bytes = sizeof(uint32_t);
    auto *prefix = preparation_commands(commands);
    const uintptr_t limit = prefix->limit.get();
    const uintptr_t cursor = prefix->cursor.get();
    const uint32_t available = static_cast<uint32_t>((limit - cursor) / word_bytes);
    if (available >= required) return true;
    const uintptr_t argument = prefix->callback_argument.get();
    const auto callback = prefix->space_callback.get();
    // Successful callbacks are not followed by another capacity check.
    return callback(commands, required, argument) != 0;
}

// Alternate read views pin the exact DWORD getter and completion accesses without
// widening them to the overlapping QWORD fields copied by the packet paths.
struct PreparationRecordPredicateSource {
    PreparationField<uint32_t> word;
};
static_assert(offsetof(PreparationRecordPredicateSource, word) == 0x00);
static_assert(sizeof(PreparationRecordPredicateSource) == 0x04);

// Native3-byte entry remains unhooked. Preserve the DWORD load, including noncanonical
// predicate values; byte-backed fields permit the original unaligned read.
inline uint32_t prepared_record_payload_word(const void *descriptor)
{
    return static_cast<const PreparationRecordPredicateSource *>(descriptor)->word.get();
}

struct PreparationSelectorPredicateSource {
    uint8_t unknown_0x00[0x08];
    PreparationField<uint32_t> first;
    PreparationField<uint32_t> second;
};
static_assert(offsetof(PreparationSelectorPredicateSource, first) == 0x08);
static_assert(offsetof(PreparationSelectorPredicateSource, second) == 0x0c);
static_assert(sizeof(PreparationSelectorPredicateSource) == 0x10);

// Native4-byte entry remains unhooked. Preserve the DWORD load, including noncanonical
// predicate values; byte-backed fields permit the original unaligned read.
inline uint32_t prepared_selector_second_word(const void *descriptor)
{
    return static_cast<const PreparationSelectorPredicateSource *>(descriptor)->second.get();
}

// Native4-byte entry remains unhooked. Preserve the DWORD load, including noncanonical
// predicate values; byte-backed fields permit the original unaligned read.
inline uint32_t prepared_selector_first_word(const void *descriptor)
{
    return static_cast<const PreparationSelectorPredicateSource *>(descriptor)->first.get();
}

struct PreparationRecordCompletionSource {
    uint8_t unknown_0x00[0x18];
    PreparationField<uint32_t> word;
};
static_assert(offsetof(PreparationRecordCompletionSource, word) == 0x18);
static_assert(sizeof(PreparationRecordCompletionSource) == 0x1c);

struct PreparationRecordSource {
    PreparationField<uint64_t> first;
    PreparationField<uint64_t> second;
    PreparationField<uint64_t> third;
    PreparationField<uint64_t> fourth;
    PreparationField<uint64_t> fifth;
    PreparationField<uint32_t> tail;
};
static_assert(offsetof(PreparationRecordSource, first) == 0x00);
static_assert(offsetof(PreparationRecordSource, second) == 0x08);
static_assert(offsetof(PreparationRecordSource, third) == 0x10);
static_assert(offsetof(PreparationRecordSource, fourth) == 0x18);
static_assert(offsetof(PreparationRecordSource, fifth) == 0x20);
static_assert(offsetof(PreparationRecordSource, tail) == 0x28);
static_assert(sizeof(PreparationRecordSource) == 0x2c);

struct PreparationSelectorSource {
    PreparationField<uint64_t> first;
    PreparationField<uint64_t> second;
    PreparationField<uint64_t> third;
    PreparationField<uint64_t> fourth;
    PreparationField<uint32_t> parameter_20;
    PreparationField<uint32_t> parameter_24;
    PreparationField<uint32_t> parameter_28;
    PreparationField<uint32_t> parameter_2c;
    PreparationField<uint32_t> parameter_30;
};
static_assert(offsetof(PreparationSelectorSource, first) == 0x00);
static_assert(offsetof(PreparationSelectorSource, second) == 0x08);
static_assert(offsetof(PreparationSelectorSource, third) == 0x10);
static_assert(offsetof(PreparationSelectorSource, fourth) == 0x18);
static_assert(offsetof(PreparationSelectorSource, parameter_20) == 0x20);
static_assert(offsetof(PreparationSelectorSource, parameter_24) == 0x24);
static_assert(offsetof(PreparationSelectorSource, parameter_28) == 0x28);
static_assert(offsetof(PreparationSelectorSource, parameter_2c) == 0x2c);
static_assert(offsetof(PreparationSelectorSource, parameter_30) == 0x30);
static_assert(sizeof(PreparationSelectorSource) == 0x34);

struct PreparationWordPacket {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> index;
    PreparationField<uint32_t> value;
};
static_assert(offsetof(PreparationWordPacket, header) == 0x00);
static_assert(offsetof(PreparationWordPacket, index) == 0x04);
static_assert(offsetof(PreparationWordPacket, value) == 0x08);
static_assert(sizeof(PreparationWordPacket) == 0x0c);

struct PreparedRecordPacket {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> index;
    PreparationField<uint64_t> first;
    PreparationField<uint64_t> second;
    PreparationField<uint64_t> third;
    PreparationField<uint64_t> fourth;
    PreparationField<uint64_t> fifth;
    PreparationField<uint32_t> tail;
    PreparationField<uint32_t> completion_header;
    PreparationField<uint32_t> completion_word;
};
static_assert(offsetof(PreparedRecordPacket, header) == 0x00);
static_assert(offsetof(PreparedRecordPacket, index) == 0x04);
static_assert(offsetof(PreparedRecordPacket, first) == 0x08);
static_assert(offsetof(PreparedRecordPacket, second) == 0x10);
static_assert(offsetof(PreparedRecordPacket, third) == 0x18);
static_assert(offsetof(PreparedRecordPacket, fourth) == 0x20);
static_assert(offsetof(PreparedRecordPacket, fifth) == 0x28);
static_assert(offsetof(PreparedRecordPacket, tail) == 0x30);
static_assert(offsetof(PreparedRecordPacket, completion_header) == 0x34);
static_assert(offsetof(PreparedRecordPacket, completion_word) == 0x38);
static_assert(sizeof(PreparedRecordPacket) == 0x3c);

struct PreparedSelectorPacket {
    PreparationField<uint32_t> header;
    PreparationField<uint32_t> index;
    PreparationField<uint64_t> first;
    PreparationField<uint64_t> second;
    PreparationField<uint64_t> third;
    PreparationField<uint64_t> fourth;
    PreparationWordPacket parameter_2c;
    PreparationWordPacket parameter_20;
    PreparationWordPacket parameter_24;
    PreparationWordPacket parameter_28;
    PreparationField<uint32_t> completion_header;
    PreparationField<uint32_t> completion_word;
};
static_assert(offsetof(PreparedSelectorPacket, header) == 0x00);
static_assert(offsetof(PreparedSelectorPacket, index) == 0x04);
static_assert(offsetof(PreparedSelectorPacket, first) == 0x08);
static_assert(offsetof(PreparedSelectorPacket, second) == 0x10);
static_assert(offsetof(PreparedSelectorPacket, third) == 0x18);
static_assert(offsetof(PreparedSelectorPacket, fourth) == 0x20);
static_assert(offsetof(PreparedSelectorPacket, parameter_2c) == 0x28);
static_assert(offsetof(PreparedSelectorPacket, parameter_20) == 0x34);
static_assert(offsetof(PreparedSelectorPacket, parameter_24) == 0x40);
static_assert(offsetof(PreparedSelectorPacket, parameter_28) == 0x4c);
static_assert(offsetof(PreparedSelectorPacket, completion_header) == 0x58);
static_assert(offsetof(PreparedSelectorPacket, completion_word) == 0x5c);
static_assert(sizeof(PreparedSelectorPacket) == 0x60);

struct PreparedSelectorClearPacket {
    PreparationWordPacket first;
    PreparationWordPacket second;
};
static_assert(offsetof(PreparedSelectorClearPacket, first) == 0x00);
static_assert(offsetof(PreparedSelectorClearPacket, second) == 0x0c);
static_assert(sizeof(PreparedSelectorClearPacket) == 0x18);

constexpr uint32_t preparation_single_word_header = 0xc0016900;
constexpr uint32_t preparation_completion_header = 0xc0001000;
}  // namespace render
