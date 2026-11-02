/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Native VideoOut registration dispatches through the acquired object's live
   driver table. Missing callbacks still release the acquisition; callback
   results survive the lock/decrement/broadcast/unlock sequence. Independent
   static reconstruction only, uncompiled/unvalidated with real bindings needed. */
#include "runtime/video_out_native_buffers.h"
#include "runtime/video_out_handle_acquire.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Native VideoOut registration requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_native_register_bridge_fn) == sizeof(uint64_t), "register bridge pointer");
_Static_assert(sizeof(rt_video_out_native_mutex_fn) == sizeof(uint64_t), "mutex pointer");
_Static_assert(sizeof(rt_video_out_native_broadcast_fn) == sizeof(uint64_t), "broadcast pointer");

const rt_video_out_native_buffer_bindings *volatile rt_video_out_native_buffer_imports;

static const uint32_t address_error = UINT32_C(0x80290002);
static const uint32_t object_error = UINT32_C(0x8029000b);
static const uint32_t missing_callback_error = UINT32_C(0x802900ff);

/* Only reached storage/addresses are described, not a complete foreign object. */
typedef struct {
    uint8_t backend_00[sizeof(uint64_t)];
    uint8_t descriptor_08[sizeof(uint64_t)];
    uint8_t unknown_10[0x40];
    uint8_t condition_50[8];
    uint8_t unknown_58[4];
    uint8_t acquisitions_5c[sizeof(uint32_t)];
} object_anchors;
_Static_assert(offsetof(object_anchors, backend_00) == 0x00, "backend word");
_Static_assert(offsetof(object_anchors, descriptor_08) == 0x08, "descriptor pointer");
_Static_assert(offsetof(object_anchors, condition_50) == 0x50, "condition address");
_Static_assert(offsetof(object_anchors, acquisitions_5c) == 0x5c, "acquisition count");

typedef struct {
    uint8_t unknown_00[0x50];
    uint8_t register_target_50[sizeof(uint64_t)];
} descriptor_anchors;
_Static_assert(offsetof(descriptor_anchors, register_target_50) == 0x50, "register callback target");

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

static uint32_t read_word(uint64_t address)
{
    uint32_t value;
    access_boundary();
    memcpy(&value, (const void *)(uintptr_t)address, sizeof value);
    __asm__ volatile("" : : "r"((uint64_t)value) : "memory");
    return value;
}

static void write_word(uint64_t address, uint32_t value)
{
    access_boundary();
    memcpy((void *)(uintptr_t)address, &value, sizeof value);
    access_boundary();
}

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage) rt_fatal(name, "the required module storage or import cell is unbound");
    return storage;
}

static const rt_video_out_native_buffer_bindings *load_bindings(void)
{
    const rt_video_out_native_buffer_bindings *imports = rt_video_out_native_buffer_imports;
    if (!imports)
        rt_fatal("Native VideoOut registration", "real module data and import/callback bridges have not been bound");
    return imports;
}

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_native_buffer_bindings *imports) \
    { \
        type call; \
        const void *cell = require_storage(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_lock, rt_video_out_native_mutex_fn, lock_slot, "Native VideoOut scePthreadMutexLock")
IMPORT_READER(load_broadcast, rt_video_out_native_broadcast_fn, broadcast_slot, "Native VideoOut scePthreadCondBroadcast")
IMPORT_READER(load_unlock, rt_video_out_native_mutex_fn, unlock_slot, "Native VideoOut scePthreadMutexUnlock")
#undef IMPORT_READER

uint64_t rt_video_out_register_native_buffers(uint32_t handle_bits, uint32_t index,
    const void *addresses, uint32_t count, const void *attributes)
{
    if (!addresses) return (uint64_t)address_error;
    if (!attributes) return (uint64_t)address_error;
    const uint64_t object = rt_video_out_acquire_handle(handle_bits);
    if (object == 0) return (uint64_t)object_error;

    const uint64_t descriptor = read_quad(object + offsetof(object_anchors, descriptor_08));
    const uint64_t target = read_quad(descriptor + offsetof(descriptor_anchors, register_target_50));
    const rt_video_out_native_buffer_bindings *imports = NULL;
    uint32_t result = missing_callback_error;
    if (target != 0) {
        const uint64_t backend = read_quad(object + offsetof(object_anchors, backend_00));
        imports = load_bindings();
        if (!imports->register_bridge)
            rt_fatal("Native VideoOut register bridge", "the actual opaque callback ABI has not been bound");
        result = imports->register_bridge(target, backend, index, addresses, count, attributes);
    }
    if (!imports) imports = load_bindings();
    void *const mutex = (void *)require_storage(imports->mutex_storage, "Native VideoOut mutex storage");
    load_lock(imports)(mutex);
    const uint64_t counter = object + offsetof(object_anchors, acquisitions_5c);
    write_word(counter, read_word(counter) - UINT32_C(1));
    load_broadcast(imports)((void *)(uintptr_t)(object + offsetof(object_anchors, condition_50)));
    load_unlock(imports)(mutex);
    return (uint64_t)result;
}
