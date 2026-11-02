/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Driver descriptor publication has late failure paths with visible partial
   state. Preserve the real copy import, overlapping parameter snapshots and
   subsequent live source checks. Independent static reconstruction only;
   uncompiled/unvalidated, with real module/import bindings still required. */
#include "runtime/video_out_driver_publish.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut descriptor publication requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_driver_copy_fn) == sizeof(uint64_t), "copy pointer");

const rt_video_out_driver_publish_bindings *volatile rt_video_out_driver_publish_imports;

enum {
    operations_bytes = 264,
    parameter_chunk_bytes = 32,
    parameter_shift_bytes = 16,
    parameter_prefix_bytes = 48,
    descriptor_stride = 320,
    reserved_cell_count = 9
};
static const uint32_t vacant_class = UINT32_MAX;
static const uint32_t word_sign = UINT32_C(0x80000000);
static const uint64_t extended_word_sign = UINT64_C(0xffffffff00000000);

/* Known storage anchors only; callbacks and unknown cells stay opaque bytes. */
typedef struct {
    uint8_t required_00[8], required_08[8], required_10[8];
    uint8_t required_18[8], required_20[8], required_28[8];
    uint8_t unchecked_30[8], required_38[8], required_40[8];
    uint8_t unknown_48[0x78];
    uint8_t reserved_c0[reserved_cell_count][8];
} operations_anchors;
_Static_assert(offsetof(operations_anchors, required_00) == 0x00, "required operation");
_Static_assert(offsetof(operations_anchors, required_08) == 0x08, "required operation");
_Static_assert(offsetof(operations_anchors, required_10) == 0x10, "required operation");
_Static_assert(offsetof(operations_anchors, required_18) == 0x18, "required operation");
_Static_assert(offsetof(operations_anchors, required_20) == 0x20, "required operation");
_Static_assert(offsetof(operations_anchors, required_28) == 0x28, "required operation");
_Static_assert(offsetof(operations_anchors, unchecked_30) == 0x30, "unchecked operation");
_Static_assert(offsetof(operations_anchors, required_38) == 0x38, "required operation");
_Static_assert(offsetof(operations_anchors, required_40) == 0x40, "required operation");
_Static_assert(offsetof(operations_anchors, reserved_c0) == 0xc0, "reserved cells");
_Static_assert(sizeof(operations_anchors) == operations_bytes, "operations copy extent");

typedef struct {
    uint8_t class_00[sizeof(uint32_t)];
    uint8_t unknown_04[4];
    uint8_t parameters_08[parameter_prefix_bytes];
    uint8_t operations_38[operations_bytes];
} descriptor_anchors;
_Static_assert(offsetof(descriptor_anchors, class_00) == 0x00, "class offset");
_Static_assert(offsetof(descriptor_anchors, parameters_08) == 0x08, "parameter offset");
_Static_assert(offsetof(descriptor_anchors, operations_38) == 0x38, "operations offset");
_Static_assert(sizeof(descriptor_anchors) == descriptor_stride, "descriptor stride");

typedef struct { uint8_t bytes[parameter_chunk_bytes]; } parameter_chunk;

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

static parameter_chunk read_chunk(uint64_t address)
{
    parameter_chunk chunk;
    access_boundary();
    memcpy(&chunk, (const void *)(uintptr_t)address, sizeof chunk);
    __asm__ volatile("" : : "m"(chunk) : "memory");
    return chunk;
}

static void write_chunk(uint64_t address, const parameter_chunk *chunk)
{
    access_boundary();
    memcpy((void *)(uintptr_t)address, chunk, sizeof *chunk);
    access_boundary();
}

static uint64_t extend_word(uint32_t bits)
{
    return (bits & word_sign) ? extended_word_sign | (uint64_t)bits : (uint64_t)bits;
}

static const void *require_storage(const void *storage, const char *name)
{
    if (!storage) rt_fatal(name, "the required module storage or import cell is unbound");
    return storage;
}

static rt_video_out_driver_copy_fn load_copy(const rt_video_out_driver_publish_bindings *imports)
{
    rt_video_out_driver_copy_fn call;
    const void *cell = require_storage(imports->copy_slot, "VideoOut driver memcpy import");
    access_boundary();
    memcpy(&call, cell, sizeof call);
    access_boundary();
    if (!call) rt_fatal("VideoOut driver memcpy", "the function import target is unbound");
    return call;
}

uint64_t rt_video_out_publish_driver(int32_t class_index, const void *operations_storage,
    const void *parameter_storage, uint64_t reserved_wire)
{
    if (!operations_storage) return 0;
    if (reserved_wire != 0) return 0;
    const uint64_t operations = (uint64_t)(uintptr_t)operations_storage;
    static const size_t required[] = {
        offsetof(operations_anchors, required_00), offsetof(operations_anchors, required_08),
        offsetof(operations_anchors, required_10), offsetof(operations_anchors, required_18),
        offsetof(operations_anchors, required_20), offsetof(operations_anchors, required_28),
        offsetof(operations_anchors, required_38), offsetof(operations_anchors, required_40)
    };
    for (size_t i = 0; i < sizeof required / sizeof required[0]; ++i)
        if (read_quad(operations + required[i]) == 0) return 0;

    const rt_video_out_driver_publish_bindings *imports = rt_video_out_driver_publish_imports;
    if (!imports)
        rt_fatal("VideoOut driver publication", "real module data and import cells have not been bound");
    const uint64_t map = (uint64_t)(uintptr_t)require_storage(imports->class_map, "VideoOut driver class map");
    const uint32_t slot = read_word(map + extend_word((uint32_t)class_index) * sizeof(uint32_t));
    const uint64_t base = (uint64_t)(uintptr_t)require_storage(imports->descriptors, "VideoOut driver descriptors");
    const uint64_t candidate = base + extend_word(slot) * descriptor_stride;
    const uint64_t descriptor = (slot & word_sign) ? 0 : candidate;
    /* A negative slot reaches the zero-address read; no safe fallback is known. */
    if (read_word(descriptor + offsetof(descriptor_anchors, class_00)) != vacant_class) return 0;

    write_word(descriptor + offsetof(descriptor_anchors, class_00), (uint32_t)class_index);
    load_copy(imports)((void *)(uintptr_t)(descriptor + offsetof(descriptor_anchors, operations_38)),
        operations_storage, operations_bytes);
    if (parameter_storage) {
        const uint64_t parameters = (uint64_t)(uintptr_t)parameter_storage;
        const parameter_chunk low = read_chunk(parameters);
        const parameter_chunk high = read_chunk(parameters + parameter_shift_bytes);
        const uint64_t output = descriptor + offsetof(descriptor_anchors, parameters_08);
        write_chunk(output + parameter_shift_bytes, &high);
        write_chunk(output, &low);
    }

    /* These source reads deliberately follow publication, including alias effects. */
    for (size_t i = 0; i < reserved_cell_count; ++i)
        if (read_quad(operations + offsetof(operations_anchors, reserved_c0)
                + i * sizeof(uint64_t)) != 0) return 0;
    return descriptor;
}
