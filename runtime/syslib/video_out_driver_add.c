/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Adding a VideoOut driver brackets publication with the genuine module mutex.
   Once, lock and unlock failures do not select local error paths. Independent
   static reconstruction only; uncompiled/unvalidated, with actual initializer,
   module storage and compatible real import adapters still required. */
#include "runtime/video_out_driver_add.h"
#include "runtime/video_out_driver_publish.h"
#include "runtime/process.h"

#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut driver addition requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_driver_once_adapter_fn) == sizeof(uint64_t), "Once adapter pointer");
_Static_assert(sizeof(rt_video_out_driver_mutex_fn) == sizeof(uint64_t), "mutex pointer");

const rt_video_out_driver_add_bindings *volatile rt_video_out_driver_add_imports;

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage) rt_fatal(name, "the required module storage or import cell is unbound");
    return storage;
}

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_driver_add_bindings *imports) \
    { \
        type call; \
        const void *cell = require_storage(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_once_adapter, rt_video_out_driver_once_adapter_fn, once_adapter_slot, "VideoOut driver Once adapter")
IMPORT_READER(load_lock, rt_video_out_driver_mutex_fn, lock_slot, "VideoOut driver scePthreadMutexLock")
IMPORT_READER(load_unlock, rt_video_out_driver_mutex_fn, unlock_slot, "VideoOut driver scePthreadMutexUnlock")
#undef IMPORT_READER

uint64_t rt_video_out_add_driver(int32_t class_index, const void *operations,
    const void *parameters, uint64_t reserved_wire)
{
    const rt_video_out_driver_add_bindings *imports = rt_video_out_driver_add_imports;
    if (!imports)
        rt_fatal("VideoOut driver addition", "real module data and import adapters have not been bound");
    if (class_index != 0) {
        void *const once = (void *)require_storage(imports->once_storage, "VideoOut driver Once storage");
        const uint64_t initializer = imports->initializer_target;
        if (initializer == 0)
            rt_fatal("VideoOut driver Once initializer", "the actual module initializer target is unbound");
        load_once_adapter(imports)(once, initializer);
    }
    void *const mutex = (void *)require_storage(imports->mutex_storage, "VideoOut driver mutex storage");
    load_lock(imports)(mutex);
    const uint64_t result = rt_video_out_publish_driver(class_index, operations, parameters, reserved_wire);
    load_unlock(imports)(mutex);
    return result;
}
