/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The default VideoOut backend wrapper diagnoses a failed lock and continues.
   Preserve that path, the subsequent live state read, and the registration
   result across unlock. This is independent static source reconstruction;
   uncompiled/unvalidated, with real module/import bindings still required. */
#include "runtime/video_out_backend_buffers.h"
#include "runtime/video_out_buffers.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut backend registration requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(int) == sizeof(uint32_t), "32-bit printf int");
_Static_assert(sizeof(unsigned int) == sizeof(uint32_t), "32-bit printf unsigned int");
_Static_assert(sizeof(rt_video_out_backend_buffers_mutex_fn) == sizeof(uint64_t), "mutex pointer");
_Static_assert(sizeof(rt_video_out_backend_buffers_printf_fn) == sizeof(uint64_t), "printf pointer");

const rt_video_out_backend_buffers_bindings *volatile rt_video_out_backend_buffers_imports;

static const uint32_t address_error = UINT32_C(0x80290002);
static const uint32_t backend_state_error = UINT32_C(0x8029000b);
static const int lock_diagnostic_site = 0x166;

/* Only the reached word is described; this is not a complete foreign object. */
typedef struct {
    uint8_t unknown_00[0x46c];
    uint8_t state_46c[sizeof(uint32_t)];
} backend_anchors;
_Static_assert(offsetof(backend_anchors, state_46c) == 0x46c, "backend state offset");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static const void *require_cell(const void *cell, const char *name)
{
    if (!cell) rt_fatal(name, "the required module storage or import cell is unbound");
    return cell;
}

static uint32_t read_backend_state(uint64_t backend)
{
    const void *storage = (const void *)(uintptr_t)(backend + offsetof(backend_anchors, state_46c));
    uint32_t value;
    access_boundary();
    memcpy(&value, storage, sizeof value);
    __asm__ volatile("" : : "r"((uint64_t)value) : "memory");
    return value;
}

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_backend_buffers_bindings *imports) \
    { \
        type call; \
        const void *cell = require_cell(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_lock, rt_video_out_backend_buffers_mutex_fn, lock_slot, "VideoOut backend scePthreadMutexLock")
IMPORT_READER(load_unlock, rt_video_out_backend_buffers_mutex_fn, unlock_slot, "VideoOut backend scePthreadMutexUnlock")
IMPORT_READER(load_printf, rt_video_out_backend_buffers_printf_fn, printf_slot, "VideoOut backend printf")
#undef IMPORT_READER

uint64_t rt_video_out_register_backend_buffers(void *backend_storage, uint32_t index,
    const void *address_storage, uint32_t count, const void *attribute_storage)
{
    /* This native early result needs neither a backend nor module bindings. */
    if (!address_storage) return (uint64_t)address_error;
    const rt_video_out_backend_buffers_bindings *imports = rt_video_out_backend_buffers_imports;
    if (!imports)
        rt_fatal("VideoOut backend buffer registration", "real module data and import cells have not been bound");
    const uint64_t backend = (uint64_t)(uintptr_t)backend_storage;
    void *const mutex_storage = (void *)require_cell(imports->mutex_storage, "VideoOut backend mutex storage");
    const int32_t locked = load_lock(imports)(mutex_storage);
    if (locked < 0) {
        const char *format = require_cell(imports->lock_error_format, "VideoOut backend lock diagnostic format");
        load_printf(imports)(format, (int)lock_diagnostic_site, (unsigned int)(uint32_t)locked);
    }

    uint32_t status = backend_state_error;
    if (read_backend_state(backend) != 0) {
        /* MOV ESI in the wrapper zeroes the full index wire saved by the lower. */
        status = (uint32_t)rt_video_out_register_buffers((void *)(uintptr_t)backend,
            (uint64_t)index, address_storage, count, attribute_storage, 0);
    }
    load_unlock(imports)(mutex_storage);
    return (uint64_t)status;
}
