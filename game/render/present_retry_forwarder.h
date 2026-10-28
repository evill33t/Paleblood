// SPDX-License-Identifier: GPL-2.0-or-later
// Complete five-byte jump to the resolved scePthreadYield import. The native entry
// stays unhooked. The observed retry caller ignores the result; the service return
// contract and other callers remain unresolved, so no host yield or value is invented.
#pragma once

#include "runtime/original.h"

namespace render {
RT_ORIGINAL(0x0247f970, original_present_retry_forwarder, void());
namespace present_retry_calls {
RT_ORIGINAL(0x02fc07c8, pthread_yield_import, void());
}
inline void retry_present_capacity_source()
{
    present_retry_calls::pthread_yield_import();
}
}  // namespace render
