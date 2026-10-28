// SPDX-License-Identifier: GPL-2.0-or-later
// Convert a constructor record into a packed32-byte descriptor. Known source
// reads retain their widths; no GPU/format meaning or whole-object size is assumed.
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
// Only the prefix read by the inline getters is described here. The remaining
// original mapping helper can require a larger backing object.
struct ConstructorRecordSource {
    ConstructorConversionField<uint32_t> word00;
    ConstructorConversionField<uint32_t> word04;
    uint8_t unknown_0x08[4];
    ConstructorConversionField<uint32_t> word0c;
    uint8_t unknown_0x10[4];
    ConstructorConversionField<uint32_t> word14;
    ConstructorConversionField<uint16_t> first_18;
    ConstructorConversionField<uint16_t> second_1a;
};
static_assert(offsetof(ConstructorRecordSource, word00) == 0);
static_assert(offsetof(ConstructorRecordSource, word04) == 4);
static_assert(offsetof(ConstructorRecordSource, word0c) == 0x0c);
static_assert(offsetof(ConstructorRecordSource, word14) == 0x14);
static_assert(offsetof(ConstructorRecordSource, first_18) == 0x18);
static_assert(offsetof(ConstructorRecordSource, second_1a) == 0x1a);
static_assert(sizeof(ConstructorRecordSource) == 0x1c);
static_assert(alignof(ConstructorRecordSource) == 1);
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
// These short native entries remain unhooked. Each inline form performs only
// its observed read; repeated calls in the consumer must remain separate.
inline uint32_t source_for_word00(const ConstructorRecordSource *source)
{
    return source->word00.get();
}
inline uint32_t source_for_word08_low(const ConstructorRecordSource *source)
{
    return source->first_18.get();
}
inline uint32_t source_for_word08_high(const ConstructorRecordSource *source)
{
    return source->second_1a.get();
}
inline uint32_t source_for_word10_high(const ConstructorRecordSource *source)
{
    constexpr uint32_t field_mask = 0x7ff;
    return source->word04.get() & field_mask;
}
inline uint32_t source_for_word0c_middle(const ConstructorRecordSource *source)
{
    constexpr uint32_t field_mask = 0x1f;
    return source->word14.get() & field_mask;
}
inline uint32_t source_for_word10_low(const ConstructorRecordSource *source)
{
    constexpr uint32_t field_shift = 13;
    constexpr uint32_t field_mask = 0x7ff;
    return (source->word0c.get() >> field_shift) & field_mask;
}
inline uint32_t source_for_word14_low(const ConstructorRecordSource *source)
{
    constexpr uint32_t field_mask = 0x7ff;
    return source->word0c.get() & field_mask;
}
inline uint32_t source_signed_parameter(const ConstructorRecordSource *source)
{
    constexpr uint32_t field_shift = 15;
    constexpr uint32_t field_mask = 3;
    return (source->word14.get() >> field_shift) & field_mask;
}
// This remaining mapping getter tail-calls an original encoder. Its full
// callee-input and return contract remains unresolved; retain the observed
// low32 consumer profile and original call instead of inventing a leaf body.
RT_ORIGINAL(0x01486cf0, source_mapping_word, uint32_t(const ConstructorRecordSource *));
// Existing73-byte original: read one DWORD and select component0..3. Keep the
// original call and shared local address; no lower implementation is copied.
RT_ORIGINAL(0x01471a40, read_record_descriptor_component, uint32_t(const uint32_t *, uint32_t));
RT_ORIGINAL(0x02fbe4f8, fail_guard, void());
RT_GLOBAL(0x057e3be0, guard_pointer, const uint64_t *);
}  // namespace constructor_conversion_calls
}  // namespace render

// Normal return is the full current guard QWORD, not a numeric status. Direct
// inputs are three GP values; extra ambient/lower arity, complete physical frame
// and a returning guard-failure continuation are explicitly unclaimed.
extern "C" uint64_t bb_render_convert_constructor_record(
    render::ConstructorDescriptorOutput *, const render::ConstructorRecordSource *, uint32_t);
