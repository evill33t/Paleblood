/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Process VideoOut's post-attribute record requests.

   The complete 538-byte internal function at module offset 0x3d30 in
   libSceVideoOut, SHA-256
   33a6f16349f07637e168db0d6338f0b2ae8aea626b86bc3d28325ecb5155e2c1,
   polls command 12 and submits command 8 under a fixed eight-submit budget.
   A failed first ioctl reads its errno cell, including the errno-zero fallthrough;
   a failed second ioctl only calls the error getter. No retry policy is inferred
   from the second errno value. Failed locking logs and still reaches unlock.

   The projected byte-backed frame keeps the borrowed regions, saved backend and
   saved guard at their known relative offsets. Work buffers are not initialized
   or cleared between attempts. In particular, the transformer's leading 32 output
   bytes stay untouched although command 8 advertises the full 736-byte buffer.
   Only the real provider can establish meaningful service output and actual
   access footprints. No default scratch, descriptor, clock, mutex or guard is
   supplied. Missing host bindings are named port-dependency stops, not statuses.

   This is uncompiled/unvalidated source progress, with ordinary flat 64-bit
   little-endian storage and compatible host-callable imports. It does not prove
   identical unwritten stack bytes, arbitrary native physical-frame aliases,
   inherited GP/SIMD/flags, compiler lowering, exact faults or service acceptance.
   The separately ported numerical helper retains its own FP/import limitations. */
#include "runtime/video_out_post_attributes.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut record requests currently require a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(unsigned int) == sizeof(uint32_t), "32-bit diagnostic varargs");
_Static_assert(sizeof(rt_video_out_ioctl_fn) == sizeof(uint64_t), "ioctl pointer");
_Static_assert(sizeof(rt_video_out_error_fn) == sizeof(uint64_t), "error pointer");
_Static_assert(sizeof(rt_video_out_mutex_fn) == sizeof(uint64_t), "mutex pointer");
_Static_assert(sizeof(rt_video_out_print_fn) == sizeof(uint64_t), "print pointer");
_Static_assert(sizeof(rt_video_out_time_fn) == sizeof(uint64_t), "time pointer");
_Static_assert(sizeof(rt_video_out_stack_fail_fn) == sizeof(uint64_t), "fail pointer");

const rt_video_out_post_bindings *volatile rt_video_out_post_imports;

enum {
    word_bytes = sizeof(uint32_t),
    quad_bytes = sizeof(uint64_t),
    packet_clear_bytes = 32,
    packet_clear_overlap = 16,
    source_bytes = 64,
    payload_bytes = 736,
    maximum_submits = 8,
    poll_command = 12,
    submit_command = 8,
    diagnostic_line = 0x166,
};
static const uint32_t feature_threshold = UINT32_C(0x920000);
static const uint64_t record_request = UINT64_C(0xc0308203);
static const uint64_t parameter_error = UINT64_C(0x80290001);
static const uint64_t address_error = UINT64_C(0x80290002);
static const uint64_t busy_error = UINT64_C(0x80290009);
static const uint64_t mapped_error_45 = UINT64_C(0x8029001c);
static const uint64_t generic_error = UINT64_C(0x802900ff);
static const uint64_t allocation_error = UINT64_C(0x8029100c);

typedef struct {
    uint8_t unknown_00[0x438];
    uint8_t request_token_438[quad_bytes];
    uint8_t unknown_440[8];
    uint8_t process_time_448[quad_bytes];
    uint8_t unknown_450[0x1c];
    uint8_t time_flag_46c[word_bytes];
} backend_anchors;

typedef struct {
    uint8_t first_00[quad_bytes];
    uint8_t signed_word_08[word_bytes];
} source_anchors;

typedef struct {
    uint8_t command_00[word_bytes];
    uint8_t zero_04[word_bytes];
    uint8_t token_08[quad_bytes];
    uint8_t value_10[quad_bytes];
    uint8_t value_18[quad_bytes];
    uint8_t value_20[quad_bytes];
    uint8_t value_28[quad_bytes];
} request_packet;

typedef struct {
    uint8_t unknown_00[0x18];
    uint8_t backend_18[quad_bytes];
    uint8_t source_20[source_bytes];
    uint8_t payload_60[payload_bytes];
    request_packet packet_340;
    uint8_t unknown_370[0x10];
    uint8_t guard_380[quad_bytes];
    uint8_t unknown_388[0x18];
} borrowed_frame;

_Static_assert(offsetof(backend_anchors, request_token_438) == 0x438, "backend token");
_Static_assert(offsetof(backend_anchors, process_time_448) == 0x448, "backend time");
_Static_assert(offsetof(backend_anchors, time_flag_46c) == 0x46c, "backend time flag");
_Static_assert(offsetof(source_anchors, first_00) == 0, "source first quad");
_Static_assert(offsetof(source_anchors, signed_word_08) == 8, "source signed word");
_Static_assert(offsetof(request_packet, command_00) == 0, "packet command");
_Static_assert(offsetof(request_packet, zero_04) == 4, "packet high command word");
_Static_assert(offsetof(request_packet, token_08) == 8, "packet token");
_Static_assert(offsetof(request_packet, value_10) == 0x10, "packet first value");
_Static_assert(offsetof(request_packet, value_18) == 0x18, "packet second value");
_Static_assert(offsetof(request_packet, value_20) == 0x20, "packet third value");
_Static_assert(offsetof(request_packet, value_28) == 0x28, "packet fourth value");
_Static_assert(sizeof(request_packet) == 0x30, "packet size");
_Static_assert(offsetof(borrowed_frame, backend_18) == 0x18, "saved backend");
_Static_assert(offsetof(borrowed_frame, source_20) == 0x20, "borrowed source");
_Static_assert(offsetof(borrowed_frame, payload_60) == 0x60, "borrowed payload");
_Static_assert(offsetof(borrowed_frame, packet_340) == 0x340, "borrowed packet");
_Static_assert(offsetof(borrowed_frame, guard_380) == 0x380, "saved guard");
_Static_assert(sizeof(borrowed_frame) == 0x3a0, "projected frame size");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static const void *require_cell(const void *cell, const char *name)
{
    if (!cell) rt_fatal(name, "the required module storage or import cell is unbound");
    return cell;
}

static uint32_t read_word(const void *storage)
{
    uint32_t value;
    access_boundary();
    memcpy(&value, storage, sizeof value);
    access_boundary();
    return value;
}

static uint64_t read_quad(const void *storage)
{
    uint64_t value;
    access_boundary();
    memcpy(&value, storage, sizeof value);
    access_boundary();
    return value;
}

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

#define BACKEND_ADDRESS(base, field) ((const void *)(uintptr_t)((base) + offsetof(backend_anchors, field)))

/* Target contents are loaded at each reached call, never cached at entry. */
#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_post_bindings *imports) \
    { \
        type call; \
        const void *cell = require_cell(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_ioctl, rt_video_out_ioctl_fn, ioctl_slot, "VideoOut ioctl")
IMPORT_READER(load_error, rt_video_out_error_fn, error_slot, "VideoOut __error")
IMPORT_READER(load_lock, rt_video_out_mutex_fn, lock_slot, "VideoOut mutex lock")
IMPORT_READER(load_print, rt_video_out_print_fn, print_slot, "VideoOut printf")
IMPORT_READER(load_time, rt_video_out_time_fn, time_slot, "VideoOut process time")
IMPORT_READER(load_unlock, rt_video_out_mutex_fn, unlock_slot, "VideoOut mutex unlock")
IMPORT_READER(load_stack_fail, rt_video_out_stack_fail_fn, stack_fail_slot, "VideoOut __stack_chk_fail")
#undef IMPORT_READER

static uint64_t load_guard_pointer(const rt_video_out_post_bindings *imports)
{
    return read_quad(require_cell(imports->guard_pointer_slot, "VideoOut __stack_chk_guard"));
}

static int32_t load_descriptor(const rt_video_out_post_bindings *imports)
{
    return signed_word(read_word(require_cell(imports->descriptor_word, "VideoOut descriptor")));
}

static uint64_t finish(const rt_video_out_post_bindings *imports, const borrowed_frame *frame,
                       uint64_t entry_guard_pointer, uint64_t result, int active)
{
    const uint64_t pointer = active ? load_guard_pointer(imports) : entry_guard_pointer;
    const uint64_t current = read_quad((const void *)(uintptr_t)pointer);
    const uint64_t saved = read_quad(frame->guard_380);
    if (current != saved) {
        load_stack_fail(imports)();
#if defined(__x86_64__)
        __asm__ volatile("ud2" ::: "memory");
#else
        __builtin_trap();
#endif
        __builtin_unreachable();
    }
    return result;
}

static void make_poll_packet(request_packet *packet, uint64_t token, void *source)
{
    static const uint8_t zeros[packet_clear_bytes] = {0};
    access_boundary();
    memcpy((uint8_t *)packet, zeros, packet_clear_bytes);
    access_boundary();
    memcpy((uint8_t *)packet + packet_clear_overlap, zeros, packet_clear_bytes);
    access_boundary();
    write_word(packet->command_00, poll_command);
    write_quad(packet->token_08, token);
    write_quad(packet->value_10, (uint64_t)(uintptr_t)source);
    write_quad(packet->value_18, source_bytes);
}

static void make_submit_packet(request_packet *packet, uint64_t token, uint64_t signed_value,
                                uint64_t first, void *payload)
{
    write_quad((uint8_t *)packet + offsetof(request_packet, command_00), submit_command);
    write_quad(packet->token_08, token);
    write_quad(packet->value_10, signed_value);
    write_quad(packet->value_18, first);
    write_quad(packet->value_20, (uint64_t)(uintptr_t)payload);
    write_quad(packet->value_28, payload_bytes);
}

static uint64_t first_error_result(uint32_t error)
{
    /* The native dispatch spans errno 1..45 after wrapping DEC and unsigned
       range comparison. Zero is handled by the caller's fallthrough. */
    const uint32_t index = error - 1u;
    if (index > 44u) return generic_error;
    switch (error) {
    case 1: case 22: return parameter_error;
    case 12: return allocation_error;
    case 14: return address_error;
    case 16: return busy_error;
    case 45: return mapped_error_45;
    default: return generic_error;
    }
}

static void empty_output(const rt_video_out_post_bindings *imports, borrowed_frame *frame)
{
    void *mutex = (void *)require_cell(imports->mutex_storage, "VideoOut mutex storage");
    const int32_t locked = load_lock(imports)(mutex);
    if (locked < 0)
        (void)load_print(imports)("[VideoOut error] %i, 0x%x\n", diagnostic_line, (unsigned int)(uint32_t)locked);
    const uint64_t backend = read_quad(frame->backend_18);
    if (read_word(BACKEND_ADDRESS(backend, time_flag_46c)) != 0) {
        const uint64_t time = load_time(imports)();
        write_quad((void *)(uintptr_t)(backend + offsetof(backend_anchors, process_time_448)), time);
    }
    (void)load_unlock(imports)(mutex);
}

uint64_t rt_video_out_post_attributes(void *backend_storage)
{
    const rt_video_out_post_bindings *imports = rt_video_out_post_imports;
    if (!imports)
        rt_fatal("VideoOut post attributes", "real module data and import cells have not been bound");
    const uint64_t entry_guard = load_guard_pointer(imports);
    _Alignas(32) borrowed_frame frame;
    write_quad(frame.guard_380, read_quad((const void *)(uintptr_t)entry_guard));
    const uint32_t feature = read_word(require_cell(imports->feature_word, "VideoOut feature gate"));
    if (feature < feature_threshold)
        return finish(imports, &frame, entry_guard, 0, 0);
    const uint64_t backend = (uint64_t)(uintptr_t)backend_storage;
    const uint64_t token = read_quad(BACKEND_ADDRESS(backend, request_token_438));
    write_quad(frame.backend_18, backend);
    uint32_t remaining = maximum_submits;
    for (;;) {
        const int32_t poll_descriptor = load_descriptor(imports);
        make_poll_packet(&frame.packet_340, token, frame.source_20);
        const int32_t polled = load_ioctl(imports)(poll_descriptor, record_request, (void *)&frame.packet_340);
        if (polled < 0) {
            const void *cell = load_error(imports)();
            const uint32_t error = read_word(cell);
            if (error != 0)
                return finish(imports, &frame, entry_guard, first_error_result(error), 1);
        }
        const uint64_t source = (uint64_t)(uintptr_t)frame.source_20;
        if (read_quad((const void *)(uintptr_t)(source + offsetof(source_anchors, first_00))) == 0) {
            empty_output(imports, &frame);
            return finish(imports, &frame, entry_guard, 0, 1);
        }
        const uint64_t transformed = rt_video_out_transform_records(frame.source_20, frame.payload_60);
        if ((uint32_t)transformed != 0)
            return finish(imports, &frame, entry_guard, transformed, 1);
        const uint32_t word = read_word((const void *)(uintptr_t)(source + offsetof(source_anchors, signed_word_08)));
        const uint64_t signed_value = (uint64_t)(int64_t)signed_word(word);
        const uint64_t first = read_quad((const void *)(uintptr_t)(source + offsetof(source_anchors, first_00)));
        const int32_t submit_descriptor = load_descriptor(imports);
        make_submit_packet(&frame.packet_340, token, signed_value, first, frame.payload_60);
        const int32_t submitted = load_ioctl(imports)(submit_descriptor, record_request, (void *)&frame.packet_340);
        if (submitted < 0)
            (void)load_error(imports)();
        --remaining;
        if (remaining == 0)
            return finish(imports, &frame, entry_guard, generic_error, 1);
    }
}
