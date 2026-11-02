/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VideoOut's private process-query helper preserves the exposed scratch frame
   and treats every nonzero query result as the same all-ones sentinel. This
   source-only reconstruction requires real module/import/frame bindings. */
#include "runtime/video_out_process_flag.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut process flag requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_process_flag_query_fn) == sizeof(uint64_t), "query pointer");
_Static_assert(sizeof(rt_video_out_process_flag_stack_fail_fn) == sizeof(uint64_t), "failure pointer");

const rt_video_out_process_flag_bindings *volatile rt_video_out_process_flag_imports;

enum { zero_chunk_bytes = 32, zero_chunk_count = 4, process_flag_shift = 62 };

typedef struct {
    uint8_t unknown_00[8];
    uint8_t flags_08[sizeof(uint64_t)];
    uint8_t unknown_10[0x78];
} query_anchors;
_Static_assert(offsetof(query_anchors, flags_08) == 0x08, "queried flag QWORD");
_Static_assert(sizeof(query_anchors) == 0x88, "known zeroed extent, not provider structure size");

typedef struct {
    query_anchors query_00;
    uint8_t unknown_88[0x18];
    uint8_t guard_a0[sizeof(uint64_t)];
    uint8_t unknown_a8[0x18];
} frame_anchors;
_Static_assert(offsetof(frame_anchors, query_00) == 0x00, "query address");
_Static_assert(offsetof(frame_anchors, guard_a0) == 0xa0, "saved guard");
_Static_assert(sizeof(frame_anchors) == RT_VIDEO_OUT_PROCESS_FLAG_FRAME_BYTES, "frame extent");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static uint64_t read_quad(uint64_t address)
{
    uint64_t value;
    access_boundary();
    memcpy(&value, (const void *)(uintptr_t)address, sizeof value);
    __asm__ volatile("" : : "r"(value) : "memory");
    return value;
}

static void write_quad(uint64_t address, uint64_t value)
{
    access_boundary();
    memcpy((void *)(uintptr_t)address, &value, sizeof value);
    access_boundary();
}

static void zero_chunk(uint64_t address)
{
    static const uint8_t zeroes[zero_chunk_bytes] = {0};
    access_boundary();
    memcpy((void *)(uintptr_t)address, zeroes, sizeof zeroes);
    access_boundary();
}

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage)
        rt_fatal(name, "the required module storage or import cell is unbound");
    return storage;
}

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_process_flag_bindings *imports) \
    { \
        type call; \
        const void *cell = require_storage(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_query, rt_video_out_process_flag_query_fn, process_query_slot, "VideoOut process flag query")
IMPORT_READER(load_stack_fail, rt_video_out_process_flag_stack_fail_fn, stack_fail_slot, "VideoOut process flag __stack_chk_fail")
#undef IMPORT_READER

uint64_t rt_video_out_query_process_flag_with_frame(uint64_t process_bits,
    void *frame_storage)
{
    const rt_video_out_process_flag_bindings *imports = rt_video_out_process_flag_imports;
    if (!imports)
        rt_fatal("VideoOut process flag", "real module data and import cells have not been bound");
    const uint64_t frame = (uint64_t)(uintptr_t)require_storage(frame_storage, "VideoOut process flag borrowed frame");
    const void *guard_cell = require_storage(imports->guard_pointer_slot, "VideoOut process flag guard pointer cell");
    const uint64_t entry_guard_pointer = read_quad((uint64_t)(uintptr_t)guard_cell);
    write_quad(frame + offsetof(frame_anchors, guard_a0), read_quad(entry_guard_pointer));
    for (uint32_t chunk = zero_chunk_count; chunk != 0; --chunk)
        zero_chunk(frame + (uint64_t)(chunk - 1) * zero_chunk_bytes);
    write_quad(frame + zero_chunk_count * zero_chunk_bytes, 0);
    const uint32_t query_result = load_query(imports)(process_bits, (void *)(uintptr_t)frame);

    /* This read is reached even for query failure and precedes both guard reads. */
    const uint64_t flags = read_quad(frame + offsetof(frame_anchors, query_00)
        + offsetof(query_anchors, flags_08));
    const uint64_t current_guard = read_quad(entry_guard_pointer);
    const uint64_t saved_guard = read_quad(frame + offsetof(frame_anchors, guard_a0));
    if (current_guard != saved_guard) {
        load_stack_fail(imports)();
#if defined(__x86_64__)
        __asm__ volatile("ud2" ::: "memory");
        __builtin_unreachable();
#else
        __builtin_trap();
#endif
    }
    const uint32_t failure_mask = query_result == 0 ? 0 : UINT32_MAX;
    const uint32_t flag = (uint32_t)((flags >> process_flag_shift) & 1);
    return (uint64_t)(failure_mask | flag);
}
