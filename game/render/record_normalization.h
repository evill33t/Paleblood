// SPDX-License-Identifier: GPL-2.0-or-later
// Native normalization prefixes. Numeric modes and unproved parameters retain neutral
// names; copied trailing words and untouched padding do not gain initialized values here.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/original.h"

namespace render {
struct RecordMode2Input {
    uint32_t component_00;
    uint32_t count_04;
    uint32_t parameter_08;
    uint32_t selector;
    uint32_t parameter_10;
    uint32_t parameter_14;
    uint32_t parameter_18;
    uint32_t flags;
    uint32_t extra_20;
};
static_assert(offsetof(RecordMode2Input, selector) == 0xc);
static_assert(offsetof(RecordMode2Input, parameter_10) == 0x10);
static_assert(offsetof(RecordMode2Input, parameter_14) == 0x14);
static_assert(offsetof(RecordMode2Input, flags) == 0x1c);
static_assert(sizeof(RecordMode2Input) == 0x24);

struct NormalizedRecordMode2 {
    RecordMode2Input input;
    uint32_t mapping;
    uint32_t supplied;
    uint32_t secondary;
    uint32_t tertiary;
    uint8_t mode;
    uint8_t unknown_0x35[0x3];
    uint32_t status;
};
static_assert(offsetof(NormalizedRecordMode2, input) == 0x0);
static_assert(offsetof(NormalizedRecordMode2, mapping) == 0x24);
static_assert(offsetof(NormalizedRecordMode2, supplied) == 0x28);
static_assert(offsetof(NormalizedRecordMode2, secondary) == 0x2c);
static_assert(offsetof(NormalizedRecordMode2, tertiary) == 0x30);
static_assert(offsetof(NormalizedRecordMode2, mode) == 0x34);
static_assert(offsetof(NormalizedRecordMode2, status) == 0x38);
static_assert(sizeof(NormalizedRecordMode2) == 0x3c);

struct RecordMode4Input {
    uint32_t component_00;
    uint32_t component_04;
    uint32_t component_08;
    uint32_t count_0c;
    uint32_t selector;
    uint32_t parameter_14;
    uint32_t parameter_18;
    uint32_t parameter_1c;
    uint32_t flags;
    uint32_t extra_24;
};
static_assert(offsetof(RecordMode4Input, selector) == 0x10);
static_assert(offsetof(RecordMode4Input, parameter_14) == 0x14);
static_assert(offsetof(RecordMode4Input, parameter_18) == 0x18);
static_assert(offsetof(RecordMode4Input, flags) == 0x20);
static_assert(sizeof(RecordMode4Input) == 0x28);
struct NormalizedRecordMode4 {
    RecordMode4Input input;
    uint32_t mapping;
    uint32_t supplied;
    uint32_t secondary;
    uint32_t tertiary;
    uint8_t mode;
    uint8_t unknown_0x39[0x3];
    uint32_t status;
};
static_assert(offsetof(NormalizedRecordMode4, input) == 0x0);
static_assert(offsetof(NormalizedRecordMode4, mapping) == 0x28);
static_assert(offsetof(NormalizedRecordMode4, supplied) == 0x2c);
static_assert(offsetof(NormalizedRecordMode4, secondary) == 0x30);
static_assert(offsetof(NormalizedRecordMode4, tertiary) == 0x34);
static_assert(offsetof(NormalizedRecordMode4, mode) == 0x38);
static_assert(offsetof(NormalizedRecordMode4, status) == 0x3c);
static_assert(sizeof(NormalizedRecordMode4) == 0x40);

RT_ORIGINAL(0x02ab7310, translate_record_selector, uint32_t(uint32_t *mapping, uint32_t selector));
RT_ORIGINAL(0x02abaf10, derive_record_parameter,
            uint32_t(uint32_t kind, uint32_t selector, uint32_t first, uint32_t second, uint32_t flags));
RT_ORIGINAL(0x0149cb30, apply_record_mapping,
            uint32_t(uint32_t *value, uint32_t derived, uint32_t mapping, uint32_t one));
RT_ORIGINAL(0x02abb4a0, report_record_normalization_error, void(uint32_t status));
RT_ORIGINAL(0x02ab7720, translate_record_secondary, uint32_t(uint32_t *value, uint32_t selector));
RT_ORIGINAL(0x02abb4f0, record_has_tertiary_parameter, uint8_t(uint32_t parameter, uint32_t selector));
RT_ORIGINAL(0x02ab78a0, translate_record_tertiary, uint32_t(uint32_t *value, uint32_t selector));

constexpr uint32_t normalization_failed = 8;
constexpr uint32_t fixed_record_selector = 0x16;
constexpr uint32_t fixed_record_mapping = 0x204704;

inline bool record_needs_secondary(uint32_t selector)
{
    // The native bounded bit test selects exactly these four numeric values.
    switch (selector) {
    case 0x14: case 0x28: case 0x2d: case 0x37: return true;
    default: return false;
    }
}

namespace normalization_detail {
template <typename T> void copy_word(void *destination, const void *source)
{
    // Keep potentially indeterminate padding as bytes rather than reading an integer value.
    uint8_t bytes[sizeof(T)];
    std::memcpy(bytes, source, sizeof(bytes));
    std::memcpy(destination, bytes, sizeof(bytes));
}
}  // namespace normalization_detail

template <typename Input> void copy_normalization_input(Input *destination, const Input *source)
{
    // Preserve the native overlap behavior: optional trailing DWORD, descending QWORDs,
    // then read both leading QWORDs before writing the second and finally the first.
    constexpr std::size_t word_bytes = sizeof(uint64_t);
    constexpr std::size_t pair_bytes = word_bytes * 2;
    constexpr std::size_t tail_bytes = sizeof(Input) % word_bytes;
    static_assert(tail_bytes == 0 || tail_bytes == sizeof(uint32_t));
    auto *out = reinterpret_cast<uint8_t *>(destination);
    const auto *in = reinterpret_cast<const uint8_t *>(source);
    if constexpr (tail_bytes != 0)
        normalization_detail::copy_word<uint32_t>(out + sizeof(Input) - tail_bytes,
                                                  in + sizeof(Input) - tail_bytes);
    for (std::size_t remaining = sizeof(Input) - tail_bytes; remaining > pair_bytes; remaining -= word_bytes)
        normalization_detail::copy_word<uint64_t>(out + remaining - word_bytes, in + remaining - word_bytes);
    uint8_t first[word_bytes], second[word_bytes];
    std::memcpy(first, in, sizeof(first));
    std::memcpy(second, in + word_bytes, sizeof(second));
    std::memcpy(out + word_bytes, second, sizeof(second));
    std::memcpy(out, first, sizeof(first));
}
}  // namespace render
