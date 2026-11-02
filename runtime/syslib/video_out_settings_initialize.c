/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VideoOut cached settings retain partial initialization effects. Mutex errors
   do not stop later queries, AppInfo uses conditional reads of uncleared scratch
   storage, and the normal return is a full guard QWORD rather than a status. */
#include "runtime/video_out_settings_initialize.h"
#include "runtime/video_out_process_flag.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut settings initialization requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
#define POINTER_WIDTH(type) _Static_assert(sizeof(type) == sizeof(uint64_t), #type " pointer")
POINTER_WIDTH(rt_video_out_settings_mutex_fn);
POINTER_WIDTH(rt_video_out_settings_mode_fn);
POINTER_WIDTH(rt_video_out_settings_pid_fn);
POINTER_WIDTH(rt_video_out_settings_app_info_fn);
POINTER_WIDTH(rt_video_out_settings_stack_fail_fn);
#undef POINTER_WIDTH

const rt_video_out_settings_initialize_bindings *volatile rt_video_out_settings_initialize_imports;

static const uint32_t app_info_mask = UINT32_C(0x20000000);

typedef struct {
    uint8_t unknown_00[8];
    uint8_t output_08[0x28];
    uint8_t mask_30[sizeof(uint32_t)];
    uint8_t predicate_34[sizeof(uint32_t)];
    uint8_t unknown_38[0x18];
    uint8_t guard_50[sizeof(uint64_t)];
} frame_anchors;
_Static_assert(offsetof(frame_anchors, output_08) == 0x08, "AppInfo output address");
_Static_assert(offsetof(frame_anchors, mask_30) == 0x30, "AppInfo output+28");
_Static_assert(offsetof(frame_anchors, predicate_34) == 0x34, "AppInfo output+2c");
_Static_assert(offsetof(frame_anchors, guard_50) == 0x50, "saved guard");
_Static_assert(sizeof(frame_anchors) == RT_VIDEO_OUT_SETTINGS_FRAME_BYTES, "frame extent");

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

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage)
        rt_fatal(name, "the required module storage, literal or import cell is unbound");
    return storage;
}

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_settings_initialize_bindings *imports) \
    { \
        type call; \
        const void *cell = require_storage(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_mutex_init, rt_video_out_settings_mutex_fn, mutex_init_slot, "VideoOut settings scePthreadMutexInit")
IMPORT_READER(load_is_neo, rt_video_out_settings_mode_fn, is_neo_slot, "VideoOut settings sceKernelIsNeoMode")
IMPORT_READER(load_has_neo, rt_video_out_settings_mode_fn, has_neo_slot, "VideoOut settings sceKernelHasNeoMode")
IMPORT_READER(load_getpid, rt_video_out_settings_pid_fn, getpid_slot, "VideoOut settings getpid")
IMPORT_READER(load_app_info, rt_video_out_settings_app_info_fn, app_info_slot, "VideoOut settings sceKernelGetAppInfo")
IMPORT_READER(load_stack_fail, rt_video_out_settings_stack_fail_fn, stack_fail_slot, "VideoOut settings __stack_chk_fail")
#undef IMPORT_READER

static void publish_word(void *storage, uint32_t value, const char *name)
{
    const void *word = require_storage(storage, name);
    write_word((uint64_t)(uintptr_t)word, value);
}

static uint64_t finish(const rt_video_out_settings_initialize_bindings *imports,
    uint64_t frame, uint64_t entry_guard_pointer)
{
    const uint64_t current = read_quad(entry_guard_pointer);
    const uint64_t saved = read_quad(frame + offsetof(frame_anchors, guard_50));
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

uint64_t rt_video_out_initialize_settings_with_frames(void *frame_storage,
    void *process_frame_storage)
{
    const rt_video_out_settings_initialize_bindings *imports = rt_video_out_settings_initialize_imports;
    if (!imports)
        rt_fatal("VideoOut settings initialization", "real module data and import cells have not been bound");
    const uint64_t frame = (uint64_t)(uintptr_t)require_storage(frame_storage, "VideoOut settings borrowed frame");
    const void *guard_cell = require_storage(imports->guard_pointer_slot, "VideoOut settings guard pointer cell");
    const uint64_t entry_guard_pointer = read_quad((uint64_t)(uintptr_t)guard_cell);
    write_quad(frame + offsetof(frame_anchors, guard_50), read_quad(entry_guard_pointer));
    void *mutex = (void *)require_storage(imports->mutex_storage, "VideoOut settings mutex storage");
    const char *mutex_name = require_storage(imports->mutex_name, "VideoOut settings mutex name");
    const uint32_t mutex_result = load_mutex_init(imports)(mutex, NULL, mutex_name);
    publish_word(imports->mutex_result_word, mutex_result, "VideoOut settings mutex result word");

    const uint32_t is_nonpositive = load_is_neo(imports)() <= 0;
    const uint32_t has_positive = load_has_neo(imports)() > 0;
    const uint32_t first_pid = load_getpid(imports)();
    const uint64_t process_flag = rt_video_out_query_process_flag_with_frame(
        (uint64_t)first_pid, process_frame_storage);
    publish_word(imports->has_neo_word, has_positive, "VideoOut settings HasNeo word");
    publish_word(imports->combined_mode_word, ((uint32_t)process_flag != 1) && is_nonpositive,
        "VideoOut settings combined mode word");

    const uint32_t second_pid = load_getpid(imports)();
    const int32_t app_result = load_app_info(imports)((uint64_t)second_pid,
        (void *)(uintptr_t)(frame + offsetof(frame_anchors, output_08)));
    uint32_t result = 1;
    if (app_result >= 0 && read_word(frame + offsetof(frame_anchors, predicate_34)) != 0)
        result = read_word(frame + offsetof(frame_anchors, mask_30)) & app_info_mask;
    publish_word(imports->app_info_mask_word, result, "VideoOut settings AppInfo mask word");
    return finish(imports, frame, entry_guard_pointer);
}
