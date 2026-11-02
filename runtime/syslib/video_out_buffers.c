/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VideoOut's internal buffer-registration caller joins attribute setup, per-index
   registration and partial cleanup. Native admission clears group words before
   range checks; a nonnegative lower result need not mean it published state.
   Preserve the selected group versus its later ID, live frame/counter reads and
   guard-pointer epochs. Uncompiled/unvalidated source profile from independent
   static analysis; no complete native frame, ambient-state or fault equivalence. */
#include "runtime/video_out_buffers.h"
#include "runtime/video_out_addresses.h"
#include "runtime/video_out_attributes.h"
#include "runtime/video_out_dipswitch.h"
#include "runtime/video_out_remove_address.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut buffer registration requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(int) == sizeof(uint32_t), "32-bit printf int");
_Static_assert(sizeof(rt_video_out_buffer_ioctl_fn) == sizeof(uint64_t), "ioctl pointer");
_Static_assert(sizeof(rt_video_out_buffer_error_fn) == sizeof(uint64_t), "error pointer");
_Static_assert(sizeof(rt_video_out_buffer_getpid_fn) == sizeof(uint64_t), "getpid pointer");
_Static_assert(sizeof(rt_video_out_buffer_printf_fn) == sizeof(uint64_t), "printf pointer");
_Static_assert(sizeof(rt_video_out_buffer_puts_fn) == sizeof(uint64_t), "puts pointer");
_Static_assert(sizeof(rt_video_out_buffer_stack_fail_fn) == sizeof(uint64_t), "fail pointer");

const rt_video_out_buffer_bindings *volatile rt_video_out_buffer_imports;

enum {
    word_bytes = sizeof(uint32_t),
    quad_bytes = sizeof(uint64_t),
    group_count = 4,
    group_stride_bytes = 0xa8,
    buffer_limit = 16,
    largest_count_minus_one = 15,
    clear_span_bytes = 32,
    cleanup_command = 5,
    cleanup_event = 4,
    warning_limit = 128,
};
static const uint64_t cleanup_request = UINT64_C(0xc0308203);
static const uint32_t unmapped_index = UINT32_C(0xffffffff);
static const uint32_t parameter_error = UINT32_C(0x80290001);
static const uint32_t no_free_group_error = UINT32_C(0x8029000f);
static const uint32_t duplicate_index_error = UINT32_C(0x80290010);

/* These are offset anchors, not whole foreign objects or bounded arrays. */
typedef struct {
    uint8_t unknown_00[8];
    uint8_t index_group_08[word_bytes];
    uint8_t unknown_0c[0x428];
    uint8_t callback_word_434[word_bytes];
    uint8_t token_438[quad_bytes];
    uint8_t unknown_440[0x18];
    uint8_t walk_count_458[word_bytes];
    uint8_t warning_count_45c[word_bytes];
    uint8_t unknown_460[0x78];
    uint8_t skip_prior_removals_4d8[word_bytes];
} backend_anchors;

typedef struct {
    uint8_t unknown_00[0x148];
    uint8_t id_148[word_bytes];
    uint8_t active_14c[word_bytes];
    uint8_t index_words_150[clear_span_bytes];
    uint8_t index_words_170[clear_span_bytes];
} group_anchors;

typedef struct {
    uint8_t command_00[word_bytes];
    uint8_t zero_04[word_bytes];
    uint8_t token_08[quad_bytes];
    uint8_t group_10[quad_bytes];
    uint8_t zero_18[24];
} cleanup_packet;

typedef struct {
    uint8_t zero_00[word_bytes];
    uint8_t backend_word_04[word_bytes];
    uint8_t group_08[word_bytes];
    uint8_t unwritten_0c[word_bytes];
} callback_record;

typedef struct {
    uint8_t unknown_00[16];
    uint8_t group_10[word_bytes];
    uint8_t mode_14[word_bytes];
    uint8_t initial_index_18[quad_bytes];
    uint8_t addresses_20[quad_bytes];
    uint8_t cleanup_group_28[quad_bytes];
    callback_record callback_30;
    cleanup_packet packet_40;
    uint8_t unknown_70[16];
    uint8_t guard_80[quad_bytes];
    uint8_t unknown_88[24];
} borrowed_frame;

#define PIN(type, field, offset) _Static_assert(offsetof(type, field) == offset, #type "." #field)
PIN(backend_anchors, index_group_08, 8);
PIN(backend_anchors, callback_word_434, 0x434);
PIN(backend_anchors, token_438, 0x438);
PIN(backend_anchors, walk_count_458, 0x458);
PIN(backend_anchors, warning_count_45c, 0x45c);
PIN(backend_anchors, skip_prior_removals_4d8, 0x4d8);
PIN(group_anchors, id_148, 0x148);
PIN(group_anchors, active_14c, 0x14c);
PIN(group_anchors, index_words_150, 0x150);
PIN(group_anchors, index_words_170, 0x170);
PIN(cleanup_packet, command_00, 0);
PIN(cleanup_packet, zero_04, 4);
PIN(cleanup_packet, token_08, 8);
PIN(cleanup_packet, group_10, 0x10);
PIN(cleanup_packet, zero_18, 0x18);
PIN(callback_record, zero_00, 0);
PIN(callback_record, backend_word_04, 4);
PIN(callback_record, group_08, 8);
PIN(callback_record, unwritten_0c, 0xc);
PIN(borrowed_frame, group_10, 0x10);
PIN(borrowed_frame, mode_14, 0x14);
PIN(borrowed_frame, initial_index_18, 0x18);
PIN(borrowed_frame, addresses_20, 0x20);
PIN(borrowed_frame, cleanup_group_28, 0x28);
PIN(borrowed_frame, callback_30, 0x30);
PIN(borrowed_frame, packet_40, 0x40);
PIN(borrowed_frame, unknown_70, 0x70);
PIN(borrowed_frame, guard_80, 0x80);
PIN(borrowed_frame, unknown_88, 0x88);
#undef PIN
_Static_assert(sizeof(cleanup_packet) == 48, "cleanup packet size");
_Static_assert(sizeof(callback_record) == 16, "callback area size");
_Static_assert(sizeof(borrowed_frame) == 0xa0, "projected frame size");

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

static int32_t signed_word(uint32_t value)
{
    int32_t result;
    memcpy(&result, &value, sizeof result);
    return result;
}

#define BACKEND_FIELD(base, field) \
    ((void *)(uintptr_t)((base) + offsetof(backend_anchors, field)))
#define GROUP_FIELD(base, stride, field) \
    ((void *)(uintptr_t)((base) + (stride) + offsetof(group_anchors, field)))

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_buffer_bindings *imports) \
    { \
        type call; \
        const void *cell = require_cell(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_ioctl, rt_video_out_buffer_ioctl_fn, ioctl_slot, "VideoOut buffers ioctl")
IMPORT_READER(load_error, rt_video_out_buffer_error_fn, error_slot, "VideoOut buffers __error")
IMPORT_READER(load_getpid, rt_video_out_buffer_getpid_fn, getpid_slot, "VideoOut buffers getpid")
IMPORT_READER(load_printf, rt_video_out_buffer_printf_fn, printf_slot, "VideoOut buffers printf")
IMPORT_READER(load_puts, rt_video_out_buffer_puts_fn, puts_slot, "VideoOut buffers puts")
IMPORT_READER(load_stack_fail, rt_video_out_buffer_stack_fail_fn, stack_fail_slot, "VideoOut buffers __stack_chk_fail")
#undef IMPORT_READER

static uint64_t load_guard_pointer(const rt_video_out_buffer_bindings *imports)
{
    return read_quad(require_cell(imports->guard_pointer_slot, "VideoOut buffers __stack_chk_guard"));
}

static uint64_t finish(const rt_video_out_buffer_bindings *imports, const borrowed_frame *frame,
                       uint64_t guard_pointer, uint32_t result)
{
    const uint64_t current = read_quad((const void *)(uintptr_t)guard_pointer);
    const uint64_t saved = read_quad(frame->guard_80);
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

static uint32_t first_free_group(uint64_t backend)
{
    for (uint32_t group = 0; group < group_count; ++group) {
        const uint64_t stride = (uint64_t)group * group_stride_bytes;
        if (read_word(GROUP_FIELD(backend, stride, active_14c)) == 0) return group;
    }
    return group_count;
}

static void clear_group_index_words(uint64_t backend, uint64_t stride)
{
    /* The upper half is cleared first, before any range rejection. */
    access_boundary();
    memset(GROUP_FIELD(backend, stride, index_words_170), 0, clear_span_bytes);
    access_boundary();
    memset(GROUP_FIELD(backend, stride, index_words_150), 0, clear_span_bytes);
    access_boundary();
}

static int indices_available(uint64_t backend, uint32_t index, uint32_t count)
{
    uint64_t cursor = backend + offsetof(backend_anchors, index_group_08)
        + (uint64_t)index * word_bytes;
    uint64_t remaining = count;
    do {
        if (read_word((const void *)(uintptr_t)cursor) != unmapped_index) return 0;
        cursor += word_bytes;
        --remaining;
    } while (remaining != 0);
    return 1;
}

static uint64_t finish_registered_group(const rt_video_out_buffer_bindings *imports,
                                       borrowed_frame *frame, uint64_t backend)
{
    uint32_t group;
    uint64_t guard_pointer;
    if (signed_word(read_word(BACKEND_FIELD(backend, walk_count_458))) <= 0) {
        group = read_word(frame->group_10);
        guard_pointer = load_guard_pointer(imports);
        return finish(imports, frame, guard_pointer, group);
    }

    const uint32_t warning_result_bits = (uint32_t)rt_video_out_read_dipswitch();
    guard_pointer = load_guard_pointer(imports);
    group = read_word(frame->group_10);
    if (signed_word(warning_result_bits) <= 0)
        return finish(imports, frame, guard_pointer, group);
    if (read_word(BACKEND_FIELD(backend, warning_count_45c)) > warning_limit)
        return finish(imports, frame, guard_pointer, group);

    const uint32_t pid_bits = load_getpid(imports)();
    const char *format = require_cell(imports->warning_format, "VideoOut buffers warning format");
    load_printf(imports)(format, (int)signed_word(pid_bits));
    uint32_t warnings = read_word(BACKEND_FIELD(backend, warning_count_45c));
    if (warnings == warning_limit) {
        const char *message = require_cell(imports->warning_limit_message,
            "VideoOut buffers warning limit message");
        load_puts(imports)(message);
        warnings = read_word(BACKEND_FIELD(backend, warning_count_45c));
    }
    write_word(BACKEND_FIELD(backend, warning_count_45c), warnings + 1u);
    /* Keep the pointer captured after the DIP-switch call across diagnostics. */
    return finish(imports, frame, guard_pointer, group);
}

static void clear_cleanup_packet(cleanup_packet *packet)
{
    access_boundary();
    memset(packet, 0, clear_span_bytes);
    access_boundary();
    memset((uint8_t *)packet + offsetof(cleanup_packet, group_10), 0, clear_span_bytes);
    access_boundary();
}

static uint64_t cleanup_failed_group(const rt_video_out_buffer_bindings *imports,
    borrowed_frame *frame, uint64_t backend, uint32_t current_index, uint32_t original_error)
{
    const uint32_t skip_prior = read_word(BACKEND_FIELD(backend, skip_prior_removals_4d8));
    const uint64_t initial_index_wire = read_quad(frame->initial_index_18);
    const uint32_t target_index = skip_prior != 0 ? current_index : (uint32_t)initial_index_wire;
    while (signed_word(current_index) > signed_word(target_index)) {
        --current_index;
        rt_video_out_remove_address((void *)(uintptr_t)backend, signed_word(current_index));
    }

    /* These two saved group views are independently live after removals. */
    const uint64_t cleanup_group = read_quad(frame->cleanup_group_28);
    const uint32_t callback_group = read_word(frame->group_10);
    const uint64_t stride = cleanup_group * group_stride_bytes;
    if (read_word(GROUP_FIELD(backend, stride, active_14c)) == 0) {
        const uint64_t guard_pointer = load_guard_pointer(imports);
        return finish(imports, frame, guard_pointer, original_error);
    }

    const uint64_t token = read_quad(BACKEND_FIELD(backend, token_438));
    const int32_t descriptor = signed_word(read_word(require_cell(imports->descriptor_word,
        "VideoOut buffers descriptor")));
    clear_cleanup_packet(&frame->packet_40);
    write_word(frame->packet_40.command_00, cleanup_command);
    write_quad(frame->packet_40.token_08, token);
    write_quad(frame->packet_40.group_10, cleanup_group);
    const int32_t removed = load_ioctl(imports)(descriptor, cleanup_request, (void *)&frame->packet_40);
    /* Refresh BEFORE testing the result; keep this pointer across __error and
       notification rather than copying either neighboring helper's rule. */
    const uint64_t guard_pointer = load_guard_pointer(imports);
    if (removed < 0) {
        load_error(imports)();
        return finish(imports, frame, guard_pointer, original_error);
    }

    write_word(GROUP_FIELD(backend, stride, active_14c), 0);
    const uint64_t target = read_quad(require_cell(imports->callback_target_slot,
        "VideoOut buffers cleanup callback target"));
    if (target != 0) {
        write_word(frame->callback_30.zero_00, 0);
        const uint64_t argument = read_quad(require_cell(imports->callback_argument_slot,
            "VideoOut buffers cleanup callback argument"));
        const uint32_t callback_word = read_word(BACKEND_FIELD(backend, callback_word_434));
        write_word(frame->callback_30.backend_word_04, callback_word);
        write_word(frame->callback_30.group_08, callback_group);
        if (!imports->callback_bridge)
            rt_fatal("VideoOut buffers cleanup callback bridge", "the actual opaque callback ABI has not been bound");
        imports->callback_bridge(target, cleanup_event, (void *)&frame->callback_30,
                                 argument, (uint64_t)callback_word, target);
    }
    return finish(imports, frame, guard_pointer, original_error);
}

uint64_t rt_video_out_register_buffers(void *backend_storage, uint64_t initial_index_wire,
    const void *address_storage, uint32_t count, const void *attribute_storage, uint32_t mode)
{
    const rt_video_out_buffer_bindings *imports = rt_video_out_buffer_imports;
    if (!imports)
        rt_fatal("VideoOut buffer registration", "real module data and import cells have not been bound");
    const uint64_t backend = (uint64_t)(uintptr_t)backend_storage;
    const uint32_t initial_index = (uint32_t)initial_index_wire;
    const uint64_t entry_guard_pointer = load_guard_pointer(imports);
    _Alignas(32) borrowed_frame frame;
    write_word(frame.mode_14, mode);
    write_quad(frame.addresses_20, (uint64_t)(uintptr_t)address_storage);
    write_quad(frame.guard_80, read_quad((const void *)(uintptr_t)entry_guard_pointer));

    const uint32_t selected_group = first_free_group(backend);
    if (selected_group == group_count)
        return finish(imports, &frame, entry_guard_pointer, no_free_group_error);
    const uint64_t selected_stride = (uint64_t)selected_group * group_stride_bytes;
    clear_group_index_words(backend, selected_stride);
    if (signed_word(initial_index + count) > buffer_limit || initial_index > buffer_limit
        || count - 1u > largest_count_minus_one)
        return finish(imports, &frame, entry_guard_pointer, parameter_error);

    write_word(BACKEND_FIELD(backend, walk_count_458), 0);
    if (!indices_available(backend, initial_index, count))
        return finish(imports, &frame, entry_guard_pointer, duplicate_index_error);
    const uint32_t group = read_word(GROUP_FIELD(backend, selected_stride, id_148));
    write_quad(frame.initial_index_18, initial_index_wire);
    const uint32_t attributes_result = (uint32_t)rt_video_out_apply_group_attributes(
        (void *)(uintptr_t)backend, group, attribute_storage, 0);
    if (signed_word(attributes_result) < 0)
        return finish(imports, &frame, entry_guard_pointer, attributes_result);

    write_word(frame.group_10, group);
    write_quad(frame.cleanup_group_28, (uint64_t)(int64_t)signed_word(group));
    uint32_t current_index = (uint32_t)read_quad(frame.initial_index_18);
    uint64_t offset = 0;
    uint64_t remaining = count;
    do {
        const uint32_t paired = read_word(frame.mode_14) != 0;
        const uint64_t addresses = read_quad(frame.addresses_20);
        const uint32_t current_group = read_word(frame.group_10);
        uint64_t first;
        uint64_t second = 0;
        if (paired != 0) {
            first = read_quad((const void *)(uintptr_t)(addresses + offset * 2u));
            second = read_quad((const void *)(uintptr_t)(addresses + offset * 2u + quad_bytes));
        } else {
            first = read_quad((const void *)(uintptr_t)(addresses + offset));
        }
        const uint32_t result = (uint32_t)rt_video_out_register_addresses(
            (void *)(uintptr_t)backend, signed_word(current_index), signed_word(current_group),
            first, second, paired);
        if (signed_word(result) < 0)
            return cleanup_failed_group(imports, &frame, backend, current_index, result);
        offset += quad_bytes;
        ++current_index;
        --remaining;
    } while (remaining != 0);
    return finish_registered_group(imports, &frame, backend);
}
