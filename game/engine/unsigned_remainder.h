// SPDX-License-Identifier: GPL-2.0-or-later
// Unsigned DIVQ remainder, including the original architecture's divide-fault operation.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace engine {
inline uint64_t remainder_u64(uint64_t dividend, uint64_t divisor)
{
#if defined(__x86_64__)
    uint64_t quotient, remainder;
    asm volatile("divq %[divisor]"
                 : "=a"(quotient), "=d"(remainder)
                 : "0"(dividend), "1"(uint64_t(0)), [divisor] "r"(divisor)
                 : "cc", "memory");
    return remainder;
#else
    if (divisor == 0) {
        // The original AMD64 fault has no reconstructed host exception bridge.
        // Stop at that boundary instead of choosing a value or claiming signal equivalence.
        std::fputs("engine::remainder_u64: AMD64 DIVQ zero-divisor fault bridge is unavailable\n",
                   stderr);
        std::abort();
    }
    return dividend % divisor;
#endif
}
}  // namespace engine
