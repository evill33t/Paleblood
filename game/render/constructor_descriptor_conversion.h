// SPDX-License-Identifier: GPL-2.0-or-later
// Convert an opaque constructor record into a packed32-byte descriptor. Names
// describe destination words and bit groups; no GPU/format meaning is assumed.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/original.h"

namespace render {
template <typename T> struct ConstructorConversionField {
    uint8_t bytes[sizeof(T)];
    T get() const { T value; std::memcpy(&value, bytes, sizeof(value)); return value; }
    void set(T value) { std::memcpy(bytes, &value, sizeof(value)); }
};
struct ConstructorRecordSource;
struct ConstructorDescriptorWordPair {
    ConstructorConversionField<uint32_t> low;
    ConstructorConversionField<uint32_t> high;
    void clear()
    {
        const uint64_t zero = 0;
        std::memcpy(this, &zero, sizeof(zero));
    }
};
// Byte-backed pairs permit the observed unaligned QWORD and DWORD accesses.
struct ConstructorDescriptorOutput {
    ConstructorDescriptorWordPair words_00_04;
    ConstructorDescriptorWordPair words_08_0c;
    ConstructorDescriptorWordPair words_10_14;
    ConstructorDescriptorWordPair words_18_1c;
};
// Relative base is native RSP=RBP-0x40. This retains the known local adjacency,
// not a compiler-generated physical frame. Unknown bytes remain uninitialized.
struct alignas(8) ConstructorConversionFrame {
    uint8_t unknown_00[4];
    ConstructorConversionField<uint32_t> saved_flag;
    ConstructorConversionField<uint32_t> mapping_word;
    uint8_t unknown_0c[4];
    ConstructorConversionField<uint64_t> saved_guard;
};
static_assert(sizeof(ConstructorDescriptorWordPair) == 8);
static_assert(alignof(ConstructorDescriptorWordPair) == 1);
static_assert(offsetof(ConstructorDescriptorWordPair, high) == 4);
static_assert(offsetof(ConstructorDescriptorOutput, words_08_0c) == 8);
static_assert(offsetof(ConstructorDescriptorOutput, words_10_14) == 0x10);
static_assert(offsetof(ConstructorDescriptorOutput, words_18_1c) == 0x18);
static_assert(sizeof(ConstructorDescriptorOutput) == 0x20);
static_assert(alignof(ConstructorDescriptorOutput) == 1);
static_assert(offsetof(ConstructorConversionFrame, saved_flag) == 4);
static_assert(offsetof(ConstructorConversionFrame, mapping_word) == 8);
static_assert(offsetof(ConstructorConversionFrame, saved_guard) == 0x10);
static_assert(sizeof(ConstructorConversionFrame) == 0x18);

namespace constructor_conversion_calls {
// Constructed call-site profiles only: this body does not establish an input
// extent or formal extra-argument/ambient-register contract for opaque getters.
RT_ORIGINAL(0x01486ca0, source_for_word00, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x01486d70, source_for_word08_low, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x014868d0, source_for_word08_high, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x01486cb0, source_for_word10_high, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x01486cf0, source_mapping_word, uint32_t(const ConstructorRecordSource *));
// Existing73-byte original: read one DWORD and select component0..3. Keep the
// original call and shared local address; no lower implementation is copied.
RT_ORIGINAL(0x01471a40, read_record_descriptor_component, uint32_t(const uint32_t *, uint32_t));
RT_ORIGINAL(0x014868e0, source_for_word0c_middle, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x01486ce0, source_for_word10_low, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x01486cd0, source_for_word14_low, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x014868f0, source_signed_parameter, uint32_t(const ConstructorRecordSource *));
RT_ORIGINAL(0x02fbe4f8, fail_guard, void());
RT_GLOBAL(0x057e3be0, guard_pointer, const uint64_t *);
}  // namespace constructor_conversion_calls
}  // namespace render

// Normal return is the full current guard QWORD, not a numeric status. Direct
// inputs are three GP values; extra ambient/lower arity, complete physical frame
// and a returning guard-failure continuation are explicitly unclaimed.
extern "C" uint64_t bb_render_convert_constructor_record(
    render::ConstructorDescriptorOutput *, const render::ConstructorRecordSource *, uint32_t);
