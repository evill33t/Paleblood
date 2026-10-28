// SPDX-License-Identifier: GPL-2.0-or-later
// Complete nine-byte original-ISA source form; the native entry stays unhooked.
#pragma once

#include "engine/locked_compare_exchange.h"
#include "runtime/original.h"

namespace render {
RT_ORIGINAL(0x0247ebc0, original_present_pool_compare_exchange,
            uint64_t(void *word, uint64_t desired, uint64_t expected));
#if defined(__x86_64__)
inline uint64_t present_pool_compare_exchange_source(void *word, uint64_t desired,
                                                      uint64_t expected)
{
    return engine::locked_compare_exchange_prior(word, desired, expected);
}
#endif
// Callers still use the original binding. The source form returns the prior full QWORD;
// it does not expose the instruction flags as a C++ result or claim a portable backend.
}  // namespace render
