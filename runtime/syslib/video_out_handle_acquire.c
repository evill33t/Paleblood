/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Native VideoOut handle acquisition serializes access and may wait before
   incrementing the acquisition count. Recheck the full key and reload the
   object after each wait; the waited-on object can differ from the next one.
   Independent static reconstruction only; uncompiled/unvalidated, requiring
   genuine module storage and compatible real synchronization/callback bridges. */
#include "runtime/video_out_handle_acquire.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut handle acquisition requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_handle_once_adapter_fn) == sizeof(uint64_t), "Once adapter pointer");
_Static_assert(sizeof(rt_video_out_handle_mutex_fn) == sizeof(uint64_t), "mutex pointer");
_Static_assert(sizeof(rt_video_out_handle_wait_fn) == sizeof(uint64_t), "condition-wait pointer");

const rt_video_out_handle_acquire_bindings *volatile rt_video_out_handle_acquire_imports;

static const uint32_t word_sign = UINT32_C(0x80000000);
static const uint32_t handle_slot_mask = UINT32_C(0xff);
static const uint32_t last_handle_slot = 3;

typedef struct {
    uint8_t key_00[sizeof(uint32_t)];
    uint8_t unknown_04[4];
    uint8_t object_08[sizeof(uint64_t)];
} slot_anchors;
_Static_assert(offsetof(slot_anchors, key_00) == 0x00, "slot key");
_Static_assert(offsetof(slot_anchors, object_08) == 0x08, "slot object");
_Static_assert(sizeof(slot_anchors) == 0x10, "slot stride");

/* Only reached fields/addresses are described, not a complete object. */
typedef struct {
    uint8_t unknown_00[8];
    uint8_t descriptor_08[sizeof(uint64_t)];
    uint8_t unknown_10[0x40];
    uint8_t condition_50[8];
    uint8_t waiters_58[sizeof(uint32_t)];
    uint8_t acquisitions_5c[sizeof(uint32_t)];
    uint8_t state_60[sizeof(uint32_t)];
    uint8_t unknown_64[4];
    uint8_t state_68[sizeof(uint32_t)];
} object_anchors;
_Static_assert(offsetof(object_anchors, descriptor_08) == 0x08, "descriptor pointer");
_Static_assert(offsetof(object_anchors, condition_50) == 0x50, "condition address");
_Static_assert(offsetof(object_anchors, waiters_58) == 0x58, "waiter count");
_Static_assert(offsetof(object_anchors, acquisitions_5c) == 0x5c, "acquisition count");
_Static_assert(offsetof(object_anchors, state_60) == 0x60, "state gate");
_Static_assert(offsetof(object_anchors, state_68) == 0x68, "state gate");

typedef struct {
    uint8_t unknown_00[8];
    uint8_t gate_08[sizeof(uint32_t)];
} descriptor_anchors;
_Static_assert(offsetof(descriptor_anchors, gate_08) == 0x08, "descriptor gate");

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

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_handle_acquire_bindings *imports) \
    { \
        type call; \
        const void *cell = require_storage(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_once_adapter, rt_video_out_handle_once_adapter_fn, once_adapter_slot, "VideoOut handle Once adapter")
IMPORT_READER(load_lock, rt_video_out_handle_mutex_fn, lock_slot, "VideoOut handle scePthreadMutexLock")
IMPORT_READER(load_unlock, rt_video_out_handle_mutex_fn, unlock_slot, "VideoOut handle scePthreadMutexUnlock")
IMPORT_READER(load_wait, rt_video_out_handle_wait_fn, wait_slot, "VideoOut handle scePthreadCondWait")
#undef IMPORT_READER

uint64_t rt_video_out_acquire_handle(uint32_t handle_bits)
{
    const rt_video_out_handle_acquire_bindings *imports = rt_video_out_handle_acquire_imports;
    if (!imports)
        rt_fatal("VideoOut handle acquisition", "real module data and import adapters have not been bound");
    void *const once = (void *)require_storage(imports->once_storage, "VideoOut handle Once storage");
    const uint64_t initializer = imports->initializer_target;
    if (initializer == 0)
        rt_fatal("VideoOut handle Once initializer", "the actual module initializer target is unbound");
    load_once_adapter(imports)(once, initializer);
    void *const mutex = (void *)require_storage(imports->mutex_storage, "VideoOut handle mutex storage");
    load_lock(imports)(mutex);

    uint64_t result = 0;
    const uint32_t index = handle_bits & handle_slot_mask;
    if (handle_bits != 0 && (handle_bits & word_sign) == 0 && index <= last_handle_slot) {
        const uint64_t table = (uint64_t)(uintptr_t)require_storage(imports->slots_storage, "VideoOut handle slots");
        const uint64_t slot = table + (uint64_t)index * sizeof(slot_anchors);
        if (read_word(slot + offsetof(slot_anchors, key_00)) == handle_bits) {
            for (;;) {
                const uint64_t object = read_quad(slot + offsetof(slot_anchors, object_08));
                if (object == 0) break;
                if (read_word(object + offsetof(object_anchors, state_68)) != 0) break;
                if (read_word(object + offsetof(object_anchors, state_60)) != 0) break;
                const uint64_t descriptor = read_quad(object + offsetof(object_anchors, descriptor_08));
                const uint32_t gate = read_word(descriptor + offsetof(descriptor_anchors, gate_08));
                /* The count read precedes either branch, including a zero gate. */
                const uint32_t count = read_word(object + offsetof(object_anchors, acquisitions_5c));
                if (gate == 0 || count == 0 || (count & word_sign) != 0) {
                    write_word(object + offsetof(object_anchors, acquisitions_5c), count + UINT32_C(1));
                    result = object;
                    break;
                }
                const uint64_t waiters = object + offsetof(object_anchors, waiters_58);
                write_word(waiters, read_word(waiters) + UINT32_C(1));
                load_wait(imports)((void *)(uintptr_t)(object + offsetof(object_anchors, condition_50)), mutex);
                write_word(waiters, read_word(waiters) - UINT32_C(1));
                if (read_word(slot + offsetof(slot_anchors, key_00)) != handle_bits) break;
            }
        }
    }
    load_unlock(imports)(mutex);
    return result;
}
