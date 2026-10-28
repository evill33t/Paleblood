// SPDX-License-Identifier: GPL-2.0-or-later
// Scan the captured pool geometry from the advanced counter. Preserve the native
// sticky marker status and publish counter before the borrowed eight-byte output.
#include "present_buffer_pool.h"
#include "present_pool_atomic.h"

namespace {
constexpr uint64_t pool_word_bytes = sizeof(uint64_t);
constexpr uint64_t marker_expected = 0;
constexpr uint64_t marker_claim = 1;
constexpr uint64_t marker_status_two = 2;
constexpr uint32_t pop_success = 0;
constexpr uint32_t pop_observed_two = 1;
constexpr uint32_t pop_no_acquisition = 2;
}  // namespace

extern "C" uint32_t bb_render_pop_present_buffer_pool(render::PresentBufferPool *pool,
                                                      void *output)
{
    using namespace render;
    const uint64_t base = pool->base.get();
    const uint64_t slots = pool->slots.get();
    const uint64_t block_count = pool->block_count.get();
    PresentBufferPoolWord *counter = &pool->counter;
    const uint64_t start = present_pool_calls::advance_present_pool_counter(counter) + 1u;
    uint32_t status = pop_no_acquisition;
    // The counter operation above still occurs when this captured count is zero.
    if (block_count == 0) return status;

    const uint64_t last_slot = slots - 1u;
    for (uint64_t offset = 0; offset < block_count; ++offset) {
        const uint64_t index = (start + offset) % block_count;
        const uint64_t stride = index * slots;
        const uint64_t marker_address = base + (stride + last_slot) * pool_word_bytes;
        auto *marker = reinterpret_cast<PresentBufferPoolWord *>(marker_address);
        const uint64_t observed = marker->value.get();
        if (observed == marker_expected) {
            const uint64_t previous = original_present_pool_compare_exchange(
                marker, marker_claim, marker_expected);
            if (previous != 0) continue;
            const uint64_t acquired = base + stride * pool_word_bytes;
            counter->value.set(index);
            std::memcpy(output, &acquired, sizeof(acquired));
            return pop_success;
        }
        // A failed compare/exchange does not take this status update, even when
        // its return is2. Only the original nonzero marker read is inspected here.
        if (observed == marker_status_two) status = pop_observed_two;
    }
    return status;
}
