/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Output configuration retains live admission tables and partially staged
   provider packets. SDK spills, guard epochs and late stack input are visible
   storage; treating them as cached values changes alias-sensitive behavior. */
#include "runtime/video_out_output_config.h"
#include "runtime/process.h"

#include <stddef.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut output configuration requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
#define BRIDGE_WIDTH(type) _Static_assert(sizeof(type) == sizeof(uint64_t), #type " pointer")
BRIDGE_WIDTH(rt_video_out_output_sdk_fn);
BRIDGE_WIDTH(rt_video_out_output_diagnostic_fn);
BRIDGE_WIDTH(rt_video_out_output_av_fn);
BRIDGE_WIDTH(rt_video_out_output_stack_fail_fn);
#undef BRIDGE_WIDTH

const rt_video_out_output_config_bindings *volatile rt_video_out_output_config_imports;

typedef struct {
    uint8_t size_00[4];
    uint8_t variant_04, variant_05, variant_06, variant_07;
    uint8_t parameter_08[8], parameter_10[8];
    uint8_t variant_18, required_ff_19, required_ff_1a, required_ff_1b;
    uint8_t required_ff_1c[4];
} descriptor_anchors;

typedef struct {
    uint8_t outgoing_selector_00[8], unknown_08[8];
    uint8_t optional_spill_10[8], marker_spill_18[4], key_spill_1c[4];
    uint8_t size_or_sdk_20[4];
    uint8_t variant_24, variant_25, variant_26, variant_27;
    uint8_t pair_28[16];
    uint8_t tail_38, first_ff_39[3], overlapping_ff_3c[4];
    uint8_t optional_40[2], optional_42[2], optional_44[4];
    uint8_t optional_48[4], optional_4c[4], saved_guard_50[8];
} frame_anchors;

typedef struct {
    uint8_t signed_word_00[2], word_02[2], word_04[4], word_08[4], word_0c[4];
} optional_anchors;

typedef struct { uint8_t bytes[16]; } block16;

#define PIN(type, field, offset) _Static_assert(offsetof(type, field) == offset, #field " offset")
PIN(descriptor_anchors, size_00, 0x00);
PIN(descriptor_anchors, variant_04, 0x04);
PIN(descriptor_anchors, variant_05, 0x05);
PIN(descriptor_anchors, variant_06, 0x06);
PIN(descriptor_anchors, variant_07, 0x07);
PIN(descriptor_anchors, parameter_08, 0x08);
PIN(descriptor_anchors, parameter_10, 0x10);
PIN(descriptor_anchors, variant_18, 0x18);
PIN(descriptor_anchors, required_ff_19, 0x19);
PIN(descriptor_anchors, required_ff_1a, 0x1a);
PIN(descriptor_anchors, required_ff_1b, 0x1b);
PIN(descriptor_anchors, required_ff_1c, 0x1c);
PIN(frame_anchors, outgoing_selector_00, 0x00);
PIN(frame_anchors, optional_spill_10, 0x10);
PIN(frame_anchors, marker_spill_18, 0x18);
PIN(frame_anchors, key_spill_1c, 0x1c);
PIN(frame_anchors, size_or_sdk_20, 0x20);
PIN(frame_anchors, variant_24, 0x24);
PIN(frame_anchors, variant_25, 0x25);
PIN(frame_anchors, variant_26, 0x26);
PIN(frame_anchors, variant_27, 0x27);
PIN(frame_anchors, pair_28, 0x28);
PIN(frame_anchors, tail_38, 0x38);
PIN(frame_anchors, first_ff_39, 0x39);
PIN(frame_anchors, overlapping_ff_3c, 0x3c);
PIN(frame_anchors, optional_40, 0x40);
PIN(frame_anchors, optional_42, 0x42);
PIN(frame_anchors, optional_44, 0x44);
PIN(frame_anchors, optional_48, 0x48);
PIN(frame_anchors, optional_4c, 0x4c);
PIN(frame_anchors, saved_guard_50, 0x50);
PIN(optional_anchors, signed_word_00, 0x00);
PIN(optional_anchors, word_02, 0x02);
PIN(optional_anchors, word_04, 0x04);
PIN(optional_anchors, word_08, 0x08);
PIN(optional_anchors, word_0c, 0x0c);
#undef PIN
_Static_assert(sizeof(descriptor_anchors) == 32, "descriptor prefix");
_Static_assert(sizeof(optional_anchors) == 16, "optional prefix");
_Static_assert(sizeof(frame_anchors) == RT_VIDEO_OUT_OUTPUT_FRAME_BYTES, "frame extent");

static const uint32_t error_0001 = UINT32_C(0x80290001);
static const uint32_t error_0016 = UINT32_C(0x80290016);
static const uint32_t error_000b = UINT32_C(0x8029000b);
static const uint32_t error_00fe = UINT32_C(0x802900fe);
static const uint32_t sdk_threshold = UINT32_C(0x05500000);
static const uint32_t provider_index_adjustment = UINT32_C(0x7f65ffff);
static const uint16_t sign_bit_16 = UINT16_C(0x8000);
static const uint32_t sign_bit_32 = UINT32_C(0x80000000);
static const uint64_t sign_bit_64 = UINT64_C(0x8000000000000000);
static const uint64_t initial_parameter_mask = UINT64_C(0x1000004011);
static const uint64_t positive_level_mask = UINT64_C(0x41fc);
static const uint64_t extended_level_mask = UINT64_C(0x7bc00);
static const uint64_t pair_positive_mask = UINT64_C(0x1e4);
static const uint64_t pair_flag_mask = UINT64_C(0x19);
static const uint64_t negative_parameter_41 = UINT64_C(0xffffffff41ffffff);
static const uint64_t negative_parameter_81 = UINT64_C(0xffffffff81ffffff);
static const uint64_t negative_parameter_c1 = UINT64_C(0xffffffffc1ffffff);

enum {
    selector_8000 = 0x8000, selector_0010 = 0x10, extended_level = 2,
    first_variant_count = 4, second_variant_count = 2, tail_variant_count = 5,
    variant_07_max_index = 37, variant_06_max_index = 19,
    parameter_10_max_index = 194, provider_error_count = 11,
    parameter_pair_max_index = 8, initial_parameter_max_index = 36,
    generic_parameter_max_index = 18, optional_shift = 2, optional_low_mask = 3,
    flag_01 = 1, flag_02 = 2, flag_04 = 4, flag_08 = 8, flag_10 = 0x10,
    parameter_10_e0 = 0xe0, parameter_10_e1 = 0xe1,
    remap_a0 = 0xa0, remap_a1 = 0xa1, remap_a2 = 0xa2, remap_a3 = 0xa3,
    output_request = 0x7000, initial_flagged_parameter = 3,
    parameter_pair_restricted = 13, non_table_parameter_count = 2,
    signed_word16_modulus = 0x10000, optional_rounding_bias = 3
};

/* Per-site relative destinations. Only recovered landing identities are
   supported; an unfamiliar live displacement is never mapped to rejection. */
enum variant_07_action {
    variant_07_continue = -24124,
    variant_07_reject = -24155,
    variant_07_extended = -24161
};
enum variant_06_action {
    variant_06_continue = -24121,
    variant_06_reject = -24307,
    variant_06_sdk = -23754,
    variant_06_positive = -24163,
    variant_06_positive_flag = -24153,
    variant_06_diagnostic_flag = -23666,
    variant_06_extended_flag = -24143,
    variant_06_extended = -23633
};
enum parameter_10_action {
    parameter_10_continue = -24196,
    parameter_10_reject = -24387,
    parameter_10_positive_flag_08 = -23860,
    parameter_10_positive = -23551,
    parameter_10_pair = -23616,
    parameter_10_positive_flag_10 = -23537
};

static void access_boundary(void) { __asm__ volatile("" ::: "memory"); }

#define SCALAR_IO(name, type) \
    static type read_##name(uint64_t address) \
    { \
        type value; \
        access_boundary(); \
        memcpy(&value, (const void *)(uintptr_t)address, sizeof value); \
        __asm__ volatile("" : : "r"((uint64_t)value) : "memory"); \
        return value; \
    } \
    static void write_##name(uint64_t address, type value) \
    { \
        access_boundary(); \
        memcpy((void *)(uintptr_t)address, &value, sizeof value); \
        access_boundary(); \
    }
SCALAR_IO(byte, uint8_t)
SCALAR_IO(short_word, uint16_t)
SCALAR_IO(word, uint32_t)
SCALAR_IO(quad, uint64_t)
#undef SCALAR_IO

static block16 read_block(uint64_t address)
{
    block16 value;
    access_boundary();
    memcpy(&value, (const void *)(uintptr_t)address, sizeof value);
    __asm__ volatile("" : : "m"(value) : "memory");
    return value;
}

static void write_block(uint64_t address, const block16 *value)
{
    access_boundary();
    memcpy((void *)(uintptr_t)address, value, sizeof *value);
    access_boundary();
}

static uint64_t require_address(const void *storage, const char *name)
{
    if (!storage) rt_fatal(name, "the required actual storage or import cell is unbound");
    return (uint64_t)(uintptr_t)storage;
}

static int32_t signed_word(uint32_t value)
{
    int32_t result;
    memcpy(&result, &value, sizeof result);
    return result;
}

static int32_t dispatch(const void *storage, uint64_t index, const char *name)
{
    return signed_word(read_word(require_address(storage, name) + index * sizeof(uint32_t)));
}

static uint64_t guard_pointer(const rt_video_out_output_config_bindings *imports)
{
    return read_quad(require_address(imports->guard_pointer_slot, "VideoOut output guard pointer cell"));
}

static uint64_t invoke_sdk(const rt_video_out_output_config_bindings *imports, uint64_t output)
{
    const uint64_t target = read_quad(require_address(imports->sdk_slot, "VideoOut output SDK import"));
    if (!imports->invoke_sdk) rt_fatal("VideoOut output SDK bridge", "the genuine provider bridge is unbound");
    return imports->invoke_sdk(target, (void *)(uintptr_t)output);
}

static void diagnostic(const rt_video_out_output_config_bindings *imports)
{
    const char *message = (const char *)(uintptr_t)require_address(
        imports->blocked_configuration_message, "VideoOut output diagnostic literal");
    const uint64_t target = read_quad(require_address(imports->diagnostic_slot, "VideoOut output puts import"));
    if (!imports->invoke_diagnostic) rt_fatal("VideoOut output puts bridge", "the genuine provider bridge is unbound");
    imports->invoke_diagnostic(target, message);
}

static uint64_t finish(const rt_video_out_output_config_bindings *imports,
    uint64_t frame, uint64_t chosen_guard, uint32_t status)
{
    const uint64_t current = read_quad(chosen_guard);
    const uint64_t saved = read_quad(frame + offsetof(frame_anchors, saved_guard_50));
    if (current != saved) {
        const uint64_t target = read_quad(require_address(imports->stack_fail_slot, "VideoOut output stack-fail import"));
        if (!imports->invoke_stack_fail) rt_fatal("VideoOut output stack-fail bridge", "the genuine provider bridge is unbound");
        imports->invoke_stack_fail(target, chosen_guard);
#if defined(__x86_64__)
        __asm__ volatile("ud2" ::: "memory");
        __builtin_unreachable();
#else
        __builtin_trap();
#endif
    }
    return (uint64_t)status;
}

uint64_t rt_video_out_configure_output_with_storage(uint64_t selector_bits,
    uint64_t descriptor_address, uint64_t optional_address, uint64_t level_bits,
    uint64_t flag_bits, uint64_t key_bits, const void *incoming_stack_word,
    void *frame_storage)
{
    const rt_video_out_output_config_bindings *imports = rt_video_out_output_config_imports;
    if (!imports) rt_fatal("VideoOut output configuration", "actual module bindings have not been supplied");
    const uint64_t frame = require_address(frame_storage, "VideoOut output borrowed frame");
    const uint32_t selector = (uint32_t)selector_bits;
    const int32_t level = signed_word((uint32_t)level_bits);
    const uint32_t flags = (uint32_t)flag_bits;
    uint32_t key = (uint32_t)key_bits;
    uint64_t chosen_guard = guard_pointer(imports);
    write_quad(frame + offsetof(frame_anchors, saved_guard_50), read_quad(chosen_guard));
    uint32_t status = error_0001;
    if (selector != 0 && selector != selector_8000 && selector != selector_0010)
        goto done;
    if (descriptor_address == 0)
        goto done;

    const uint32_t size = read_word(descriptor_address + offsetof(descriptor_anchors, size_00));
    status = error_0016;
    if (size != sizeof(descriptor_anchors)) goto done;

    uint8_t variant = read_byte(descriptor_address + offsetof(descriptor_anchors, variant_04));
    if ((uint8_t)(variant - 1) < first_variant_count) {
        if (level < extended_level) goto done;
    } else if (variant != UINT8_MAX) goto done;
    variant = read_byte(descriptor_address + offsetof(descriptor_anchors, variant_05));
    if ((uint8_t)(variant - 1) < second_variant_count) {
        if (level < extended_level) goto done;
    } else if (variant != UINT8_MAX) goto done;

    uint32_t index = (uint8_t)(read_byte(descriptor_address +
        offsetof(descriptor_anchors, variant_07)) + 1);
    if (index > variant_07_max_index) goto done;
    switch (dispatch(imports->variant_07_table, index, "VideoOut output variant07 table")) {
    case variant_07_reject: goto done;
    case variant_07_extended: if (level < extended_level) goto done; break;
    case variant_07_continue: break;
    default: rt_fatal("VideoOut output variant07 target", "unrecovered live dispatch target");
    }

    const uint64_t initial_x = read_quad(descriptor_address + offsetof(descriptor_anchors, parameter_08));
    const uint64_t initial_index = initial_x + 1;
    uint32_t marker = 0;
    if (initial_index <= initial_parameter_max_index &&
        ((initial_parameter_mask >> initial_index) & 1) != 0) {
        const uint64_t initial_y = read_quad(descriptor_address + offsetof(descriptor_anchors, parameter_10));
        if (initial_y == negative_parameter_c1 || initial_y == parameter_10_e1) {
            if (initial_y == parameter_10_e1 && level <= 0) goto done;
            marker = 1;
            if ((initial_x == initial_flagged_parameter || initial_x == UINT64_MAX) && (flags & flag_01) == 0)
                goto done;
        }
    }

    index = (uint8_t)(read_byte(descriptor_address + offsetof(descriptor_anchors, variant_06)) + 1);
    if (index > variant_06_max_index) goto done;
    switch (dispatch(imports->variant_06_table, index, "VideoOut output variant06 table")) {
    case variant_06_reject: goto done;
    case variant_06_continue: break;
    case variant_06_positive: if (level <= 0) goto done; break;
    case variant_06_positive_flag:
        if (level <= 0 || (flags & flag_02) == 0) goto done;
        break;
    case variant_06_extended_flag:
        if (level < extended_level || (flags & flag_02) == 0) goto done;
        break;
    case variant_06_extended: if (level < extended_level) goto done; break;
    case variant_06_diagnostic_flag:
        if ((flags & flag_02) == 0) { diagnostic(imports); goto done; }
        break;
    case variant_06_sdk: {
        write_word(frame + offsetof(frame_anchors, size_or_sdk_20), 0);
        write_word(frame + offsetof(frame_anchors, key_spill_1c), key);
        write_quad(frame + offsetof(frame_anchors, optional_spill_10), optional_address);
        write_word(frame + offsetof(frame_anchors, marker_spill_18), marker);
        const uint32_t query = (uint32_t)invoke_sdk(imports, frame + offsetof(frame_anchors, size_or_sdk_20));
        marker = read_word(frame + offsetof(frame_anchors, marker_spill_18));
        optional_address = read_quad(frame + offsetof(frame_anchors, optional_spill_10));
        key = read_word(frame + offsetof(frame_anchors, key_spill_1c));
        chosen_guard = guard_pointer(imports);
        /* The native SDK DWORD read is unconditional, even on query error. */
        const uint32_t sdk = read_word(frame + offsetof(frame_anchors, size_or_sdk_20));
        const int32_t required = extended_level - (int32_t)((query != 0) | (sdk < sdk_threshold));
        if (level < required) goto done;
        break;
    }
    default: rt_fatal("VideoOut output variant06 target", "unrecovered live dispatch target");
    }

    if (marker == 0) {
        const uint64_t x_index = read_quad(descriptor_address + offsetof(descriptor_anchors, parameter_08)) + 1;
        if (x_index > generic_parameter_max_index) goto done;
        if (((positive_level_mask >> x_index) & 1) != 0) {
            if (level <= 0) goto done;
        } else if (((extended_level_mask >> x_index) & 1) != 0) {
            if (level < extended_level) goto done;
        } else if (x_index != 0) goto done;

        const uint64_t y = read_quad(descriptor_address + offsetof(descriptor_anchors, parameter_10));
        if ((y & sign_bit_64) != 0 && y != UINT64_MAX) {
            if (y != negative_parameter_41 && y != negative_parameter_81 && y != negative_parameter_c1)
                goto done;
        } else {
            const uint64_t y_index = y + 1;
            if (y_index > parameter_10_max_index) {
                if (y - parameter_10_e0 >= non_table_parameter_count || level <= 0) goto done;
            } else switch (dispatch(imports->parameter_10_table, y_index, "VideoOut output parameter10 table")) {
            case parameter_10_reject: goto done;
            case parameter_10_continue: break;
            case parameter_10_positive: if (level <= 0) goto done; break;
            case parameter_10_positive_flag_08:
                if (level <= 0 || (flags & flag_08) == 0) goto done;
                break;
            case parameter_10_positive_flag_10:
                if (level <= 0 || (flags & flag_10) == 0) goto done;
                break;
            case parameter_10_pair:
                if (level <= 1 && y == parameter_pair_restricted) goto done;
                if (x_index <= parameter_pair_max_index &&
                    (((pair_positive_mask >> x_index) & 1) != 0 ||
                    (((pair_flag_mask >> x_index) & 1) != 0 && (flags & flag_04) != 0))) {
                    if (level <= 0) goto done;
                } else if (level < extended_level) goto done;
                break;
            default: rt_fatal("VideoOut output parameter10 target", "unrecovered live dispatch target");
            }
        }
    }

    const uint8_t tail = read_byte(descriptor_address + offsetof(descriptor_anchors, variant_18));
    if (tail < tail_variant_count) { if (level <= 0) goto done; }
    else if (tail != UINT8_MAX) goto done;
    if (read_byte(descriptor_address + offsetof(descriptor_anchors, required_ff_19)) != UINT8_MAX) goto done;
    if (read_byte(descriptor_address + offsetof(descriptor_anchors, required_ff_1a)) != UINT8_MAX) goto done;
    if (read_byte(descriptor_address + offsetof(descriptor_anchors, required_ff_1b)) != UINT8_MAX) goto done;
    if (read_word(descriptor_address + offsetof(descriptor_anchors, required_ff_1c)) != UINT32_MAX) goto done;
    const uint32_t existing_key = read_word(require_address(imports->key_word, "VideoOut output key word"));
    if (existing_key != key) {
        status = error_000b;
        if (existing_key != 0) goto done;
    }

    write_word(frame + offsetof(frame_anchors, size_or_sdk_20), sizeof(descriptor_anchors));
    /* chosen_guard is now the pointer captured by the native commit stage. */
    write_word(frame + offsetof(frame_anchors, key_spill_1c), key);
    write_byte(frame + offsetof(frame_anchors, variant_24),
        read_byte(descriptor_address + offsetof(descriptor_anchors, variant_04)));
    write_byte(frame + offsetof(frame_anchors, variant_25),
        read_byte(descriptor_address + offsetof(descriptor_anchors, variant_05)));
    write_byte(frame + offsetof(frame_anchors, variant_26),
        read_byte(descriptor_address + offsetof(descriptor_anchors, variant_06)));
    write_byte(frame + offsetof(frame_anchors, variant_27),
        read_byte(descriptor_address + offsetof(descriptor_anchors, variant_07)));
    const block16 pair = read_block(descriptor_address + offsetof(descriptor_anchors, parameter_08));
    write_block(frame + offsetof(frame_anchors, pair_28), &pair);
    write_byte(frame + offsetof(frame_anchors, tail_38), tail);
    write_word(frame + offsetof(frame_anchors, first_ff_39), UINT32_MAX);
    write_word(frame + offsetof(frame_anchors, overlapping_ff_3c), UINT32_MAX);
    if ((flags & flag_10) != 0) {
        uint64_t high;
        memcpy(&high, pair.bytes + sizeof(uint64_t), sizeof high);
        if (high == remap_a0) write_quad(frame + offsetof(frame_anchors, pair_28) + sizeof(uint64_t), remap_a2);
        else if (high == remap_a1) write_quad(frame + offsetof(frame_anchors, pair_28) + sizeof(uint64_t), remap_a3);
    }

    rt_video_out_output_native_call call;
    if (optional_address == 0) {
        const block16 zero = {{0}};
        write_block(frame + offsetof(frame_anchors, optional_40), &zero);
        memset(call.ymm0, 0, sizeof call.ymm0);
    } else {
        const uint16_t raw = read_short_word(optional_address + offsetof(optional_anchors, signed_word_00));
        const int32_t value = (raw & sign_bit_16) ? (int32_t)raw - signed_word16_modulus : (int32_t)raw;
        status = error_0001;
        if ((raw & optional_low_mask) != 0) goto done;
        const uint32_t rounded = (uint32_t)(value >= 0 ? value : value + optional_rounding_bias);
        write_short_word(frame + offsetof(frame_anchors, optional_40), (uint16_t)(rounded >> optional_shift));
        write_short_word(frame + offsetof(frame_anchors, optional_42),
            read_short_word(optional_address + offsetof(optional_anchors, word_02)));
        write_word(frame + offsetof(frame_anchors, optional_44),
            read_word(optional_address + offsetof(optional_anchors, word_04)));
        write_word(frame + offsetof(frame_anchors, optional_48),
            read_word(optional_address + offsetof(optional_anchors, word_08)));
        write_word(frame + offsetof(frame_anchors, optional_4c),
            read_word(optional_address + offsetof(optional_anchors, word_0c)));
        /* The packet remap did not change the native vector snapshot. */
        memcpy(call.ymm0, pair.bytes, sizeof pair);
        memset(call.ymm0 + sizeof pair, 0, sizeof pair);
    }
    call.rdi = output_request; call.rsi = 0; call.rdx = 0;
    call.rcx = frame + offsetof(frame_anchors, size_or_sdk_20);
    call.r8 = 0; call.r9 = frame + offsetof(frame_anchors, optional_40);
    call.rax = (uint64_t)selector;
    call.stack_slot = frame + offsetof(frame_anchors, outgoing_selector_00);
    status = 0;
    write_quad(call.stack_slot, (uint64_t)selector);
    const uint64_t target = read_quad(require_address(imports->av_slot, "VideoOut output AV import"));
    if (!imports->invoke_av) rt_fatal("VideoOut output AV bridge", "the genuine provider bridge is unbound");
    const uint32_t result = (uint32_t)imports->invoke_av(target, &call);
    if ((result & sign_bit_32) == 0) {
        const uint32_t stack_value = read_word(require_address(incoming_stack_word, "VideoOut output incoming stack word"));
        const uint32_t published_key = read_word(frame + offsetof(frame_anchors, key_spill_1c));
        write_word(require_address(imports->key_word, "VideoOut output key word"), published_key);
        write_word(require_address(imports->stack_word, "VideoOut output stack word"), stack_value);
    } else {
        const uint32_t error_index = result + provider_index_adjustment;
        status = error_00fe;
        if (error_index < provider_error_count)
            status = read_word(require_address(imports->provider_error_table,
                "VideoOut output provider error table") + (uint64_t)error_index * sizeof(uint32_t));
    }

done:
    return finish(imports, frame, chosen_guard, status);
}
