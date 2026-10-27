// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace render {
struct RecordConfigurationRegister;

// A byte-backed view of only the native DWORD access. The enclosing register's
// trailing storage is neither read nor initialized here.
struct RecordConfigurationWordView {
    uint8_t bytes[sizeof(uint32_t)];

    uint32_t get() const
    {
        uint32_t value;
        std::memcpy(&value, bytes, sizeof(value));
        return value;
    }

    void set(uint32_t value) { std::memcpy(bytes, &value, sizeof(value)); }
};
static_assert(offsetof(RecordConfigurationWordView, bytes) == 0x00);
static_assert(sizeof(RecordConfigurationWordView) == 0x04);
static_assert(alignof(RecordConfigurationWordView) == 1);

inline RecordConfigurationWordView *record_configuration_word(RecordConfigurationRegister *state)
{
    return reinterpret_cast<RecordConfigurationWordView *>(state);
}

// The native seven-byte entry remains unhooked. This logical-void inline profile
// writes exactly four bytes; incoming RAX and flags are not reconstructed.
inline void initialize_record_configuration_register(RecordConfigurationRegister *state)
{
    constexpr uint32_t initial_word = 0x0000043f;
    record_configuration_word(state)->set(initial_word);
}
}  // namespace render
