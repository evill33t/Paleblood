/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VideoOut backend initialization continues after negative attribute results.
   Preserve shared scratch storage and the full guard return independently of
   the published driver. Independent static reconstruction, uncompiled and
   unvalidated; real module/literal/import bindings and frame transport required. */
#include "runtime/video_out_backend_initialize.h"
#include "runtime/video_out_driver_add.h"
#include "runtime/video_out_once_word.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut backend initialization requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(int) == sizeof(uint32_t), "32-bit printf int");
_Static_assert(sizeof(unsigned int) == sizeof(uint32_t), "32-bit printf unsigned int");
#define POINTER_WIDTH(type) _Static_assert(sizeof(type) == sizeof(uint64_t), #type " pointer")
POINTER_WIDTH(rt_video_out_initialize_open_fn);
POINTER_WIDTH(rt_video_out_initialize_sysctl_fn);
POINTER_WIDTH(rt_video_out_initialize_attribute_fn);
POINTER_WIDTH(rt_video_out_initialize_settype_fn);
POINTER_WIDTH(rt_video_out_initialize_mutex_fn);
POINTER_WIDTH(rt_video_out_initialize_printf_fn);
POINTER_WIDTH(rt_video_out_initialize_stack_fail_fn);
#undef POINTER_WIDTH

const rt_video_out_backend_initialize_bindings *volatile rt_video_out_backend_initialize_imports;

enum {
    device_open_flags = 2,
    mutex_type_wire = 2,
    zero_chunk_bytes = 32,
    zero_chunk_shift = 16,
    driver_packet_bytes = 48,
    attribute_init_site = 0x218,
    attribute_settype_site = 0x21b,
    mutex_init_site = 0x21e,
    attribute_destroy_site = 0x221
};

typedef struct {
    uint8_t unknown_00[0x18];
    uint8_t sdk_and_attributes_18[8];
    uint8_t length_and_packet_20[driver_packet_bytes];
    uint8_t unknown_50[8];
    uint8_t guard_58[sizeof(uint64_t)];
} frame_anchors;
_Static_assert(offsetof(frame_anchors, sdk_and_attributes_18) == 0x18, "SDK/attribute address");
_Static_assert(offsetof(frame_anchors, length_and_packet_20) == 0x20, "length/packet address");
_Static_assert(offsetof(frame_anchors, guard_58) == 0x58, "saved guard");
_Static_assert(sizeof(frame_anchors) == RT_VIDEO_OUT_INITIALIZE_FRAME_BYTES, "frame extent");

typedef struct {
    uint8_t unknown_00[8];
    uint8_t name_08[sizeof(uint64_t)];
    uint8_t unknown_10[0x20];
} packet_anchors;
_Static_assert(offsetof(packet_anchors, name_08) == 0x08, "driver name pointer");
_Static_assert(sizeof(packet_anchors) == driver_packet_bytes, "driver packet extent");

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

static void write_quad(uint64_t address, uint64_t value)
{
    access_boundary();
    memcpy((void *)(uintptr_t)address, &value, sizeof value);
    access_boundary();
}

static void write_word(uint64_t address, uint32_t value)
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
    if (!storage) rt_fatal(name, "the required module storage, literal or import cell is unbound");
    return storage;
}

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_backend_initialize_bindings *imports) \
    { \
        type call; \
        const void *cell = require_storage(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_open, rt_video_out_initialize_open_fn, open_slot, "VideoOut initialize open")
IMPORT_READER(load_sysctl, rt_video_out_initialize_sysctl_fn, sysctl_slot, "VideoOut initialize sysctlbyname")
IMPORT_READER(load_attribute_init, rt_video_out_initialize_attribute_fn, attribute_init_slot, "VideoOut initialize scePthreadMutexattrInit")
IMPORT_READER(load_attribute_settype, rt_video_out_initialize_settype_fn, attribute_settype_slot, "VideoOut initialize scePthreadMutexattrSettype")
IMPORT_READER(load_mutex_init, rt_video_out_initialize_mutex_fn, mutex_init_slot, "VideoOut initialize scePthreadMutexInit")
IMPORT_READER(load_attribute_destroy, rt_video_out_initialize_attribute_fn, attribute_destroy_slot, "VideoOut initialize scePthreadMutexattrDestroy")
IMPORT_READER(load_printf, rt_video_out_initialize_printf_fn, printf_slot, "VideoOut initialize printf")
IMPORT_READER(load_stack_fail, rt_video_out_initialize_stack_fail_fn, stack_fail_slot, "VideoOut initialize __stack_chk_fail")
#undef IMPORT_READER

static void diagnose_negative(const rt_video_out_backend_initialize_bindings *imports,
    int site, int32_t result)
{
    if (result < 0) {
        const char *format = require_storage(imports->error_format, "VideoOut initialize error format");
        load_printf(imports)(format, site, (unsigned int)(uint32_t)result);
    }
}

static uint64_t finish(const rt_video_out_backend_initialize_bindings *imports,
    uint64_t frame, uint64_t entry_guard_pointer)
{
    /* The pointer is cached at entry; both pointees are read again here. */
    const uint64_t current = read_quad(entry_guard_pointer);
    const uint64_t saved = read_quad(frame + offsetof(frame_anchors, guard_58));
    if (current != saved) {
        load_stack_fail(imports)();
#if defined(__x86_64__)
        __asm__ volatile("ud2" ::: "memory");
        __builtin_unreachable();
#else
        __builtin_trap();
#endif
    }
    return current;
}

uint64_t rt_video_out_initialize_backend_with_frame(void *frame_storage)
{
    const rt_video_out_backend_initialize_bindings *imports = rt_video_out_backend_initialize_imports;
    if (!imports)
        rt_fatal("VideoOut backend initialization", "real module data and import cells have not been bound");
    const uint64_t frame = (uint64_t)(uintptr_t)require_storage(frame_storage, "VideoOut initialize borrowed frame");
    const void *guard_cell = require_storage(imports->guard_pointer_slot, "VideoOut initialize guard pointer cell");
    const uint64_t entry_guard_pointer = read_quad((uint64_t)(uintptr_t)guard_cell);
    write_quad(frame + offsetof(frame_anchors, guard_58), read_quad(entry_guard_pointer));
    const char *device_path = require_storage(imports->device_path, "VideoOut initialize device path");
    const int32_t descriptor = load_open(imports)(device_path, device_open_flags);
    if (descriptor < 0) return finish(imports, frame, entry_guard_pointer);

    void *descriptor_word = (void *)require_storage(imports->device_descriptor_word, "VideoOut initialize device descriptor word");
    write_word((uint64_t)(uintptr_t)descriptor_word, (uint32_t)descriptor);
    const uint64_t attributes = frame + offsetof(frame_anchors, sdk_and_attributes_18);
    const uint64_t packet = frame + offsetof(frame_anchors, length_and_packet_20);
    write_word(attributes, 0);
    write_quad(packet, sizeof(uint32_t));
    const char *sdk_name = require_storage(imports->sdk_name, "VideoOut initialize SDK query name");
    uint32_t sdk = 0;
    if (load_sysctl(imports)(sdk_name, (void *)(uintptr_t)attributes,
            (void *)(uintptr_t)packet, NULL, 0) >= 0)
        sdk = read_word(attributes);
    void *sdk_word = (void *)require_storage(imports->sdk_word, "VideoOut initialize SDK word");
    write_word((uint64_t)(uintptr_t)sdk_word, sdk);

    diagnose_negative(imports, attribute_init_site, load_attribute_init(imports)((void *)(uintptr_t)attributes));
    diagnose_negative(imports, attribute_settype_site, load_attribute_settype(imports)((void *)(uintptr_t)attributes, mutex_type_wire));
    void *mutex = (void *)require_storage(imports->mutex_storage, "VideoOut initialize mutex storage");
    const char *mutex_name = require_storage(imports->mutex_name, "VideoOut initialize mutex name");
    diagnose_negative(imports, mutex_init_site, load_mutex_init(imports)(mutex, (void *)(uintptr_t)attributes, mutex_name));
    diagnose_negative(imports, attribute_destroy_site, load_attribute_destroy(imports)((void *)(uintptr_t)attributes));
    rt_video_out_read_once_word();

    const char *driver_name = require_storage(imports->driver_name, "VideoOut initialize driver name");
    const void *operations = require_storage(imports->driver_operations, "VideoOut initialize driver operations");
    zero_chunk(packet);
    zero_chunk(packet + zero_chunk_shift);
    write_quad(packet + offsetof(packet_anchors, name_08), (uint64_t)(uintptr_t)driver_name);
    const uint64_t driver = rt_video_out_add_driver(0, operations, (void *)(uintptr_t)packet, 0);
    void *driver_word = (void *)require_storage(imports->driver_word, "VideoOut initialize driver word");
    write_quad((uint64_t)(uintptr_t)driver_word, driver);
    return finish(imports, frame, entry_guard_pointer);
}
