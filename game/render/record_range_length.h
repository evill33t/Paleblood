// SPDX-License-Identifier: GPL-2.0-or-later
// The extent helper has the same full-width shift and DWORD projection as the
// range helper. Its separate seven-byte native entry remains unhooked.
#pragma once

#include <cstdint>

namespace render {
inline uint32_t record_range_length(uintptr_t packed)
{
    constexpr uint32_t field_shift = 8;
    return static_cast<uint32_t>(packed >> field_shift);
}
}  // namespace render
