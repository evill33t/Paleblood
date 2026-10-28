// SPDX-License-Identifier: GPL-2.0-or-later
// Original-ISA source form of the locked QWORD exchange. It is not installed as a hook.
#pragma once

#include <cstdint>

namespace engine {
#if defined(__x86_64__)
// A packed alias keeps the instruction's memory operand free of an invented alignment
// requirement. The actual LOCK CMPXCHG handles access, fault and failed-write behavior.
struct __attribute__((packed, may_alias)) LockedExchangeWord64 { uint64_t value; };
inline uint64_t locked_compare_exchange_prior(void *address, uint64_t desired, uint64_t expected)
{
    auto *word = reinterpret_cast<volatile LockedExchangeWord64 *>(address);
    asm volatile("lock cmpxchgq %[desired], %[word]"
                 : "+a"(expected), [word] "+m"(word->value)
                 : [desired] "r"(desired)
                 : "cc", "memory");
    return expected;
}
#endif
// Other architectures retain the original binding. A portable atomic/fault bridge is
// not established here; this header supplies only the original AMD64 instruction form.
}  // namespace engine
