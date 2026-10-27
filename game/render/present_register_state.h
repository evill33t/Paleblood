// SPDX-License-Identifier: GPL-2.0-or-later
// The four-byte word used by presentation-register initializers and field setters.
// The caller's larger record and its trailing bytes are not part of this view.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/original.h"

namespace render {
struct RecordRegisterState;

struct RecordRegisterWordView {
    uint8_t bytes[sizeof(uint32_t)];

    uint32_t get() const
    {
        uint32_t value;
        std::memcpy(&value, bytes, sizeof(value));
        return value;
    }
    void set(uint32_t value) { std::memcpy(bytes, &value, sizeof(value)); }
};
static_assert(offsetof(RecordRegisterWordView, bytes) == 0);
static_assert(sizeof(RecordRegisterWordView) == sizeof(uint32_t));
static_assert(alignof(RecordRegisterWordView) == 1);

inline RecordRegisterWordView *record_register_word(RecordRegisterState *state)
{
    return reinterpret_cast<RecordRegisterWordView *>(state);
}

constexpr uint32_t record_register_range_preserve = 0xffffff8b;
constexpr uint32_t record_register_enabled_preserve = 0xfffffffd;

// Each setter returns the preserved OLD word after publishing the new word. Enabled
// uses the entire low input byte, not a bool. No pointer/value admission is added;
// valid ordinary backing is required, and ambient flags/fault granularity are unclaimed.
RT_ORIGINAL(0x01485390, set_record_register_range,
            uint32_t(RecordRegisterState *, uint32_t first, uint32_t last));
RT_ORIGINAL(0x014853f0, set_record_register_enabled,
            uint32_t(RecordRegisterState *, uint32_t enabled));
}  // namespace render
