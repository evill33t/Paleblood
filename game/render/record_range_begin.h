// SPDX-License-Identifier: GPL-2.0-or-later
// Source form of the seven-byte packed range helper. Shift the full input before
// taking its DWORD result; the native entry stays unhooked.
#pragma once

#include <cstdint>

namespace render {
inline uint32_t record_range_begin(uintptr_t packed)
{
    constexpr uint32_t field_shift = 8;
    return static_cast<uint32_t>(packed >> field_shift);
}
}  // namespace render
