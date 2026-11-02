/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VideoOut's Once initializer reports any mutex-init failure through a returning
   diagnostic tail. Success writes only ten DWORDs before backend initialization.
   Preserve those branches and full tail returns. Independent static source
   reconstruction, uncompiled/unvalidated; genuine bindings/bridges required. */
#include "runtime/video_out_once_initialize.h"
#include "runtime/video_out_backend_initialize.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut Once initialization requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_once_mutex_init_fn) == sizeof(uint64_t), "mutex-init pointer");
_Static_assert(sizeof(rt_video_out_once_diagnostic_bridge_fn) == sizeof(uint64_t), "diagnostic bridge pointer");

const rt_video_out_once_initialize_bindings *volatile rt_video_out_once_initialize_imports;

enum { descriptor_stride = 320, early_vacant_descriptors = 7, last_vacant_descriptor = 7 };
static const uint32_t vacant_class = UINT32_MAX;

typedef struct {
    uint8_t class_zero_00[sizeof(uint32_t)];
    uint8_t unknown_04[12];
    uint8_t class_four_10[sizeof(uint32_t)];
} map_anchors;
_Static_assert(offsetof(map_anchors, class_zero_00) == 0x00, "first class map word");
_Static_assert(offsetof(map_anchors, class_four_10) == 0x10, "fifth class map word");

typedef struct {
    uint8_t class_00[sizeof(uint32_t)];
    uint8_t unknown_04[descriptor_stride - sizeof(uint32_t)];
} descriptor_anchors;
_Static_assert(offsetof(descriptor_anchors, class_00) == 0x00, "descriptor class");
_Static_assert(sizeof(descriptor_anchors) == descriptor_stride, "descriptor stride");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static uint64_t read_quad(const void *storage)
{
    uint64_t value;
    access_boundary();
    memcpy(&value, storage, sizeof value);
    __asm__ volatile("" : : "r"(value) : "memory");
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
    if (!storage) rt_fatal(name, "the required module storage, literal or import cell is unbound");
    return storage;
}

static rt_video_out_once_mutex_init_fn load_mutex_init(const rt_video_out_once_initialize_bindings *imports)
{
    rt_video_out_once_mutex_init_fn call;
    const void *cell = require_storage(imports->mutex_init_slot, "VideoOut Once scePthreadMutexInit");
    access_boundary();
    memcpy(&call, cell, sizeof call);
    access_boundary();
    if (!call) rt_fatal("VideoOut Once scePthreadMutexInit", "the function import target is unbound");
    return call;
}

uint64_t rt_video_out_initialize_once_with_frame(void *backend_frame_storage)
{
    const rt_video_out_once_initialize_bindings *imports = rt_video_out_once_initialize_imports;
    if (!imports)
        rt_fatal("VideoOut Once initialization", "real module data and import adapters have not been bound");
    void *const mutex = (void *)require_storage(imports->mutex_storage, "VideoOut Once mutex storage");
    const char *name = require_storage(imports->mutex_name, "VideoOut Once mutex name");
    const int32_t result = load_mutex_init(imports)(mutex, NULL, name);
    if (result != 0) {
        const char *format = require_storage(imports->error_format, "VideoOut Once error format");
        const void *cell = require_storage(imports->printf_target_slot, "VideoOut Once raw printf target cell");
        const uint64_t target = read_quad(cell);
        if (target == 0)
            rt_fatal("VideoOut Once raw printf target", "the actual import target is unbound");
        if (!imports->diagnostic_bridge)
            rt_fatal("VideoOut Once diagnostic bridge", "full native printf RAX capture has not been bound");
        return imports->diagnostic_bridge(target, format, name, (uint32_t)result);
    }

    const uint64_t map = (uint64_t)(uintptr_t)require_storage(imports->class_map, "VideoOut Once class map");
    write_word(map + offsetof(map_anchors, class_zero_00), 0);
    const uint64_t descriptors = (uint64_t)(uintptr_t)require_storage(imports->descriptors, "VideoOut Once descriptors");
    for (uint64_t i = 0; i < early_vacant_descriptors; ++i)
        write_word(descriptors + i * descriptor_stride + offsetof(descriptor_anchors, class_00), vacant_class);
    write_word(map + offsetof(map_anchors, class_four_10), 1);
    write_word(descriptors + (uint64_t)last_vacant_descriptor * descriptor_stride
        + offsetof(descriptor_anchors, class_00), vacant_class);
    return rt_video_out_initialize_backend_with_frame(backend_frame_storage);
}
