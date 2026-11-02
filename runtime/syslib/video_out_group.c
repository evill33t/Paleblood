/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Check VideoOut's indexed group against its unindexed backend words.

   The complete 450-byte read-only leaf at module offset 0x6c20 in libSceVideoOut,
   SHA-256 33a6f16349f07637e168db0d6338f0b2ae8aea626b86bc3d28325ecb5155e2c1,
   consumes only the base and signed low DWORD group. State must equal exactly 1.
   Each reached group word precedes its conditional backend read; later pairs
   are not read after success. Negative and large signed groups are not rejected.

   These byte layouts are offset dictionaries, not complete guest objects or
   capacity promises. The uncompiled host port preserves machine-width address
   arithmetic and ordered ordinary accesses. Exact faults, concurrent atomicity,
   compiler load lowering and full native register/flag effects are unverified.
   No public SCE registration, device format or backend policy is inferred. */
#include "runtime/video_out_group.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut group words currently require a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");

enum {
    group_stride_bytes = 0xa8,
    pair_count = 16,
    word_bytes = sizeof(uint32_t),
    scanned_state = 1,
};

typedef struct {
    uint8_t unknown_00[0x14c];
    uint8_t state_14c[word_bytes];
    uint8_t words_150[pair_count][word_bytes];
} group_anchors;

typedef struct {
    uint8_t unknown_00[0x3e8];
    uint8_t words_3e8[pair_count][word_bytes];
} backend_anchors;

_Static_assert(offsetof(group_anchors, state_14c) == 0x14c, "indexed state");
_Static_assert(offsetof(group_anchors, words_150) == 0x150, "indexed words");
_Static_assert(sizeof(group_anchors) == 0x190, "last indexed word endpoint");
_Static_assert(offsetof(backend_anchors, words_3e8) == 0x3e8, "backend words");
_Static_assert(sizeof(backend_anchors) == 0x428, "last backend word endpoint");

static uint32_t read_word(uint64_t address)
{
    uint32_t value;
    __asm__ volatile("" ::: "memory");
    memcpy(&value, (const void *)(uintptr_t)address, sizeof value);
    __asm__ volatile("" ::: "memory");
    return value;
}

uint64_t rt_video_out_group_has_pair(const void *backend_storage, int32_t group)
{
    const uint64_t base = (uint64_t)(uintptr_t)backend_storage;
    const uint64_t stride = (uint64_t)(int64_t)group * group_stride_bytes;
    const uint64_t indexed = base + stride;
    if (read_word(indexed + offsetof(group_anchors, state_14c)) != scanned_state)
        return 0;

    for (uint64_t pair = 0; pair < pair_count; ++pair) {
        const uint64_t offset = pair * word_bytes;
        if (read_word(indexed + offsetof(group_anchors, words_150) + offset) != 0 &&
            read_word(base + offsetof(backend_anchors, words_3e8) + offset) != 0)
            return 1;
    }
    return 0;
}
