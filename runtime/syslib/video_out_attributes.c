/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Admit and publish VideoOut group attributes.

   This source reconstructs the complete 3153-byte internal helper at module
   offset 0x5fc0 in libSceVideoOut, SHA-256
   33a6f16349f07637e168db0d6338f0b2ae8aea626b86bc3d28325ecb5155e2c1.
   Literal format words and feature masks remain unnamed by GPU meaning. The
   native dimension tables are separate live module dependencies. Input reads
   retain their epochs, including rereads after alias-visible partial stores.

   Auxiliary and alignment writes are not rolled back on later errors. A special
   sizing path produces size 22/alignment 0 and can subsequently divide by zero;
   no convenient error/default replaces it. Other sizing uses wrapping 32-bit
   arithmetic, unsigned DIVL rounding and signed IDIVL final remainder. Negative
   computed sizes return their raw bits through a zero-extended 32-bit result.

   Publication performs separate live tail/32-byte snapshots in native order,
   preserving ordinary overlapping input/backend storage. The projected frame
   retains its overlapping stride/size slot, unaligned command tail, callback
   padding and saved guard. It is not a physical native-stack reconstruction.
   The optional callback requires a real adapter carrying the known GP/YMM0
   subset; no foreign callback is invoked through an invented C signature.

   Uncompiled/unvalidated flat 64-bit little-endian source progress. Real module and
   service initialization/footprints, unknown bytes, physical-frame aliases,
   ambient state, compiler lowering and concurrency remain unproved. On x86-64
   actual DIVQ retains zero-divisor faults; ARM64 explicitly traps and does not
   claim the same signal identity. No default service or SCE export is supplied. */
#include "runtime/video_out_attributes.h"
#include "runtime/video_out_group.h"
#include "runtime/video_out_post_attributes.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut group attributes require a little-endian host"
#endif
#if !defined(__x86_64__) && !defined(__aarch64__)
#error "VideoOut attribute division currently supports x86-64 and ARM64 only"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_attribute_mode_fn) == sizeof(uint64_t), "mode pointer");
_Static_assert(sizeof(rt_video_out_attribute_ioctl_fn) == sizeof(uint64_t), "ioctl pointer");
_Static_assert(sizeof(rt_video_out_attribute_error_fn) == sizeof(uint64_t), "error pointer");
_Static_assert(sizeof(rt_video_out_attribute_stack_fail_fn) == sizeof(uint64_t), "fail pointer");

const rt_video_out_attribute_bindings *volatile rt_video_out_attribute_imports;

enum {
    word_bytes = sizeof(uint32_t),
    quad_bytes = sizeof(uint64_t),
    attribute_prefix_bytes = 32,
    group_stride_bytes = 0xa8,
    pair_stride_bytes = 16,
    map_entries = 16,
    table_row_bytes = 12,
    dimension_rows = 18,
    mode_mask = 7,
    notification_event = 3,
};
static const uint32_t parameter_error = UINT32_C(0x80290001);
static const uint32_t address_error = UINT32_C(0x80290002);
static const uint32_t format_error = UINT32_C(0x80290003);
static const uint32_t pitch_error = UINT32_C(0x80290004);
static const uint32_t dimension_error = UINT32_C(0x80290005);
static const uint32_t selector_error = UINT32_C(0x80290007);
static const uint32_t reserved_error = UINT32_C(0x80290008);
static const uint32_t busy_error = UINT32_C(0x80290009);
static const uint32_t group_error = UINT32_C(0x8029000a);
static const uint32_t alignment_error = UINT32_C(0x80290015);
static const uint32_t flags_error = UINT32_C(0x8029001a);
static const uint32_t mapped_error_45 = UINT32_C(0x8029001c);
static const uint32_t generic_error = UINT32_C(0x802900ff);
static const uint32_t allocation_error = UINT32_C(0x8029100c);
static const uint32_t wide_format_threshold = UINT32_C(0xbfffffff);
static const uint64_t attribute_request = UINT64_C(0xc0308207);

typedef struct {
    uint8_t format_00[word_bytes];
    uint8_t selector_04[word_bytes];
    uint8_t reserved_08[word_bytes];
    uint8_t width_0c[word_bytes];
    uint8_t height_10[word_bytes];
    uint8_t pitch_14[word_bytes];
    uint8_t flags_18[word_bytes];
    uint8_t reserved_1c[word_bytes];
    uint8_t tail_20[quad_bytes];
} attribute_anchors;

typedef struct {
    uint8_t unknown_00[8];
    uint8_t group_map_08[word_bytes];
    uint8_t unknown_0c[0x3c];
    uint8_t first_48[quad_bytes];
    uint8_t second_50[quad_bytes];
    uint8_t unknown_58[0x3dc];
    uint8_t callback_word_434[word_bytes];
    uint8_t token_438[quad_bytes];
    uint8_t unknown_440[0x10];
    uint8_t feature_450[word_bytes];
    uint8_t unknown_454[0x10];
    uint8_t gate_464[word_bytes];
    uint8_t unknown_468[8];
    uint8_t gate_470[word_bytes];
    uint8_t gate_474[word_bytes];
    uint8_t gate_478[word_bytes];
    uint8_t gate_47c[word_bytes];
    uint8_t gate_480[word_bytes];
    uint8_t gate_484[word_bytes];
    uint8_t unknown_488[0x18];
    uint8_t gate_4a0[word_bytes];
    uint8_t unknown_4a4[4];
    uint8_t gate_4a8[word_bytes];
} backend_anchors;

typedef struct {
    uint8_t unknown_00[0x14c];
    uint8_t active_14c[word_bytes];
    uint8_t unknown_150[0x40];
    uint8_t current_190[attribute_prefix_bytes];
    uint8_t current_tail_1b0[quad_bytes];
    uint8_t secondary_1b8[attribute_prefix_bytes];
    uint8_t secondary_tail_1d8[quad_bytes];
    uint8_t size_1e0[word_bytes];
    uint8_t alignment_1e4[word_bytes];
    uint8_t auxiliary_1e8[word_bytes];
} group_anchors;

typedef struct {
    uint8_t first_00[word_bytes];
    uint8_t second_04[word_bytes];
    uint8_t tag_08[word_bytes];
} dimension_row;

typedef struct {
    uint8_t token_00[quad_bytes];
    uint8_t group_08;
    uint8_t update_09;
    uint8_t zero_0a[2];
    uint8_t format_0c[word_bytes];
    uint8_t selector_10[word_bytes];
    uint8_t pitch_14[word_bytes];
    uint8_t width_18[word_bytes];
    uint8_t height_1c[word_bytes];
    uint8_t zero_20[2];
    uint8_t flags_22[2];
    uint8_t tail_24[quad_bytes];
    uint8_t zero_2c[word_bytes];
} request_packet;

typedef struct {
    uint8_t zero_00[word_bytes];
    uint8_t backend_word_04[word_bytes];
    uint8_t group_08[word_bytes];
    uint8_t unwritten_0c[word_bytes];
    uint8_t attribute_10[attribute_prefix_bytes];
    uint8_t tail_30[quad_bytes];
} callback_record;

typedef struct {
    uint8_t unknown_00[8];
    uint8_t vertical_08[quad_bytes];
    uint8_t unknown_10[4];
    uint8_t update_14[word_bytes];
    uint8_t stride_or_size_18[quad_bytes];
    uint8_t unknown_20[4];
    uint8_t format_24[word_bytes];
    callback_record callback_28;
    request_packet packet_60;
    uint8_t guard_90[quad_bytes];
} borrowed_frame;

#define PIN(type, field, offset) _Static_assert(offsetof(type, field) == offset, #type "." #field)
PIN(attribute_anchors, format_00, 0);
PIN(attribute_anchors, selector_04, 4);
PIN(attribute_anchors, reserved_08, 8);
PIN(attribute_anchors, width_0c, 0xc);
PIN(attribute_anchors, height_10, 0x10);
PIN(attribute_anchors, pitch_14, 0x14);
PIN(attribute_anchors, flags_18, 0x18);
PIN(attribute_anchors, reserved_1c, 0x1c);
PIN(attribute_anchors, tail_20, 0x20);
PIN(backend_anchors, group_map_08, 8);
PIN(backend_anchors, first_48, 0x48);
PIN(backend_anchors, second_50, 0x50);
PIN(backend_anchors, callback_word_434, 0x434);
PIN(backend_anchors, token_438, 0x438);
PIN(backend_anchors, feature_450, 0x450);
PIN(backend_anchors, gate_464, 0x464);
PIN(backend_anchors, gate_470, 0x470);
PIN(backend_anchors, gate_474, 0x474);
PIN(backend_anchors, gate_478, 0x478);
PIN(backend_anchors, gate_47c, 0x47c);
PIN(backend_anchors, gate_480, 0x480);
PIN(backend_anchors, gate_484, 0x484);
PIN(backend_anchors, gate_4a0, 0x4a0);
PIN(backend_anchors, gate_4a8, 0x4a8);
PIN(group_anchors, active_14c, 0x14c);
PIN(group_anchors, current_190, 0x190);
PIN(group_anchors, current_tail_1b0, 0x1b0);
PIN(group_anchors, secondary_1b8, 0x1b8);
PIN(group_anchors, secondary_tail_1d8, 0x1d8);
PIN(group_anchors, size_1e0, 0x1e0);
PIN(group_anchors, alignment_1e4, 0x1e4);
PIN(group_anchors, auxiliary_1e8, 0x1e8);
PIN(dimension_row, first_00, 0);
PIN(dimension_row, second_04, 4);
PIN(dimension_row, tag_08, 8);
PIN(request_packet, token_00, 0);
PIN(request_packet, group_08, 8);
PIN(request_packet, update_09, 9);
PIN(request_packet, zero_0a, 0xa);
PIN(request_packet, format_0c, 0xc);
PIN(request_packet, selector_10, 0x10);
PIN(request_packet, pitch_14, 0x14);
PIN(request_packet, width_18, 0x18);
PIN(request_packet, height_1c, 0x1c);
PIN(request_packet, zero_20, 0x20);
PIN(request_packet, flags_22, 0x22);
PIN(request_packet, tail_24, 0x24);
PIN(request_packet, zero_2c, 0x2c);
PIN(callback_record, zero_00, 0);
PIN(callback_record, backend_word_04, 4);
PIN(callback_record, group_08, 8);
PIN(callback_record, unwritten_0c, 0xc);
PIN(callback_record, attribute_10, 0x10);
PIN(callback_record, tail_30, 0x30);
PIN(borrowed_frame, vertical_08, 8);
PIN(borrowed_frame, update_14, 0x14);
PIN(borrowed_frame, stride_or_size_18, 0x18);
PIN(borrowed_frame, format_24, 0x24);
PIN(borrowed_frame, callback_28, 0x28);
PIN(borrowed_frame, packet_60, 0x60);
PIN(borrowed_frame, guard_90, 0x90);
#undef PIN
_Static_assert(sizeof(attribute_anchors) == 40, "attribute size");
_Static_assert(sizeof(dimension_row) == table_row_bytes, "table row size");
_Static_assert(sizeof(request_packet) == 48, "request size");
_Static_assert(sizeof(callback_record) == 56, "callback size");
_Static_assert(sizeof(borrowed_frame) == 0x98, "projected frame size");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

/* Keep reached reads whose values are later discarded, including the final
   table sentinel, B470 on ungated formats and B464 on ungated selectors. The
   compiler must produce the loaded value even when C control flow ignores it.
   This does not claim MMIO, atomic access or exact instruction lowering. */
static void retain_loaded_value(uint64_t value)
{
    __asm__ volatile("" : : "r"(value) : "memory");
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
        retain_loaded_value((uint64_t)value); \
        return value; \
    }
STORAGE_READER(read_byte, uint8_t)
STORAGE_READER(read_word, uint32_t)
STORAGE_READER(read_quad, uint64_t)
#undef STORAGE_READER
#define STORAGE_WRITER(name, type) \
    static void name(void *storage, type value) \
    { \
        access_boundary(); \
        memcpy(storage, &value, sizeof value); \
        access_boundary(); \
    }
STORAGE_WRITER(write_byte, uint8_t)
STORAGE_WRITER(write_half, uint16_t)
STORAGE_WRITER(write_word, uint32_t)
STORAGE_WRITER(write_quad, uint64_t)
#undef STORAGE_WRITER

static void read_prefix(void *snapshot, const void *source)
{
    access_boundary();
    memcpy(snapshot, source, attribute_prefix_bytes);
    access_boundary();
}

static void write_prefix(void *destination, const void *snapshot)
{
    access_boundary();
    memcpy(destination, snapshot, attribute_prefix_bytes);
    access_boundary();
}

static int32_t signed_word(uint32_t bits)
{
    int32_t result;
    memcpy(&result, &bits, sizeof result);
    return result;
}

#define ATTRIBUTE_FIELD(base, field) ((const void *)(uintptr_t)((base) + offsetof(attribute_anchors, field)))
#define BACKEND_FIELD(base, field) ((void *)(uintptr_t)((base) + offsetof(backend_anchors, field)))
#define GROUP_FIELD(base, stride, field) ((void *)(uintptr_t)((base) + (stride) + offsetof(group_anchors, field)))
#define TABLE_FIELD(base, row, field) ((const void *)(uintptr_t)((base) + (uint64_t)(row) * table_row_bytes + offsetof(dimension_row, field)))

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_attribute_bindings *imports) \
    { \
        type call; \
        const void *cell = require_cell(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_mode, rt_video_out_attribute_mode_fn, mode_slot, "VideoOut sceKernelIsNeoMode")
IMPORT_READER(load_ioctl, rt_video_out_attribute_ioctl_fn, ioctl_slot, "VideoOut attribute ioctl")
IMPORT_READER(load_error, rt_video_out_attribute_error_fn, error_slot, "VideoOut attribute __error")
IMPORT_READER(load_stack_fail, rt_video_out_attribute_stack_fail_fn, stack_fail_slot, "VideoOut attribute __stack_chk_fail")
#undef IMPORT_READER

static uint64_t finish(const rt_video_out_attribute_bindings *imports, const borrowed_frame *frame,
                       uint64_t guard_pointer, uint32_t result)
{
    const uint64_t current = read_quad((const void *)(uintptr_t)guard_pointer);
    const uint64_t saved = read_quad(frame->guard_90);
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

static uint64_t unsigned_remainder(uint64_t dividend, uint64_t divisor)
{
#if defined(__x86_64__)
    uint64_t quotient = dividend;
    uint64_t remainder = 0;
    __asm__ volatile("divq %2" : "+a"(quotient), "+d"(remainder) : "r"(divisor) : "cc", "memory");
    return remainder;
#else
    if (divisor == 0) __builtin_trap();
    return dividend % divisor;
#endif
}

static int is_pair(uint32_t first, uint32_t second, uint32_t expected_first, uint32_t expected_second)
{
    return first == expected_first && second == expected_second;
}

static int adaptive_dimensions(uint32_t first, uint32_t second)
{
    uint32_t adjusted = first + 31u;
    if (signed_word(first) >= 0) adjusted = first;
    if ((adjusted & UINT32_C(0xffffffe0)) != first) return 0;
    if (first - 1280u > 639u) return 0;
    /* The preceding range admits only positive adjusted values, so this shift
       is also the native arithmetic shift's result on every reached path. */
    return ((adjusted >> 5) * 2u) * 9u == second;
}

static int feature_byte(uint64_t backend, uint8_t mask)
{
    return (read_byte(BACKEND_FIELD(backend, feature_450)) & mask) != 0;
}

static int extended_dimensions(const rt_video_out_attribute_bindings *imports,
                               uint64_t backend, uint32_t first, uint32_t second)
{
    const uint64_t table = (uint64_t)(uintptr_t)require_cell(imports->dimension_table_18, "VideoOut extended dimension table");
    uint32_t expected_first = 3840;
    for (uint32_t row = 0; row < dimension_rows; ++row) {
        if (expected_first == first && read_word(TABLE_FIELD(table, row, second_04)) == second) {
            const uint32_t tag = read_word(TABLE_FIELD(table, row, tag_08));
            if (tag == 0) return 1;
            if (tag == 4 && read_word(BACKEND_FIELD(backend, gate_480)) != 0) return 1;
            if ((tag == 4 || tag == 5) && feature_byte(backend, 0x20)) return 1;
            if (tag == 6 && (read_byte((const uint8_t *)BACKEND_FIELD(backend, feature_450) + 1) & 1u) != 0)
                return 1;
            return 0;
        }
        /* Native reads this even after the eighteenth mismatch, before testing
           loop exhaustion. The first row's first word is instead an immediate. */
        expected_first = read_word(TABLE_FIELD(table, row + 1u, first_00));
    }
    return 0;
}

static uint32_t admit_dimensions(const rt_video_out_attribute_bindings *imports,
                                 uint64_t backend, uint64_t stride, uint32_t mode,
                                 uint32_t first, uint32_t second)
{
    if (read_word(BACKEND_FIELD(backend, gate_484)) != 0) {
        void *auxiliary = GROUP_FIELD(backend, stride, auxiliary_1e8);
        write_word(auxiliary, UINT32_MAX);
        if (mode == mode_mask) {
            if (is_pair(first, second, 1920, 1080)) return 0;
        } else {
            if (extended_dimensions(imports, backend, first, second)) return 0;
            if (is_pair(first, second, 1920, 1080)) return 0;
            if (read_word(BACKEND_FIELD(backend, gate_480)) != 0 && adaptive_dimensions(first, second)) return 0;
            if (is_pair(first, second, 1280, 720)) {
                write_word(auxiliary, 2);
                return 0;
            }
        }
        write_word(auxiliary, 0);
        return dimension_error;
    }
    if (mode == mode_mask)
        return is_pair(first, second, 1920, 1080) ? 0 : dimension_error;
    if (read_word(BACKEND_FIELD(backend, gate_474)) != 0) {
        if (read_word(BACKEND_FIELD(backend, gate_47c)) == 0) {
            uint32_t row;
            if (is_pair(first, second, 3840, 2160)) row = 0;
            else if (is_pair(first, second, 1920, 1080)) row = 1;
            else if (is_pair(first, second, 1280, 720)) row = 2;
            else goto gated_adaptive;
            const uint64_t table = (uint64_t)(uintptr_t)require_cell(imports->dimension_table_3, "VideoOut three-row dimension table");
            const uint32_t tag = read_word(TABLE_FIELD(table, row, tag_08));
            if (tag == 0) return 0;
            if (tag == 2 || tag == 3) {
                if (read_word(BACKEND_FIELD(backend, gate_478)) != 0) return 0;
                if (tag == 2 && feature_byte(backend, 1)) return 0;
                return dimension_error;
            }
            goto gated_adaptive;
        }
    } else if (read_word(BACKEND_FIELD(backend, callback_word_434)) == 1) {
        if (is_pair(first, second, 3840, 2160)) return 0;
        return is_pair(first, second, 1920, 1080) ? 0 : dimension_error;
    }
    {
        uint32_t row;
        if (is_pair(first, second, 1920, 1080)) row = 0;
        else if (is_pair(first, second, 3840, 2160)) row = 1;
        else if (is_pair(first, second, 720, 480)) row = 2;
        else if (is_pair(first, second, 720, 576)) row = 3;
        else return adaptive_dimensions(first, second) ? 0 : dimension_error;
        const uint64_t table = (uint64_t)(uintptr_t)require_cell(imports->dimension_table_4, "VideoOut four-row dimension table");
        const uint32_t tag = read_word(TABLE_FIELD(table, row, tag_08));
        if (tag == 0) return 0;
        if (tag == 1 || tag == 2) {
            if (read_word(BACKEND_FIELD(backend, gate_464)) != 0) return 0;
            if (tag == 2 && feature_byte(backend, 1)) return 0;
        }
        return dimension_error;
    }
gated_adaptive:
    if (read_word(BACKEND_FIELD(backend, gate_480)) == 0) return dimension_error;
    return adaptive_dimensions(first, second) ? 0 : dimension_error;
}

static uint32_t admit_format(uint64_t backend, borrowed_frame *frame, uint32_t format,
                             uint32_t features, uint32_t mode)
{
    uint32_t captured_gate = 0;
    if (signed_word(format) <= signed_word(UINT32_C(0x8881ffff)))
        captured_gate = read_word(BACKEND_FIELD(backend, gate_470));
    uint32_t required = 0;
    int restricted = 0;
    switch (format) {
    case UINT32_C(0x80000000): case UINT32_C(0x80002200):
    case UINT32_C(0x88000000): case UINT32_C(0x88060000):
    case UINT32_C(0xc1060000):
        break;
    case UINT32_C(0x80060000):
        write_word(frame->format_24, format);
        return (features & UINT32_C(0x800)) != 0 ? 0 : format_error;
    case UINT32_C(0x80220000): case UINT32_C(0x80320000):
    case UINT32_C(0x80420000): case UINT32_C(0x80520000):
        write_word(frame->format_24, format);
        return (((features >> 1) & 1u) | captured_gate) != 0 ? 0 : format_error;
    case UINT32_C(0xb0000000): case UINT32_C(0xb0060000):
        write_word(frame->format_24, format);
        return (features & UINT32_C(0x810)) == UINT32_C(0x810) ? 0 : format_error;
    case UINT32_C(0x88720000): case UINT32_C(0x88740000):
        required = UINT32_C(0x8); restricted = 1; break;
    case UINT32_C(0x88750000): case UINT32_C(0xc1760000):
        required = UINT32_C(0x808); restricted = 1; break;
    case UINT32_C(0x88820000): case UINT32_C(0x88840000):
        required = UINT32_C(0xa); restricted = 1; break;
    case UINT32_C(0x88850000):
        required = UINT32_C(0x80a); restricted = 1; break;
    case UINT32_C(0xb0720000): case UINT32_C(0xb0740000): case UINT32_C(0xb0750000):
        required = UINT32_C(0x818); restricted = 1; break;
    case UINT32_C(0xb0820000): case UINT32_C(0xb0840000): case UINT32_C(0xb0850000):
        required = UINT32_C(0x81a); restricted = 1; break;
    default:
        return format_error;
    }
    if ((features & required) != required) return format_error;
    write_word(frame->format_24, format);
    if (restricted && mode == mode_mask) {
        const uint32_t nibble = (format >> 20) & UINT32_C(0xf);
        if (nibble - 7u < 2u) return format_error;
    }
    return 0;
}

static uint32_t ioctl_error_result(uint32_t error)
{
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

uint64_t rt_video_out_apply_group_attributes(void *backend_storage, uint32_t group,
    const void *attribute_storage, uint32_t update)
{
    const rt_video_out_attribute_bindings *imports = rt_video_out_attribute_imports;
    if (!imports)
        rt_fatal("VideoOut group attributes", "real module data and import cells have not been bound");
    const uint64_t guard_pointer = read_quad(require_cell(imports->guard_pointer_slot, "VideoOut attribute __stack_chk_guard"));
    _Alignas(16) borrowed_frame frame;
    write_quad(frame.guard_90, read_quad((const void *)(uintptr_t)guard_pointer));
    if (!attribute_storage) return finish(imports, &frame, guard_pointer, address_error);
    const uint64_t attribute = (uint64_t)(uintptr_t)attribute_storage;
    if (read_word(ATTRIBUTE_FIELD(attribute, reserved_1c)) != 0)
        return finish(imports, &frame, guard_pointer, parameter_error);
    if (read_quad(ATTRIBUTE_FIELD(attribute, tail_20)) != 0)
        return finish(imports, &frame, guard_pointer, parameter_error);
    if (group > 3u) return finish(imports, &frame, guard_pointer, group_error);
    const uint64_t backend = (uint64_t)(uintptr_t)backend_storage;
    write_word(frame.update_14, update);
    uint64_t stride = (uint64_t)group * group_stride_bytes;
    const uint32_t active = read_word(GROUP_FIELD(backend, stride, active_14c));
    if (update != 0) {
        if (active == 0) return finish(imports, &frame, guard_pointer, group_error);
        if ((uint32_t)rt_video_out_group_has_pair(backend_storage, (int32_t)group) != 0)
            return finish(imports, &frame, guard_pointer, busy_error);
    } else if (active == 1) {
        return finish(imports, &frame, guard_pointer, busy_error);
    }
    const uint32_t initial_flags = read_word(ATTRIBUTE_FIELD(attribute, flags_18));
    const uint32_t initial_width = read_word(ATTRIBUTE_FIELD(attribute, width_0c));
    const uint32_t initial_height = read_word(ATTRIBUTE_FIELD(attribute, height_10));
    const uint32_t mode = initial_flags & mode_mask;
    uint32_t error = admit_dimensions(imports, backend, stride, mode, initial_width, initial_height);
    if (error != 0) return finish(imports, &frame, guard_pointer, error);
    const uint32_t format = read_word(ATTRIBUTE_FIELD(attribute, format_00));
    const uint32_t features = read_word(BACKEND_FIELD(backend, feature_450));
    error = admit_format(backend, &frame, format, features, mode);
    if (error != 0) return finish(imports, &frame, guard_pointer, error);
    const uint32_t requested_selector = read_word(ATTRIBUTE_FIELD(attribute, selector_04));
    if (requested_selector > 3u) return finish(imports, &frame, guard_pointer, selector_error);
    const uint32_t selector_gate = read_word(BACKEND_FIELD(backend, gate_464));
    uint32_t selector;
    if (requested_selector == 0) {
        const uint32_t native_mode = load_mode(imports)();
        selector = 2u - (native_mode < 1u);
    } else if (requested_selector == 1) {
        selector = 0;
    } else {
        selector = requested_selector == 2 ? 2 : 1;
        if (selector_gate == 0) return finish(imports, &frame, guard_pointer, selector_error);
    }
    if (read_word(ATTRIBUTE_FIELD(attribute, reserved_08)) != 0)
        return finish(imports, &frame, guard_pointer, reserved_error);
    const uint32_t width = read_word(ATTRIBUTE_FIELD(attribute, width_0c));
    const uint32_t pitch = read_word(ATTRIBUTE_FIELD(attribute, pitch_14));
    if (pitch < width) return finish(imports, &frame, guard_pointer, pitch_error);
    uint32_t sizing_format;
    if (selector == 0) {
        if ((pitch & UINT32_C(0x3f)) != 0)
            return finish(imports, &frame, guard_pointer, pitch_error);
        if (read_word(BACKEND_FIELD(backend, gate_480)) == 0) {
            if (width > 1920u) return finish(imports, &frame, guard_pointer, parameter_error);
            sizing_format = read_word(ATTRIBUTE_FIELD(attribute, format_00));
            if ((sizing_format & UINT32_C(0xc0000000)) != UINT32_C(0x80000000))
                return finish(imports, &frame, guard_pointer, parameter_error);
        } else {
            sizing_format = read_word(ATTRIBUTE_FIELD(attribute, format_00));
        }
    } else if (selector == 1 && width >= 1921u && read_word(BACKEND_FIELD(backend, gate_480)) == 0) {
        sizing_format = read_word(ATTRIBUTE_FIELD(attribute, format_00));
        if ((sizing_format & UINT32_C(0xc0000000)) != UINT32_C(0x80000000))
            return finish(imports, &frame, guard_pointer, parameter_error);
    } else {
        sizing_format = read_word(ATTRIBUTE_FIELD(attribute, format_00));
    }
    const uint32_t factor = 4u + 4u * (sizing_format > wide_format_threshold);
    uint32_t alignment;
    if (read_word(BACKEND_FIELD(backend, gate_4a8)) == 0) {
        uint32_t size = factor * pitch;
        size *= read_word(ATTRIBUTE_FIELD(attribute, height_10));
        alignment = 1;
        write_word(frame.stride_or_size_18, size);
        if (signed_word(size) < 0) return finish(imports, &frame, guard_pointer, size);
    } else if (selector == 2 && sizing_format > wide_format_threshold) {
        /* This literal size bypasses rounding and its sign test. Alignment 0
           may subsequently reach DIVQ on a matching address-map entry. */
        alignment = 0;
        write_word(frame.stride_or_size_18, 22);
    } else {
        uint32_t horizontal;
        uint32_t vertical;
        if (selector == 0) {
            horizontal = 64; vertical = 1; alignment = 256;
        } else if (selector == 1) {
            horizontal = 128; vertical = 64;
            alignment = 32768u + 32768u * (sizing_format > wide_format_threshold);
        } else {
            horizontal = 128; vertical = 128; alignment = 65536;
        }
        write_quad(frame.stride_or_size_18, stride);
        write_quad(frame.vertical_08, vertical);
        uint32_t rounded_pitch = pitch + horizontal - 1u;
        const uint32_t horizontal_remainder = rounded_pitch % horizontal;
        stride = read_quad(frame.stride_or_size_18);
        rounded_pitch -= horizontal_remainder;
        if (rounded_pitch != pitch) return finish(imports, &frame, guard_pointer, pitch_error);
        const uint32_t height = read_word(ATTRIBUTE_FIELD(attribute, height_10));
        const uint32_t vertical_divisor = (uint32_t)read_quad(frame.vertical_08);
        uint32_t total = factor * pitch;
        uint32_t rounded_height = height + vertical_divisor - 1u;
        rounded_height -= rounded_height % vertical_divisor;
        total *= rounded_height;
        const uint32_t dividend = total + alignment - 1u;
        /* IDIVL uses the signed wrapped numerator; Q here is a positive fixed
           divisor. Unsigned remainder would change negative intermediate bits. */
        const int32_t remainder = signed_word(dividend) % (int32_t)alignment;
        const uint32_t size = dividend - (uint32_t)remainder;
        write_word(frame.stride_or_size_18, size);
        if (signed_word(size) < 0) return finish(imports, &frame, guard_pointer, size);
    }
    if (read_word(frame.update_14) != 0) {
        const int32_t size = signed_word(read_word(frame.stride_or_size_18));
        if (signed_word(read_word(GROUP_FIELD(backend, stride, size_1e0))) < size)
            return finish(imports, &frame, guard_pointer, parameter_error);
        if (signed_word(read_word(GROUP_FIELD(backend, stride, alignment_1e4))) < (int32_t)alignment) {
            for (uint32_t row = 0; row < map_entries; ++row) {
                const uint64_t map = backend + offsetof(backend_anchors, group_map_08) + (uint64_t)row * word_bytes;
                if (read_word((const void *)(uintptr_t)map) != group) continue;
                const uint64_t first_address = backend + offsetof(backend_anchors, first_48) + (uint64_t)row * pair_stride_bytes;
                if (unsigned_remainder(read_quad((const void *)(uintptr_t)first_address), alignment) != 0)
                    return finish(imports, &frame, guard_pointer, alignment_error);
                const uint64_t second_address = backend + offsetof(backend_anchors, second_50) + (uint64_t)row * pair_stride_bytes;
                if (unsigned_remainder(read_quad((const void *)(uintptr_t)second_address), alignment) != 0)
                    return finish(imports, &frame, guard_pointer, alignment_error);
            }
        }
    }
    write_word(GROUP_FIELD(backend, stride, alignment_1e4), alignment);
    uint32_t working_flags = read_word(ATTRIBUTE_FIELD(attribute, flags_18));
    uint32_t wire_flags = 0;
    if ((working_flags & mode_mask) == mode_mask) {
        if ((working_flags & UINT32_C(0x10)) != 0 && read_word(BACKEND_FIELD(backend, gate_4a0)) != 0)
            return finish(imports, &frame, guard_pointer, flags_error);
        working_flags &= ~UINT32_C(7);
        wire_flags = 7;
    }
    wire_flags |= working_flags & UINT32_C(8);
    uint32_t residual = working_flags & ~UINT32_C(8);
    if ((working_flags & UINT32_C(0x10)) != 0 && feature_byte(backend, 0x40)) {
        residual = working_flags & ~UINT32_C(0x18);
        wire_flags |= UINT32_C(0x10);
    }
    if (residual != 0) return finish(imports, &frame, guard_pointer, flags_error);
    const uint64_t token = read_quad(BACKEND_FIELD(backend, token_438));
    const uint8_t update_bit = read_word(frame.update_14) != 0;
    const int32_t descriptor = signed_word(read_word(require_cell(imports->descriptor_word, "VideoOut attribute descriptor")));
    const uint64_t published_stride = stride;
    write_quad(frame.packet_60.token_00, token);
    const uint32_t selected_format = read_word(frame.format_24);
    write_byte(&frame.packet_60.group_08, (uint8_t)group);
    write_byte(&frame.packet_60.update_09, update_bit);
    write_half(frame.packet_60.zero_0a, 0);
    write_word(frame.packet_60.format_0c, selected_format);
    write_word(frame.packet_60.selector_10, selector);
    write_word(frame.packet_60.pitch_14, pitch);
    write_word(frame.packet_60.width_18, width);
    const uint32_t packet_height = read_word(ATTRIBUTE_FIELD(attribute, height_10));
    write_word(frame.packet_60.height_1c, packet_height);
    write_half(frame.packet_60.zero_20, 0);
    write_half(frame.packet_60.flags_22, (uint16_t)wire_flags);
    write_quad(frame.packet_60.tail_24, UINT64_MAX);
    write_word(frame.packet_60.zero_2c, 0);
    const int32_t applied = load_ioctl(imports)(descriptor, attribute_request, (void *)&frame.packet_60);
    if (applied < 0) {
        const void *error_cell = load_error(imports)();
        const uint32_t native_error = read_word(error_cell);
        return finish(imports, &frame, guard_pointer, ioctl_error_result(native_error));
    }
    uint8_t snapshot[attribute_prefix_bytes];
    if (read_word(frame.update_14) == 1) {
        const uint64_t tail = read_quad(ATTRIBUTE_FIELD(attribute, tail_20));
        write_quad(GROUP_FIELD(backend, published_stride, current_tail_1b0), tail);
        read_prefix(snapshot, (const void *)(uintptr_t)attribute);
        write_prefix(GROUP_FIELD(backend, published_stride, current_190), snapshot);
    } else {
        const uint64_t tail = read_quad(ATTRIBUTE_FIELD(attribute, tail_20));
        const uint32_t size = read_word(frame.stride_or_size_18);
        write_quad(GROUP_FIELD(backend, published_stride, secondary_tail_1d8), tail);
        read_prefix(snapshot, (const void *)(uintptr_t)attribute);
        write_prefix(GROUP_FIELD(backend, published_stride, secondary_1b8), snapshot);
        read_prefix(snapshot, (const void *)(uintptr_t)attribute);
        write_prefix(GROUP_FIELD(backend, published_stride, current_190), snapshot);
        const uint64_t current_tail = read_quad(ATTRIBUTE_FIELD(attribute, tail_20));
        write_quad(GROUP_FIELD(backend, published_stride, current_tail_1b0), current_tail);
        write_word(GROUP_FIELD(backend, published_stride, size_1e0), size);
        write_word(GROUP_FIELD(backend, published_stride, active_14c), 1);
    }
    (void)rt_video_out_post_attributes(backend_storage);
    const uint64_t callback = read_quad(require_cell(imports->callback_target_slot, "VideoOut attribute callback target"));
    if (callback != 0) {
        write_word(frame.callback_28.zero_00, 0);
        const uint64_t argument = read_quad(require_cell(imports->callback_argument_slot, "VideoOut attribute callback argument"));
        const uint32_t backend_word = read_word(BACKEND_FIELD(backend, callback_word_434));
        write_word(frame.callback_28.backend_word_04, backend_word);
        write_word(frame.callback_28.group_08, group);
        read_prefix(snapshot, (const void *)(uintptr_t)attribute);
        write_prefix(frame.callback_28.attribute_10, snapshot);
        const uint64_t tail = read_quad(ATTRIBUTE_FIELD(attribute, tail_20));
        write_quad(frame.callback_28.tail_30, tail);
        if (!imports->callback_bridge)
            rt_fatal("VideoOut attribute callback bridge", "the actual opaque callback ABI has not been bound");
        imports->callback_bridge(callback, notification_event, (void *)&frame.callback_28,
                                 argument, callback, tail, (const void *)snapshot);
    }
    /* This function never refreshes its entry-captured guard pointer, including
       after the post helper and callback have changed module storage. */
    return finish(imports, &frame, guard_pointer, 0);
}
