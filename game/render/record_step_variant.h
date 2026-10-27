// SPDX-License-Identifier: GPL-2.0-or-later
// The step's stable byte predicate comes from the real user-PA query. The wrapper
// prepares no arguments; this is its observed call profile, not a complete service ABI.
#pragma once

#include <cstdint>

#include "runtime/original.h"

namespace render {
// Only the native low32 result is consumed. Service state and side effects remain opaque;
// the public SDK placeholder does not supply a usable return contract for this import.
RT_ORIGINAL(0x02fc07a8, native_is_user_pa_enabled, uint32_t());

// Native SETNE normalizes AL alone; upper56 RAX retains delegated result bits. Expose
// only that stable byte. Ambient registers, flags and inherited inputs are unclaimed.
RT_ORIGINAL(0x01489c40, record_step_user_pa_enabled, uint8_t());
}  // namespace render
