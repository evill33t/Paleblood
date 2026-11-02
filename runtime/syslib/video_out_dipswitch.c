/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VideoOut's registration warning tail reads an actual DIP-switch device.
   Preserve the service result and error-conversion/print/close order; no guessed
   warning policy can replace the device operation. Uncompiled/unvalidated source
   profile from the supplied original. Borrowed frame storage retains known
   fields, not arbitrary native-stack aliases or unknown provider footprints. */
#include "runtime/video_out_dipswitch.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut DIP-switch access requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(unsigned int) == sizeof(uint32_t), "32-bit printf unsigned int");
_Static_assert(sizeof(rt_video_out_dipswitch_open_fn) == sizeof(uint64_t), "open pointer");
_Static_assert(sizeof(rt_video_out_dipswitch_ioctl_fn) == sizeof(uint64_t), "ioctl pointer");
_Static_assert(sizeof(rt_video_out_dipswitch_close_fn) == sizeof(uint64_t), "close pointer");
_Static_assert(sizeof(rt_video_out_dipswitch_error_fn) == sizeof(uint64_t), "error pointer");
_Static_assert(sizeof(rt_video_out_dipswitch_kernel_error_fn) == sizeof(uint64_t), "kernel error pointer");
_Static_assert(sizeof(rt_video_out_dipswitch_printf_fn) == sizeof(uint64_t), "printf pointer");
_Static_assert(sizeof(rt_video_out_dipswitch_stack_fail_fn) == sizeof(uint64_t), "fail pointer");

const rt_video_out_dipswitch_bindings *volatile rt_video_out_dipswitch_imports;

static const uint64_t dipswitch_request = UINT64_C(0x40048806);
static const uint32_t diagnostic_value = 6;

typedef struct {
    uint8_t unknown_00[8];
    uint8_t output_08[sizeof(uint32_t)];
    uint8_t unknown_0c[4];
    uint8_t guard_10[sizeof(uint64_t)];
} borrowed_frame;
_Static_assert(offsetof(borrowed_frame, output_08) == 8, "output offset");
_Static_assert(offsetof(borrowed_frame, guard_10) == 0x10, "saved guard offset");
_Static_assert(sizeof(borrowed_frame) == 0x18, "projected frame size");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static const void *require_cell(const void *cell, const char *name)
{
    if (!cell) rt_fatal(name, "the required module storage or import cell is unbound");
    return cell;
}

#define STORAGE_READER(name, type) \
    static type name(const void *storage) \
    { \
        type value; \
        access_boundary(); \
        memcpy(&value, storage, sizeof value); \
        __asm__ volatile("" : : "r"((uint64_t)value) : "memory"); \
        return value; \
    }
STORAGE_READER(read_word, uint32_t)
STORAGE_READER(read_quad, uint64_t)
#undef STORAGE_READER

static void write_word(void *storage, uint32_t value)
{
    access_boundary();
    memcpy(storage, &value, sizeof value);
    access_boundary();
}

static void write_quad(void *storage, uint64_t value)
{
    access_boundary();
    memcpy(storage, &value, sizeof value);
    access_boundary();
}

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_dipswitch_bindings *imports) \
    { \
        type call; \
        const void *cell = require_cell(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_open, rt_video_out_dipswitch_open_fn, open_slot, "VideoOut dipsw _open")
IMPORT_READER(load_ioctl, rt_video_out_dipswitch_ioctl_fn, ioctl_slot, "VideoOut dipsw _ioctl")
IMPORT_READER(load_close, rt_video_out_dipswitch_close_fn, close_slot, "VideoOut dipsw _close")
IMPORT_READER(load_error, rt_video_out_dipswitch_error_fn, error_slot, "VideoOut dipsw __error")
IMPORT_READER(load_kernel_error, rt_video_out_dipswitch_kernel_error_fn, kernel_error_slot, "VideoOut dipsw sceKernelError")
IMPORT_READER(load_printf, rt_video_out_dipswitch_printf_fn, printf_slot, "VideoOut dipsw printf")
IMPORT_READER(load_stack_fail, rt_video_out_dipswitch_stack_fail_fn, stack_fail_slot, "VideoOut dipsw __stack_chk_fail")
#undef IMPORT_READER

static uint64_t finish(const rt_video_out_dipswitch_bindings *imports, const borrowed_frame *frame,
                       uint64_t guard_pointer, uint32_t result)
{
    const uint64_t current = read_quad((const void *)(uintptr_t)guard_pointer);
    const uint64_t saved = read_quad(frame->guard_10);
    if (current != saved) {
        load_stack_fail(imports)();
#if defined(__x86_64__)
        __asm__ volatile("ud2" ::: "memory");
#else
        __builtin_trap();
#endif
        __builtin_unreachable();
    }
    return (uint64_t)result;
}

static uint32_t report_error(const rt_video_out_dipswitch_bindings *imports,
                             const char *operation, const char *operation_name)
{
    const void *error_cell = load_error(imports)();
    const uint32_t error = read_word(error_cell);
    const uint32_t converted = load_kernel_error(imports)(error);
    const char *format = require_cell(imports->diagnostic_format, "VideoOut dipsw diagnostic format");
    const char *label = require_cell(operation, operation_name);
    /* Both numeric conversions in the native format are unsigned hexadecimal. */
    load_printf(imports)(format, (unsigned int)diagnostic_value, label, (unsigned int)converted);
    return converted;
}

uint64_t rt_video_out_read_dipswitch(void)
{
    const rt_video_out_dipswitch_bindings *imports = rt_video_out_dipswitch_imports;
    if (!imports)
        rt_fatal("VideoOut DIP-switch read", "real module data and import cells have not been bound");
    const uint64_t guard_pointer = read_quad(require_cell(imports->guard_pointer_slot,
        "VideoOut dipsw __stack_chk_guard"));
    _Alignas(16) borrowed_frame frame;
    write_quad(frame.guard_10, read_quad((const void *)(uintptr_t)guard_pointer));
    const char *path = require_cell(imports->device_path, "VideoOut dipsw device path");
    const int32_t descriptor = load_open(imports)(path, 0u);
    if (descriptor < 0) {
        const uint32_t error = report_error(imports, imports->open_operation,
            "VideoOut dipsw open operation label");
        return finish(imports, &frame, guard_pointer, error);
    }

    write_word(frame.output_08, 0);
    const int32_t result = load_ioctl(imports)(descriptor, dipswitch_request, (void *)frame.output_08);
    uint32_t value;
    if (result < 0) {
        value = report_error(imports, imports->ioctl_operation,
            "VideoOut dipsw ioctl operation label");
        load_close(imports)(descriptor);
    } else {
        load_close(imports)(descriptor);
        /* Close can affect borrowed storage: reload the DWORD only afterward. */
        value = read_word(frame.output_08);
    }
    /* The entry guard pointer remains cached across all imports and diagnostics. */
    return finish(imports, &frame, guard_pointer, value);
}
