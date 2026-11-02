/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The paired VideoOut settings-mutex tails replace only RDI. Keep native state
   transport explicit and retain the imported callee's full raw return value. */
#include "runtime/video_out_settings_lock.h"
#include "runtime/process.h"

#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut settings mutex tails require a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_settings_mutex_tail_fn) == sizeof(uint64_t), "bridge pointer");

const rt_video_out_settings_lock_bindings *volatile rt_video_out_settings_lock_imports;

enum mutex_operation { lock_operation, unlock_operation };

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage)
        rt_fatal(name, "the required real module binding or captured native context is unbound");
    return storage;
}

static uint64_t read_target(const void *cell)
{
    uint64_t target;
    __asm__ volatile("" ::: "memory");
    memcpy(&target, cell, sizeof target);
    __asm__ volatile("" : : "r"(target) : "memory");
    return target;
}

static uint64_t transfer(enum mutex_operation operation, void *native_context)
{
    const char *name = operation == lock_operation
        ? "VideoOut settings mutex lock tail" : "VideoOut settings mutex unlock tail";
    const rt_video_out_settings_lock_bindings *imports = rt_video_out_settings_lock_imports;
    if (!imports)
        rt_fatal(name, "real module data and a native tail bridge have not been bound");
    void *mutex = (void *)require_storage(imports->mutex_storage, name);
    const void *cell = require_storage(operation == lock_operation
        ? imports->lock_slot : imports->unlock_slot, name);
    const uint64_t target = read_target(cell);
    const rt_video_out_settings_mutex_tail_fn bridge = imports->invoke_tail;
    if (!bridge)
        rt_fatal(name, "the genuine native-state tail bridge is unbound");
    void *context = (void *)require_storage(native_context, name);
    return bridge(target, mutex, context);
}

uint64_t rt_video_out_lock_settings_with_context(void *native_context)
{
    return transfer(lock_operation, native_context);
}

uint64_t rt_video_out_unlock_settings_with_context(void *native_context)
{
    return transfer(unlock_operation, native_context);
}
