// SPDX-License-Identifier: GPL-2.0-or-later
// Pool views for the managed presentation-capacity callback. The counter advance
// and marker compare/exchange remain opaque originals, not host-atomic substitutes.
// Pointer fields and address calculations retain their full unsigned64 bit patterns.
#pragma once

#include "present_finish.h"

namespace render {
struct PresentBufferPoolWord {
    PresentFinishField<uint64_t> value;
};
static_assert(offsetof(PresentBufferPoolWord, value) == 0x00);
static_assert(sizeof(PresentBufferPoolWord) == 0x08);

struct PresentBufferPool {
    uint8_t unknown_0x00[0x08];
    PresentFinishField<uint64_t> slots;
    PresentFinishField<uint64_t> block_count;
    PresentFinishField<uint64_t> base;
    PresentBufferPoolWord counter;
};
static_assert(offsetof(PresentBufferPool, slots) == 0x08);
static_assert(offsetof(PresentBufferPool, block_count) == 0x10);
static_assert(offsetof(PresentBufferPool, base) == 0x18);
static_assert(offsetof(PresentBufferPool, counter) == 0x20);
static_assert(sizeof(PresentBufferPool) == 0x28);
static_assert(sizeof(uintptr_t) == sizeof(uint64_t));

// Output is exactly eight borrowed bytes, including unaligned or overlapping slots.
// Success publishes the counter before output; failure performs no own output store.
RT_ORIGINAL(0x02ad3b80, pop_present_buffer_pool, uint32_t(PresentBufferPool *, void *output));
// Begin0 returns before pool access. Otherwise SAR3 precedes unsigned DIVQ; no
// zero-divisor fallback or native exception-delivery equivalence is established.
RT_ORIGINAL(0x02ad3d40, capture_present_buffer_pool_record,
            uint64_t(const PresentBufferPool *, uint64_t begin));

namespace present_pool_calls {
// Only these explicit input/return profiles are established at the pool callers.
// Extra inherited GP/SIMD inputs, lower memory effects and native frame aliases
// remain unclosed. Advance uses its original address; the shared CAS binding is
// declared separately in present_pool_atomic.h.
RT_ORIGINAL(0x0247eb80, advance_present_pool_counter, uint64_t(PresentBufferPoolWord *));
}  // namespace present_pool_calls
}  // namespace render
