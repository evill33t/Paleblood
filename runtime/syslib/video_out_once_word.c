/* SPDX-License-Identifier: GPL-2.0-or-later */
/* This VideoOut helper returns a live module word after attempting real Once;
   the synchronization result is ignored. The initializer's effects and the
   word's meaning are not established here. Independent static reconstruction,
   uncompiled/unvalidated; genuine module/callback bridge bindings required. */
#include "runtime/video_out_once_word.h"
#include "runtime/process.h"

#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut Once-word access requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_word_once_adapter_fn) == sizeof(uint64_t), "Once adapter pointer");

const rt_video_out_once_word_bindings *volatile rt_video_out_once_word_imports;

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage) rt_fatal(name, "the required module storage or import cell is unbound");
    return storage;
}

static rt_video_out_word_once_adapter_fn load_once_adapter(const rt_video_out_once_word_bindings *imports)
{
    rt_video_out_word_once_adapter_fn call;
    const void *cell = require_storage(imports->once_adapter_slot, "VideoOut word Once adapter");
    access_boundary();
    memcpy(&call, cell, sizeof call);
    access_boundary();
    if (!call) rt_fatal("VideoOut word Once adapter", "the function import target is unbound");
    return call;
}

uint64_t rt_video_out_read_once_word(void)
{
    const rt_video_out_once_word_bindings *imports = rt_video_out_once_word_imports;
    if (!imports)
        rt_fatal("VideoOut Once-word access", "real module data and import adapters have not been bound");
    void *const once = (void *)require_storage(imports->once_storage, "VideoOut word Once storage");
    const uint64_t initializer = imports->initializer_target;
    if (initializer == 0)
        rt_fatal("VideoOut word Once initializer", "the actual module initializer target is unbound");
    load_once_adapter(imports)(once, initializer);

    const void *storage = require_storage(imports->result_word, "VideoOut result word");
    uint32_t value;
    access_boundary();
    memcpy(&value, storage, sizeof value);
    __asm__ volatile("" : : "r"((uint64_t)value) : "memory");
    return (uint64_t)value;
}
