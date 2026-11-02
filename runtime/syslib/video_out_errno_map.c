/* SPDX-License-Identifier: GPL-2.0-or-later */
/* This errno mapper has its own live dispatch table. In particular its direct
   return landing preserves the input rather than assigning a constant zero. */
#include "runtime/video_out_errno_map.h"
#include "runtime/process.h"

#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut errno dispatch requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(int32_t) == 4, "relative table entry width");

const rt_video_out_errno_map_bindings *volatile rt_video_out_errno_map_imports;

/* These are signed relative destinations in the observed code/table layout.
   Preserve the live read; they name actions, not a frozen errno lookup array. */
enum dispatch_action {
    dispatch_error_0001 = -62623,
    dispatch_error_00ff = -62617,
    dispatch_error_100c = -62611,
    dispatch_error_0002 = -62605,
    dispatch_error_001c = -62599,
    dispatch_error_0009 = -62593,
    dispatch_return_after_0001 = -62618,
    dispatch_return_after_00ff = -62612,
    dispatch_return_after_100c = -62606,
    dispatch_return_after_0002 = -62600,
    dispatch_return_after_001c = -62594,
    dispatch_return_after_0009 = -62588
};

static const uint32_t error_0001 = UINT32_C(0x80290001);
static const uint32_t error_00ff = UINT32_C(0x802900ff);
static const uint32_t error_100c = UINT32_C(0x8029100c);
static const uint32_t error_0002 = UINT32_C(0x80290002);
static const uint32_t error_001c = UINT32_C(0x8029001c);
static const uint32_t error_0009 = UINT32_C(0x80290009);

static int32_t read_dispatch(const void *table, uint32_t index)
{
    const uint64_t address = (uint64_t)(uintptr_t)table +
        (uint64_t)index * sizeof(int32_t);
    int32_t displacement;
    __asm__ volatile("" ::: "memory");
    memcpy(&displacement, (const void *)(uintptr_t)address, sizeof displacement);
    __asm__ volatile("" : : "r"((uint32_t)displacement) : "memory");
    return displacement;
}

uint64_t rt_video_out_map_errno(uint64_t error_bits)
{
    const uint32_t error = (uint32_t)error_bits;
    if (error >= RT_VIDEO_OUT_ERRNO_TABLE_ENTRIES)
        return (uint64_t)error_00ff;

    const rt_video_out_errno_map_bindings *imports = rt_video_out_errno_map_imports;
    if (!imports || !imports->relative_table)
        rt_fatal("VideoOut errno dispatch table", "the actual live module table is unbound");

    switch (read_dispatch(imports->relative_table, error)) {
    case dispatch_error_0001: return (uint64_t)error_0001;
    case dispatch_error_00ff: return (uint64_t)error_00ff;
    case dispatch_error_100c: return (uint64_t)error_100c;
    case dispatch_error_0002: return (uint64_t)error_0002;
    case dispatch_error_001c: return (uint64_t)error_001c;
    case dispatch_error_0009: return (uint64_t)error_0009;
    case dispatch_return_after_0001:
    case dispatch_return_after_00ff:
    case dispatch_return_after_100c:
    case dispatch_return_after_0002:
    case dispatch_return_after_001c:
    case dispatch_return_after_0009:
        return (uint64_t)error;
    default:
        rt_fatal("VideoOut errno dispatch target", "the live table selects an unrecovered instruction target");
    }
}
