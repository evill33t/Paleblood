/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Register queried VideoOut address ranges and publish their group entries.

   The complete 1194-byte internal helper at module offset 0x6f70 in
   libSceVideoOut, SHA-256
   33a6f16349f07637e168db0d6338f0b2ae8aea626b86bc3d28325ecb5155e2c1,
   consumes six GP arguments. Negative group/index inputs are sign-extended
   before address scaling; no array bounds follow from these access formulas.
   Signed alignment/length words become 64-bit bit patterns used by unsigned
   division and range comparisons. No progress check or query limit is added.

   Each completed range can increment a live backend counter. Those changes
   survive later alignment, query and ioctl errors. Errno zero after a negative
   ioctl returns zero without publishing the success entries or callback. The
   first and second walks use different saved-baseline and guard-pointer epochs.

   A projected byte-backed frame retains the known relative query, command,
   callback and saved fields. Query storage is reused without clearing. Command
   construction writes 44 of 48 bytes; callback padding is also left untouched.
   Real provider initialization/footprints and opaque callback semantics remain
   external dependencies. The callback bridge exposes the known GP subset only,
   not a claim about native arity, R8/R9, SIMD or arithmetic flags at dispatch.

   Uncompiled/unvalidated source for ordinary flat 64-bit little-endian storage and
   compatible host imports. It does not establish equal unwritten stack bytes,
   arbitrary native physical-frame aliases, concurrent access, compiler lowering,
   or service acceptance. x86 DIVQ retains its zero-divisor fault; ARM64 explicitly
   traps rather than inventing a status, but its fault/signal identity differs.
   No module bindings, real service or SCE export are installed here. */
#include "runtime/video_out_addresses.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut address registration requires a little-endian host"
#endif
#if !defined(__x86_64__) && !defined(__aarch64__)
#error "VideoOut address division currently supports x86-64 and ARM64 only"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(rt_video_out_address_query_fn) == sizeof(uint64_t), "query pointer");
_Static_assert(sizeof(rt_video_out_address_ioctl_fn) == sizeof(uint64_t), "ioctl pointer");
_Static_assert(sizeof(rt_video_out_address_error_fn) == sizeof(uint64_t), "error pointer");
_Static_assert(sizeof(rt_video_out_address_stack_fail_fn) == sizeof(uint64_t), "fail pointer");

const rt_video_out_address_bindings *volatile rt_video_out_address_imports;

enum {
    word_bytes = sizeof(uint32_t),
    quad_bytes = sizeof(uint64_t),
    group_stride_bytes = 0xa8,
    index_pair_bytes = 16,
    query_bytes = 72,
    required_query_flag_18 = 0x10,
    required_query_flag_20 = 2,
    uncounted_query_type = 3,
    registration_event = 1,
};
static const uint64_t registration_request = UINT64_C(0xc0308206);
static const uint32_t parameter_error = UINT32_C(0x80290001);
static const uint32_t address_error = UINT32_C(0x80290002);
static const uint32_t busy_error = UINT32_C(0x80290009);
static const uint32_t query_error = UINT32_C(0x80290013);
static const uint32_t coherence_error = UINT32_C(0x80290014);
static const uint32_t alignment_error = UINT32_C(0x80290015);
static const uint32_t mapped_error_45 = UINT32_C(0x8029001c);
static const uint32_t generic_error = UINT32_C(0x802900ff);
static const uint32_t allocation_error = UINT32_C(0x8029100c);
static const uint64_t residue_mask = UINT64_C(0xffff);
static const uint64_t residue_sign = UINT64_C(0x8000000000000000);

/* Offset anchors describe accesses, not complete foreign objects or arrays. */
typedef struct {
    uint8_t unknown_00[8];
    uint8_t index_group_08[word_bytes];
    uint8_t unknown_0c[0x3c];
    uint8_t index_first_48[quad_bytes];
    uint8_t index_second_50[quad_bytes];
    uint8_t unknown_58[0x3d8];
    uint8_t descriptor_430[word_bytes];
    uint8_t callback_word_434[word_bytes];
    uint8_t token_438[quad_bytes];
    uint8_t unknown_440[0x18];
    uint8_t walk_count_458[word_bytes];
} backend_anchors;

typedef struct {
    uint8_t unknown_00[0x150];
    uint8_t index_active_150[word_bytes];
    uint8_t unknown_154[0x8c];
    uint8_t length_1e0[word_bytes];
    uint8_t alignment_1e4[word_bytes];
} group_anchors;

typedef struct {
    uint8_t start_00[quad_bytes];
    uint8_t end_08[quad_bytes];
    uint8_t field_10[quad_bytes];
    uint8_t flags_18;
    uint8_t unknown_19[3];
    uint8_t type_1c[word_bytes];
    uint8_t flags_20;
    uint8_t unknown_21[0x27];
} query_anchors;

typedef struct {
    uint8_t token_00[quad_bytes];
    uint8_t index_08[word_bytes];
    uint8_t group_0c[word_bytes];
    uint8_t first_10[quad_bytes];
    uint8_t second_18[quad_bytes];
    uint8_t mode_20[quad_bytes];
    uint8_t zero_28[word_bytes];
    uint8_t unwritten_2c[word_bytes];
} command_anchors;

typedef struct {
    uint8_t event_00[word_bytes];
    uint8_t backend_word_04[word_bytes];
    uint8_t index_08[word_bytes];
    uint8_t unwritten_0c[word_bytes];
    uint8_t first_10[quad_bytes];
    uint8_t second_18[quad_bytes];
    uint8_t mode_20[word_bytes];
    uint8_t group_24[word_bytes];
} callback_record;

typedef struct {
    uint8_t first_baseline_00[quad_bytes];
    uint8_t group_stride_08[quad_bytes];
    uint8_t length_10[quad_bytes];
    uint8_t second_18[quad_bytes];
    uint8_t unknown_20[4];
    uint8_t descriptor_24[word_bytes];
    uint8_t first_28[quad_bytes];
    uint8_t token_30[quad_bytes];
    uint8_t backend_38[quad_bytes];
    uint8_t unknown_40[4];
    uint8_t group_44[word_bytes];
    uint8_t mode_48[word_bytes];
    uint8_t index_4c[word_bytes];
    callback_record callback_50;
    uint8_t workspace_78[query_bytes];
    uint8_t guard_c0[quad_bytes];
} borrowed_frame;

#define PIN(type, field, offset) _Static_assert(offsetof(type, field) == offset, #type "." #field)
PIN(backend_anchors, index_group_08, 8);
PIN(backend_anchors, index_first_48, 0x48);
PIN(backend_anchors, index_second_50, 0x50);
PIN(backend_anchors, descriptor_430, 0x430);
PIN(backend_anchors, callback_word_434, 0x434);
PIN(backend_anchors, token_438, 0x438);
PIN(backend_anchors, walk_count_458, 0x458);
PIN(group_anchors, index_active_150, 0x150);
PIN(group_anchors, length_1e0, 0x1e0);
PIN(group_anchors, alignment_1e4, 0x1e4);
PIN(query_anchors, start_00, 0);
PIN(query_anchors, end_08, 8);
PIN(query_anchors, field_10, 0x10);
PIN(query_anchors, flags_18, 0x18);
PIN(query_anchors, type_1c, 0x1c);
PIN(query_anchors, flags_20, 0x20);
PIN(command_anchors, token_00, 0);
PIN(command_anchors, index_08, 8);
PIN(command_anchors, group_0c, 0xc);
PIN(command_anchors, first_10, 0x10);
PIN(command_anchors, second_18, 0x18);
PIN(command_anchors, mode_20, 0x20);
PIN(command_anchors, zero_28, 0x28);
PIN(command_anchors, unwritten_2c, 0x2c);
PIN(callback_record, event_00, 0);
PIN(callback_record, backend_word_04, 4);
PIN(callback_record, index_08, 8);
PIN(callback_record, unwritten_0c, 0xc);
PIN(callback_record, first_10, 0x10);
PIN(callback_record, second_18, 0x18);
PIN(callback_record, mode_20, 0x20);
PIN(callback_record, group_24, 0x24);
PIN(borrowed_frame, first_baseline_00, 0);
PIN(borrowed_frame, group_stride_08, 8);
PIN(borrowed_frame, length_10, 0x10);
PIN(borrowed_frame, second_18, 0x18);
PIN(borrowed_frame, descriptor_24, 0x24);
PIN(borrowed_frame, first_28, 0x28);
PIN(borrowed_frame, token_30, 0x30);
PIN(borrowed_frame, backend_38, 0x38);
PIN(borrowed_frame, group_44, 0x44);
PIN(borrowed_frame, mode_48, 0x48);
PIN(borrowed_frame, index_4c, 0x4c);
PIN(borrowed_frame, callback_50, 0x50);
PIN(borrowed_frame, workspace_78, 0x78);
PIN(borrowed_frame, guard_c0, 0xc0);
#undef PIN
_Static_assert(sizeof(query_anchors) == query_bytes, "query size");
_Static_assert(sizeof(command_anchors) == 48, "command size");
_Static_assert(sizeof(callback_record) == 40, "callback size");
_Static_assert(sizeof(borrowed_frame) == 0xc8, "projected frame size");

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
        access_boundary(); \
        return value; \
    }
STORAGE_READER(read_byte, uint8_t)
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

#define BACKEND_FIELD(base, field) ((void *)(uintptr_t)((base) + offsetof(backend_anchors, field)))
#define GROUP_FIELD(base, stride, field) ((void *)(uintptr_t)((base) + (stride) + offsetof(group_anchors, field)))
#define QUERY_FIELD(frame, field) ((frame)->workspace_78 + offsetof(query_anchors, field))
#define COMMAND_FIELD(frame, field) ((frame)->workspace_78 + offsetof(command_anchors, field))

#define IMPORT_READER(name, type, field, label) \
    static type name(const rt_video_out_address_bindings *imports) \
    { \
        type call; \
        const void *cell = require_cell(imports->field, label); \
        access_boundary(); \
        memcpy(&call, cell, sizeof call); \
        access_boundary(); \
        if (!call) rt_fatal(label, "the function import target is unbound"); \
        return call; \
    }
IMPORT_READER(load_query, rt_video_out_address_query_fn, query_slot, "VideoOut sceKernelVirtualQuery")
IMPORT_READER(load_ioctl, rt_video_out_address_ioctl_fn, ioctl_slot, "VideoOut address ioctl")
IMPORT_READER(load_error, rt_video_out_address_error_fn, error_slot, "VideoOut address __error")
IMPORT_READER(load_stack_fail, rt_video_out_address_stack_fail_fn, stack_fail_slot, "VideoOut address __stack_chk_fail")
#undef IMPORT_READER

static uint64_t load_guard_pointer(const rt_video_out_address_bindings *imports)
{
    return read_quad(require_cell(imports->guard_pointer_slot, "VideoOut address __stack_chk_guard"));
}

static uint64_t finish(const rt_video_out_address_bindings *imports, const borrowed_frame *frame,
                       uint64_t guard_pointer, uint32_t result)
{
    const uint64_t current = read_quad((const void *)(uintptr_t)guard_pointer);
    const uint64_t saved = read_quad(frame->guard_c0);
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
    /* Unlike C division by zero, this retains the actual DIVQ fault boundary. */
    __asm__ volatile("divq %2" : "+a"(quotient), "+d"(remainder) : "r"(divisor) : "cc", "memory");
    return remainder;
#else
    /* ARM UDIV itself would return zero on zero divisor. Stop explicitly; the
       host trap is not claimed to reproduce the native x86 fault/signal. */
    if (divisor == 0) __builtin_trap();
    return dividend % divisor;
#endif
}

static uint32_t walk_range(const rt_video_out_address_bindings *imports, borrowed_frame *frame,
                           uint64_t cursor, uint64_t range_end, int first_walk,
                           uint32_t *completed_count)
{
    uint64_t first_start = 0;
    uint64_t register_baseline;
    uint32_t count = 0;
    do {
        const int32_t queried = load_query(imports)(cursor, 0, (void *)frame->workspace_78, query_bytes);
        if (queried < 0) return query_error;
        if ((read_byte(QUERY_FIELD(frame, flags_20)) & required_query_flag_20) == 0)
            return query_error;
        if ((read_byte(QUERY_FIELD(frame, flags_18)) & required_query_flag_18) == 0)
            return query_error;
        const uint32_t indicator = read_word(QUERY_FIELD(frame, type_1c)) != uncounted_query_type;
        const uint64_t start = read_quad(QUERY_FIELD(frame, start_00));
        const uint64_t field = read_quad(QUERY_FIELD(frame, field_10));
        /* Zero start is the sentinel itself, so it can reset the baseline on
           more than one iteration. First-walk storage stays live in frame00. */
        if (first_start == 0) {
            first_start = start;
            if (first_walk) write_quad(frame->first_baseline_00, field);
            else register_baseline = field;
        } else {
            const uint64_t baseline = first_walk ? read_quad(frame->first_baseline_00) : register_baseline;
            if (start - first_start != field - baseline) return coherence_error;
        }
        uint64_t rounded = field;
        if ((field & residue_sign) != 0) rounded += residue_mask;
        rounded &= ~residue_mask;
        if ((start & residue_mask) != field - rounded) return alignment_error;
        cursor = read_quad(QUERY_FIELD(frame, end_08));
        count += indicator;
    } while (cursor < range_end);
    *completed_count = count;
    return 0;
}

static void increment_walk_count(uint64_t backend)
{
    void *cell = BACKEND_FIELD(backend, walk_count_458);
    const uint32_t count = read_word(cell);
    write_word(cell, count + 1u);
}

static uint32_t ioctl_error_result(uint32_t error)
{
    /* Canonical native 46-cell dispatch, including zero's no-publication path. */
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

uint64_t rt_video_out_register_addresses(void *backend_storage, int32_t index,
    int32_t group, uint64_t first, uint64_t second, uint32_t mode)
{
    const rt_video_out_address_bindings *imports = rt_video_out_address_imports;
    if (!imports)
        rt_fatal("VideoOut address registration", "real module data and import cells have not been bound");
    uint64_t backend = (uint64_t)(uintptr_t)backend_storage;
    uint32_t index_bits = (uint32_t)index;
    uint32_t group_bits = (uint32_t)group;
    uint64_t guard_pointer = load_guard_pointer(imports);
    const uint64_t stride = (uint64_t)((int64_t)group * group_stride_bytes);
    _Alignas(16) borrowed_frame frame;
    write_quad(frame.guard_c0, read_quad((const void *)(uintptr_t)guard_pointer));
    const uint64_t divisor = (uint64_t)(int64_t)signed_word(read_word(GROUP_FIELD(backend, stride, alignment_1e4)));
    if (unsigned_remainder(first, divisor) != 0)
        return finish(imports, &frame, guard_pointer, alignment_error);
    write_quad(frame.second_18, second);
    const uint64_t length = (uint64_t)(int64_t)signed_word(read_word(GROUP_FIELD(backend, stride, length_1e0)));
    const uint32_t descriptor = read_word(BACKEND_FIELD(backend, descriptor_430));
    uint64_t token = read_quad(BACKEND_FIELD(backend, token_438));
    write_quad(frame.group_stride_08, stride);
    write_word(frame.descriptor_24, descriptor);
    const uint64_t first_end = first + length;
    write_quad(frame.length_10, length);
    if (first_end > first) {
        write_quad(frame.backend_38, backend);
        write_word(frame.mode_48, mode);
        write_word(frame.group_44, group_bits);
        write_quad(frame.token_30, token);
        write_word(frame.index_4c, index_bits);
        write_quad(frame.first_28, first);
        uint32_t count;
        const uint32_t error = walk_range(imports, &frame, first, first_end, 1, &count);
        /* First-walk exits refresh the guard pointer even on a query failure. */
        guard_pointer = load_guard_pointer(imports);
        if (error != 0) return finish(imports, &frame, guard_pointer, error);
        group_bits = read_word(frame.group_44);
        mode = read_word(frame.mode_48);
        first = read_quad(frame.first_28);
        backend = read_quad(frame.backend_38);
        index_bits = read_word(frame.index_4c);
        token = read_quad(frame.token_30);
        if (count != 0) increment_walk_count(backend);
    }
    second = read_quad(frame.second_18);
    if (mode != 0) {
        write_quad(frame.token_30, token);
        if (unsigned_remainder(second, divisor) != 0)
            return finish(imports, &frame, guard_pointer, alignment_error);
        const uint64_t second_end = read_quad(frame.length_10) + second;
        if (second_end > second) {
            write_quad(frame.backend_38, backend);
            write_word(frame.group_44, group_bits);
            write_word(frame.mode_48, mode);
            write_word(frame.index_4c, index_bits);
            write_quad(frame.first_28, first);
            uint32_t count;
            const uint32_t error = walk_range(imports, &frame, second, second_end, 0, &count);
            /* The second walk keeps the cached pointer across every query,
               including failures. It does not refresh the global guard cell. */
            if (error != 0) return finish(imports, &frame, guard_pointer, error);
            group_bits = read_word(frame.group_44);
            mode = read_word(frame.mode_48);
            second = read_quad(frame.second_18);
            first = read_quad(frame.first_28);
            backend = read_quad(frame.backend_38);
            index_bits = read_word(frame.index_4c);
            token = read_quad(frame.token_30);
            if (count != 0) increment_walk_count(backend);
        } else {
            token = read_quad(frame.token_30);
        }
    }
    write_quad(frame.backend_38, backend);
    const int32_t command_descriptor = signed_word(read_word(frame.descriptor_24));
    write_quad(COMMAND_FIELD(&frame, token_00), token);
    write_word(COMMAND_FIELD(&frame, index_08), index_bits);
    write_word(COMMAND_FIELD(&frame, group_0c), group_bits);
    write_quad(COMMAND_FIELD(&frame, first_10), first);
    write_quad(COMMAND_FIELD(&frame, second_18), second);
    write_quad(COMMAND_FIELD(&frame, mode_20), mode != 0);
    write_word(COMMAND_FIELD(&frame, zero_28), 0);
    const int32_t registered = load_ioctl(imports)(command_descriptor, registration_request, (void *)frame.workspace_78);
    if (registered < 0) {
        const void *error_cell = load_error(imports)();
        const uint32_t error = read_word(error_cell);
        guard_pointer = load_guard_pointer(imports);
        return finish(imports, &frame, guard_pointer, ioctl_error_result(error));
    }
    backend = read_quad(frame.backend_38);
    const uint64_t saved_stride = read_quad(frame.group_stride_08);
    const uint64_t signed_index = (uint64_t)(int64_t)signed_word(index_bits);
    const uint64_t group_base = backend + saved_stride;
    write_word((void *)(uintptr_t)(group_base + offsetof(group_anchors, index_active_150) + signed_index * word_bytes), 1);
    write_word((void *)(uintptr_t)(backend + offsetof(backend_anchors, index_group_08) + signed_index * word_bytes), group_bits);
    write_quad((void *)(uintptr_t)(backend + offsetof(backend_anchors, index_first_48) + signed_index * index_pair_bytes), first);
    write_quad((void *)(uintptr_t)(backend + offsetof(backend_anchors, index_second_50) + signed_index * index_pair_bytes), second);
    const uint64_t callback = read_quad(require_cell(imports->callback_target_slot, "VideoOut address callback target"));
    if (callback != 0) {
        write_word(frame.callback_50.event_00, registration_event);
        const uint32_t callback_word = read_word(BACKEND_FIELD(backend, callback_word_434));
        const uint64_t argument = read_quad(require_cell(imports->callback_argument_slot, "VideoOut address callback argument"));
        write_word(frame.callback_50.backend_word_04, callback_word);
        write_word(frame.callback_50.index_08, index_bits);
        write_quad(frame.callback_50.first_10, first);
        write_quad(frame.callback_50.second_18, second);
        const uint32_t callback_mode = mode == 1;
        write_word(frame.callback_50.mode_20, callback_mode);
        write_word(frame.callback_50.group_24, group_bits);
        if (!imports->callback_bridge)
            rt_fatal("VideoOut address callback bridge", "the actual opaque callback ABI has not been bound");
        imports->callback_bridge(callback, registration_event, (void *)&frame.callback_50,
                                 argument, callback, (uint64_t)callback_mode);
    }
    guard_pointer = load_guard_pointer(imports);
    return finish(imports, &frame, guard_pointer, 0);
}
