/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Device descriptor lookup always attempts real Once before its live DWORD
   read. The initializer wrapper ignores the synchronization result entirely. */
#include "runtime/video_out_device_descriptor.h"
#include "runtime/process.h"

#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut device descriptor access requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_device_once_fn) == sizeof(uint64_t), "Once adapter pointer");

const rt_video_out_device_descriptor_bindings *volatile rt_video_out_device_descriptor_imports;

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static const rt_video_out_device_descriptor_bindings *require_bindings(void)
{
    const rt_video_out_device_descriptor_bindings *imports = rt_video_out_device_descriptor_imports;
    if (!imports)
        rt_fatal("VideoOut device descriptor", "real module data and Once adapter have not been bound");
    return imports;
}

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage)
        rt_fatal(name, "the required module storage or import adapter cell is unbound");
    return storage;
}

static rt_video_out_device_once_fn load_once(const rt_video_out_device_descriptor_bindings *imports)
{
    rt_video_out_device_once_fn call;
    const void *slot = require_storage(imports->once_adapter_slot, "VideoOut device Once adapter");
    access_boundary();
    memcpy(&call, slot, sizeof call);
    access_boundary();
    if (!call)
        rt_fatal("VideoOut device Once adapter", "the function import target is unbound");
    return call;
}

static uint64_t initialize(const rt_video_out_device_descriptor_bindings *imports)
{
    void *const once = (void *)require_storage(imports->once_storage, "VideoOut device Once storage");
    const uint64_t initializer = imports->initializer_target;
    if (initializer == 0)
        rt_fatal("VideoOut device initializer", "the actual module initializer target is unbound");
    load_once(imports)(once, initializer);
    return UINT64_C(0);
}

uint64_t rt_video_out_ensure_device_initialization(void)
{
    return initialize(require_bindings());
}

uint64_t rt_video_out_read_device_descriptor(void)
{
    const rt_video_out_device_descriptor_bindings *imports = require_bindings();
    initialize(imports);
    const void *word = require_storage(imports->device_word, "VideoOut device descriptor word");
    uint32_t value;
    access_boundary();
    memcpy(&value, word, sizeof value);
    __asm__ volatile("" : : "r"((uint64_t)value) : "memory");
    return (uint64_t)value;
}
