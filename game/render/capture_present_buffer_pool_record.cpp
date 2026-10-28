// SPDX-License-Identifier: GPL-2.0-or-later
// Locate the containing pool record using SAR3 followed by unsigned64 remainder.
// The wrapped negative-looking difference is not subjected to signed division.
#include <limits>

#include "present_buffer_pool.h"
#include "../engine/unsigned_remainder.h"

namespace {
constexpr uint64_t pool_word_bytes = sizeof(uint64_t);
constexpr uint32_t pool_word_shift = 3;
constexpr uint32_t address_word_bits = std::numeric_limits<uint64_t>::digits;

uint64_t arithmetic_pool_word_shift(uint64_t difference)
{
    const uint64_t sign_fill = uint64_t{0} - (difference >> (address_word_bits - 1u));
    return (difference >> pool_word_shift) |
           (sign_fill << (address_word_bits - pool_word_shift));
}
}  // namespace

extern "C" uint64_t bb_render_capture_present_buffer_pool_record(
    const render::PresentBufferPool *pool, uint64_t begin)
{
    if (begin == 0) return 0;
    const uint64_t base = pool->base.get();
    const uint64_t difference = begin - base;
    const uint64_t shifted = arithmetic_pool_word_shift(difference);
    const uint64_t slots = pool->slots.get();
    // The shared helper retains AMD64 DIVQ. Zero-divisor exception delivery is
    // unclosed; no fallback record is invented, and no host-fault equivalence is claimed.
    const uint64_t remainder = engine::remainder_u64(shifted, slots);
    const uint64_t whole = shifted - remainder;
    return base + whole * pool_word_bytes;
}
