/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VideoOut's registration rollback removes an index only after the driver
   accepts command 4. Preserve its partial-state order and errno-zero early
   return: neither implies that the cached address pair has been cleared.
   Uncompiled/unvalidated source profile from the supplied original. Projected
   borrowed storage preserves reached field widths/order, not arbitrary physical
   stack aliases, provider footprints, concurrency or exact host fault identity. */
#include "runtime/video_out_remove_address.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut address removal requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_remove_ioctl_fn) == sizeof(uint64_t), "ioctl pointer");
_Static_assert(sizeof(rt_video_out_remove_error_fn) == sizeof(uint64_t), "error pointer");
_Static_assert(sizeof(rt_video_out_remove_stack_fail_fn) == sizeof(uint64_t), "fail pointer");

const rt_video_out_remove_bindings *volatile rt_video_out_remove_imports;

enum {
    word_bytes = sizeof(uint32_t),
    quad_bytes = sizeof(uint64_t),
    group_stride_bytes = 0xa8,
    remove_command = 4,
    remove_event = 2,
    clear_span_bytes = 32,
};
static const uint64_t remove_request = UINT64_C(0xc0308203);
static const uint32_t unmapped_index = UINT32_C(0xffffffff);
static const uint32_t parameter_error = UINT32_C(0x80290001);
static const uint32_t address_error = UINT32_C(0x80290002);
static const uint32_t busy_error = UINT32_C(0x80290009);
static const uint32_t mapped_error_45 = UINT32_C(0x8029001c);
static const uint32_t generic_error = UINT32_C(0x802900ff);
static const uint32_t allocation_error = UINT32_C(0x8029100c);

/* Offset anchors describe reached accesses, not complete objects or arrays. */
typedef struct {
    uint8_t unknown_00[8];
    uint8_t index_group_08[word_bytes];
    uint8_t unknown_0c[0x3dc];
    uint8_t index_busy_3e8[word_bytes];
    uint8_t unknown_3ec[0x48];
    uint8_t callback_word_434[word_bytes];
    uint8_t token_438[quad_bytes];
} backend_anchors;

typedef struct {
    uint8_t unknown_00[0x150];
    uint8_t index_active_150[word_bytes];
} group_anchors;

typedef struct {
    uint8_t command_00[word_bytes];
    uint8_t zero_04[word_bytes];
    uint8_t token_08[quad_bytes];
    uint8_t index_10[quad_bytes];
    uint8_t zero_18[24];
} remove_packet;

typedef struct {
    uint8_t zero_00[word_bytes];
    uint8_t backend_word_04[word_bytes];
    uint8_t index_08[word_bytes];
    uint8_t unwritten_0c[word_bytes];
} callback_record;

typedef struct {
    uint8_t unknown_00[16];
    callback_record callback_10;
    remove_packet packet_20;
    uint8_t unknown_50[16];
    uint8_t guard_60[quad_bytes];
    uint8_t unknown_68[24];
} borrowed_frame;

#define PIN(type, field, offset) _Static_assert(offsetof(type, field) == offset, #type "." #field)
PIN(backend_anchors, index_group_08, 8);
PIN(backend_anchors, index_busy_3e8, 0x3e8);
PIN(backend_anchors, callback_word_434, 0x434);
PIN(backend_anchors, token_438, 0x438);
PIN(group_anchors, index_active_150, 0x150);
PIN(remove_packet, command_00, 0);
PIN(remove_packet, zero_04, 4);
PIN(remove_packet, token_08, 8);
PIN(remove_packet, index_10, 0x10);
PIN(remove_packet, zero_18, 0x18);
PIN(callback_record, zero_00, 0);
PIN(callback_record, backend_word_04, 4);
PIN(callback_record, index_08, 8);
PIN(callback_record, unwritten_0c, 0xc);
PIN(borrowed_frame, callback_10, 0x10);
PIN(borrowed_frame, packet_20, 0x20);
PIN(borrowed_frame, unknown_50, 0x50);
PIN(borrowed_frame, guard_60, 0x60);
PIN(borrowed_frame, unknown_68, 0x68);
#undef PIN
_Static_assert(sizeof(remove_packet) == 48, "remove packet size");
_Static_assert(sizeof(callback_record) == 16, "callback area size");
_Static_assert(sizeof(borrowed_frame) == 0x80, "projected frame size");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static const void *require_cell(const void *cell, const char *name)
{
    if (!cell) rt_fatal(name, "the required module storage or import cell is unbound");
    return cell;
}

/* Keep reached reads explicit even if a later caller discards their values.
   This is a source-level constraint, not proof of emitted access/fault widths. */
static void retain_loaded_value(uint64_t value)
{
    __asm__ volatile("" : : "r"(value) : "memory");
}

#define STORAGE_READER(name, type) \
    static type name(const void *storage) \
    { \
        type value; \
        access_boundary(); \
        memcpy(&value, storage, sizeof value); \
        retain_loaded_value((uint64_t)value); \
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

static int32_t signed_word(uint32_t value)
{
    int32_t result;
    memcpy(&result, &value, sizeof result);
    return result;
}

#define BACKEND_INDEX_FIELD(base, index, field) \
    ((void *)(uintptr_t)((base) + (index) * word_bytes + offsetof(backend_anchors, field)))
#define BACKEND_FIELD(base, field) \
    ((void *)(uintptr_t)((base) + offsetof(backend_anchors, field)))
#define GROUP_INDEX_FIELD(base, stride, index, field) \
    ((void *)(uintptr_t)((base) + (stride) + (index) * word_bytes + offsetof(group_anchors, field)))

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_remove_bindings *imports) \
    { \
        type call; \
        const void *cell = require_cell(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_ioctl, rt_video_out_remove_ioctl_fn, ioctl_slot, "VideoOut remove ioctl")
IMPORT_READER(load_error, rt_video_out_remove_error_fn, error_slot, "VideoOut remove __error")
IMPORT_READER(load_stack_fail, rt_video_out_remove_stack_fail_fn, stack_fail_slot, "VideoOut remove __stack_chk_fail")
#undef IMPORT_READER

static uint64_t finish(const rt_video_out_remove_bindings *imports, const borrowed_frame *frame,
                       uint64_t guard_pointer, uint32_t result)
{
    const uint64_t current = read_quad((const void *)(uintptr_t)guard_pointer);
    const uint64_t saved = read_quad(frame->guard_60);
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

static uint32_t ioctl_error_result(uint32_t error)
{
    /* The original's 46-cell table sends errno zero straight to the return. */
    switch (error) {
    case 0: return 0;
    case 1: case 22: return parameter_error;
    case 12: return allocation_error;
    case 14: return address_error;
    case 16: return busy_error;
    case 45: return mapped_error_45;
    default: return generic_error;
    }
}

static void clear_packet(remove_packet *packet)
{
    /* Two ordered, overlapping native stores cover 48 bytes, not 64. */
    access_boundary();
    memset(packet, 0, clear_span_bytes);
    access_boundary();
    memset((uint8_t *)packet + offsetof(remove_packet, index_10), 0, clear_span_bytes);
    access_boundary();
}

uint64_t rt_video_out_remove_address(void *backend_storage, int32_t index)
{
    const rt_video_out_remove_bindings *imports = rt_video_out_remove_imports;
    if (!imports)
        rt_fatal("VideoOut address removal", "real module data and import cells have not been bound");
    const uint64_t backend = (uint64_t)(uintptr_t)backend_storage;
    const uint64_t signed_index = (uint64_t)(int64_t)index;
    const uint64_t guard_pointer = read_quad(require_cell(imports->guard_pointer_slot,
        "VideoOut remove __stack_chk_guard"));
    _Alignas(32) borrowed_frame frame;
    write_quad(frame.guard_60, read_quad((const void *)(uintptr_t)guard_pointer));
    if (read_word(BACKEND_INDEX_FIELD(backend, signed_index, index_busy_3e8)) != 0)
        return finish(imports, &frame, guard_pointer, busy_error);

    const uint64_t token = read_quad(BACKEND_FIELD(backend, token_438));
    const int32_t descriptor = signed_word(read_word(require_cell(imports->descriptor_word,
        "VideoOut remove descriptor")));
    clear_packet(&frame.packet_20);
    write_word(frame.packet_20.command_00, remove_command);
    write_quad(frame.packet_20.token_08, token);
    write_quad(frame.packet_20.index_10, signed_index);
    const int32_t removed = load_ioctl(imports)(descriptor, remove_request, (void *)&frame.packet_20);
    if (removed < 0) {
        const void *error_cell = load_error(imports)();
        const uint32_t error = read_word(error_cell);
        return finish(imports, &frame, guard_pointer, ioctl_error_result(error));
    }

    /* The group is read after ioctl, before overwriting the live mapping. */
    void *mapping = BACKEND_INDEX_FIELD(backend, signed_index, index_group_08);
    const int64_t group = (int64_t)signed_word(read_word(mapping));
    write_word(mapping, unmapped_index);
    const uint64_t stride = (uint64_t)(group * group_stride_bytes);
    write_word(GROUP_INDEX_FIELD(backend, stride, signed_index, index_active_150), 0);
    const uint64_t target = read_quad(require_cell(imports->callback_target_slot,
        "VideoOut remove callback target"));
    if (target != 0) {
        write_word(frame.callback_10.zero_00, 0);
        const uint64_t argument = read_quad(require_cell(imports->callback_argument_slot,
            "VideoOut remove callback argument"));
        const uint32_t callback_word = read_word(BACKEND_FIELD(backend, callback_word_434));
        write_word(frame.callback_10.backend_word_04, callback_word);
        write_word(frame.callback_10.index_08, (uint32_t)index);
        if (!imports->callback_bridge)
            rt_fatal("VideoOut remove callback bridge", "the actual opaque callback ABI has not been bound");
        imports->callback_bridge(target, remove_event, (void *)&frame.callback_10,
                                 argument, target, (uint64_t)callback_word);
    }
    /* This helper keeps its entry guard pointer even across ioctl/callback. */
    return finish(imports, &frame, guard_pointer, 0);
}
