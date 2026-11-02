/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Build VideoOut's pair of internal coefficient records.

   Static reading of the complete 3586-byte internal function at module offset
   0x7940 in libSceVideoOut, SHA-256
   33a6f16349f07637e168db0d6338f0b2ae8aea626b86bc3d28325ecb5155e2c1,
   establishes the nine captured source words, wrapped arithmetic, ordered
   comparisons, two numerical passes and publication through the record packer.
   These numerical limits describe the original calculation, without assigning
   names to an unestablished hardware format or service policy.

   The host port requires explicit live import cells. It never supplies a default
   canary or pretends the loader has bound one. The selected guard pointer is
   retained across calls at the original epochs, then its current value is read
   before the saved value is compared. Imports and helper results are not statuses
   invented to advance boot. This source remains uncompiled and unvalidated.

   Ordinary initialized accessible backing is required. Only the stored scalar
   results are modeled: unused SIMD lanes, full registers/flags, FP traps/status,
   DAZ/FTZ/NaN handling across ISAs, host math/import accuracy, exact fault progress
   and aliases into the physical native stack frame are not equivalence claims. */
#include "runtime/video_out_records.h"
#include "runtime/video_out_transform.h"
#include "runtime/video_out_coefficients.h"
#include "runtime/process.h"

#include <float.h>
#include <limits.h>
#include <string.h>

#if !defined(__x86_64__) && !defined(__aarch64__)
#error "VideoOut records require an x86-64 SSE4.1 or ARM64 scalar FP port"
#endif
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut record storage currently requires a little-endian host"
#endif
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(float) == sizeof(uint32_t) && FLT_RADIX == 2 &&
               FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128, "binary32 floats");

_Static_assert(sizeof(rt_video_out_clear_fn) == sizeof(uint64_t), "clear pointer representation");
_Static_assert(sizeof(rt_video_out_copy_fn) == sizeof(uint64_t), "copy pointer representation");
_Static_assert(sizeof(rt_video_out_print_fn) == sizeof(uint64_t), "print pointer representation");
_Static_assert(sizeof(rt_video_out_stack_fail_fn) == sizeof(uint64_t), "fail pointer representation");

const rt_video_out_record_bindings *volatile rt_video_out_record_imports;

enum {
    word_bytes = sizeof(uint32_t),
    row_words = 10,
    full_row_words = 8,
    vector_words = 4,
    copied_stage_bytes = 0x29c,
    quantized_bank = 3,
    first_output_offset = 0x20,
    second_output_offset = 0x180,
    parameter_error = 0x80290001u,
    fixed_ratio_limit = 0x200000,
    fixed_fraction_shift = 5,
    coefficient_shift = 2,
    coefficient_limit = 8191,
    negative_coefficient_base = 16384,
    first_tap_limit = 10,
    second_tap_limit = 6,
    required_word_limit = 65500,
};

/* Byte fields are wire-layout anchors; no typed float/int guest dereferences. */
typedef struct {
    uint8_t unknown_00[0x10];
    uint8_t first_numerator_10[word_bytes];
    uint8_t second_numerator_14[word_bytes];
    uint8_t first_denominator_18[word_bytes];
    uint8_t second_denominator_1c[word_bytes];
    uint8_t kind_20[word_bytes];
    uint8_t ceiling_24[word_bytes];
    uint8_t increment_28[word_bytes];
    uint8_t divisor_2c[word_bytes];
    uint8_t flags_30[word_bytes];
} source_anchors;

typedef struct {
    uint8_t ratio_00[word_bytes];
    uint8_t control_04[word_bytes];
    uint8_t mode_08[word_bytes];
    uint8_t discriminator_0c[word_bytes];
    uint8_t second_taps_10[word_bytes];
    uint8_t first_taps_14[word_bytes];
    uint8_t required_float_18[word_bytes];
    uint8_t required_word_1c[word_bytes];
    uint8_t window_20[word_bytes];
    uint8_t window_24[word_bytes];
} parameter_anchors;

typedef struct {
    rt_video_out_transform_staging_prefix prefix;
    uint8_t window_298[word_bytes];
} complete_staging;

typedef struct {
    complete_staging stages[2];
    uint8_t ready_538;
    uint8_t unknown_539[11];
} copied_stages;

typedef struct {
    uint8_t integer_284[word_bytes];
    uint8_t fraction_288[word_bytes];
    uint8_t second_integer_28c[word_bytes];
    uint8_t second_fraction_290[word_bytes];
} staging_tail;

_Static_assert(offsetof(source_anchors, first_numerator_10) == 0x10, "first numerator");
_Static_assert(offsetof(source_anchors, flags_30) == 0x30, "source flags");
_Static_assert(offsetof(parameter_anchors, required_float_18) == 0x18, "required float");
_Static_assert(offsetof(parameter_anchors, window_24) == 0x24, "window cap");
_Static_assert(sizeof(complete_staging) == copied_stage_bytes, "complete staging");
_Static_assert(offsetof(complete_staging, window_298) == 0x298, "last staging word");
_Static_assert(offsetof(copied_stages, ready_538) == 0x538, "copy ready byte");
_Static_assert(sizeof(copied_stages) == 0x544, "clear size");
_Static_assert(sizeof(staging_tail) == RT_VIDEO_OUT_TRANSFORM_TAIL_BYTES, "tail size");

_Static_assert(offsetof(source_anchors, second_numerator_14) == 0x14, "source second_numerator_14");
_Static_assert(offsetof(source_anchors, first_denominator_18) == 0x18, "source first_denominator_18");
_Static_assert(offsetof(source_anchors, second_denominator_1c) == 0x1c, "source second_denominator_1c");
_Static_assert(offsetof(source_anchors, kind_20) == 0x20, "source kind_20");
_Static_assert(offsetof(source_anchors, ceiling_24) == 0x24, "source ceiling_24");
_Static_assert(offsetof(source_anchors, increment_28) == 0x28, "source increment_28");
_Static_assert(offsetof(source_anchors, divisor_2c) == 0x2c, "source divisor_2c");
_Static_assert(offsetof(parameter_anchors, ratio_00) == 0x00, "parameter ratio_00");
_Static_assert(offsetof(parameter_anchors, control_04) == 0x04, "parameter control_04");
_Static_assert(offsetof(parameter_anchors, mode_08) == 0x08, "parameter mode_08");
_Static_assert(offsetof(parameter_anchors, discriminator_0c) == 0x0c, "parameter discriminator_0c");
_Static_assert(offsetof(parameter_anchors, second_taps_10) == 0x10, "parameter second_taps_10");
_Static_assert(offsetof(parameter_anchors, first_taps_14) == 0x14, "parameter first_taps_14");
_Static_assert(offsetof(parameter_anchors, required_word_1c) == 0x1c, "parameter required_word_1c");
_Static_assert(offsetof(parameter_anchors, window_20) == 0x20, "parameter window_20");
_Static_assert(offsetof(staging_tail, integer_284) == 0, "tail integer");
_Static_assert(offsetof(staging_tail, fraction_288) == 4, "tail fraction");
_Static_assert(offsetof(staging_tail, second_integer_28c) == 8, "second tail integer");
_Static_assert(offsetof(staging_tail, second_fraction_290) == 12, "second tail fraction");
_Static_assert(second_output_offset - first_output_offset == sizeof(rt_video_out_transform_packed),
               "adjacent packed records");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static uint32_t read_word(uint64_t address)
{
    uint32_t word;
    access_boundary();
    memcpy(&word, (const void *)(uintptr_t)address, sizeof word);
    access_boundary();
    return word;
}

static uint64_t read_quad(const void *storage)
{
    uint64_t word;
    access_boundary();
    memcpy(&word, storage, sizeof word);
    access_boundary();
    return word;
}

static void write_word(void *storage, uint32_t word)
{
    access_boundary();
    memcpy(storage, &word, sizeof word);
    access_boundary();
}

static float float_word(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}
static int32_t signed_word(uint32_t bits)
{
    int32_t value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
static float read_float(const void *p) { return float_word(read_word((uint64_t)(uintptr_t)p)); }
static void write_float(void *p, float value) { write_word(p, float_bits(value)); }

static float fp_add(float left, float right)
{
#if defined(__x86_64__)
    __asm__ volatile("addss %1, %0" : "+x"(left) : "x"(right) : "memory");
    return left;
#else
    float result;
    __asm__ volatile("fadd %s0, %s1, %s2" : "=w"(result) : "w"(left), "w"(right) : "memory");
    return result;
#endif
}

static float fp_subtract(float left, float right)
{
#if defined(__x86_64__)
    __asm__ volatile("subss %1, %0" : "+x"(left) : "x"(right) : "memory");
    return left;
#else
    float result;
    __asm__ volatile("fsub %s0, %s1, %s2" : "=w"(result) : "w"(left), "w"(right) : "memory");
    return result;
#endif
}

static float fp_multiply(float left, float right)
{
#if defined(__x86_64__)
    __asm__ volatile("mulss %1, %0" : "+x"(left) : "x"(right) : "memory");
    return left;
#else
    float result;
    __asm__ volatile("fmul %s0, %s1, %s2" : "=w"(result) : "w"(left), "w"(right) : "memory");
    return result;
#endif
}

static float fp_divide(float left, float right)
{
#if defined(__x86_64__)
    __asm__ volatile("divss %1, %0" : "+x"(left) : "x"(right) : "memory");
    return left;
#else
    float result;
    __asm__ volatile("fdiv %s0, %s1, %s2" : "=w"(result) : "w"(left), "w"(right) : "memory");
    return result;
#endif
}

static float fp_from_signed(int32_t value)
{
#if defined(__x86_64__)
    float result = 0.0f;
    __asm__ volatile("cvtsi2ss %1, %0" : "+x"(result) : "r"(value) : "memory");
#else
    float result;
    __asm__ volatile("scvtf %s0, %w1" : "=w"(result) : "r"(value) : "memory");
#endif
    return result;
}

static int32_t fp_truncate_signed(float value)
{
    int32_t result;
#if defined(__x86_64__)
    __asm__ volatile("cvttss2si %1, %0" : "=r"(result) : "x"(value) : "memory");
#else
    __asm__ volatile("fcvtzs %w0, %s1" : "=r"(result) : "w"(value) : "memory");
    const uint32_t sign_bit = UINT32_C(0x80000000);
    const uint32_t magnitude_mask = UINT32_C(0x7fffffff);
    const uint32_t signed_limit = UINT32_C(0x4f000000);
    const uint32_t bits = float_bits(value);
    const uint32_t magnitude = bits & magnitude_mask;
    /* FCVTZS already executed. Correct its NaN/saturation result, preserving
       -2^31 as a valid input, without a C out-of-range float-to-int cast. */
    if (magnitude > signed_limit ||
        (magnitude == signed_limit && (bits & sign_bit) == 0))
        result = INT32_MIN;
#endif
    return result;
}



static float fp_down(float value)
{
#if defined(__x86_64__)
    __asm__ volatile("roundss $9, %0, %0" : "+x"(value) : : "memory");
    return value;
#else
    float result;
    __asm__ volatile("frintm %s0, %s1" : "=w"(result) : "w"(value) : "memory");
    return result;
#endif
}

static float fp_up(float value)
{
#if defined(__x86_64__)
    __asm__ volatile("roundss $10, %0, %0" : "+x"(value) : : "memory");
    return value;
#else
    float result;
    __asm__ volatile("frintp %s0, %s1" : "=w"(result) : "w"(value) : "memory");
    return result;
#endif
}

typedef struct { uint8_t above, below, equal, unordered; } fp_relation;
static fp_relation fp_compare(float left, float right)
{
    fp_relation r;
#if defined(__x86_64__)
    __asm__ volatile("ucomiss %5, %4\n\tseta %0\n\tsetb %1\n\tsete %2\n\tsetp %3"
                     : "=q"(r.above), "=q"(r.below), "=q"(r.equal), "=q"(r.unordered)
                     : "x"(left), "x"(right) : "cc", "memory");
#else
    uint32_t above, below, equal, unordered;
    __asm__ volatile("fcmp %s4, %s5\n\tcset %w0, gt\n\tcset %w1, cc\n\tcset %w2, eq\n\tcset %w3, vs"
                     : "=r"(above), "=r"(below), "=r"(equal), "=r"(unordered)
                     : "w"(left), "w"(right) : "cc", "memory");
    r.above = (uint8_t)above; r.unordered = (uint8_t)unordered;
    r.below = (uint8_t)(below || unordered);
    r.equal = (uint8_t)(equal || unordered);
#endif
    return r;
}

static float fp_min(float left, float right)
{
#if defined(__x86_64__)
    __asm__ volatile("minss %1, %0" : "+x"(left) : "x"(right) : "memory");
    return left;
#else
    const fp_relation r = fp_compare(left, right);
    return r.below && !r.unordered ? left : right;
#endif
}

static float fp_max(float left, float right)
{
#if defined(__x86_64__)
    __asm__ volatile("maxss %1, %0" : "+x"(left) : "x"(right) : "memory");
    return left;
#else
    return fp_compare(left, right).above ? left : right;
#endif
}

static float from_word(uint32_t bits) { return fp_from_signed(signed_word(bits)); }
static uint32_t trunc_word(float value) { return (uint32_t)fp_truncate_signed(value); }
static uint32_t signed_min(uint32_t a, uint32_t b) { return signed_word(a) < signed_word(b) ? a : b; }

#define SOURCE_WORD(base, field) read_word((base) + offsetof(source_anchors, field))
#define PARAMETER(base, field) ((uint8_t *)(base) + offsetof(parameter_anchors, field))

static uint64_t guard_pointer(const rt_video_out_record_bindings *imports)
{
    if (!imports->guard_pointer_slot)
        rt_fatal("VideoOut __stack_chk_guard", "the guard pointer import cell is unbound");
    return read_quad(imports->guard_pointer_slot);
}

static void clear_stages(const rt_video_out_record_bindings *imports, copied_stages *copies)
{
    if (!imports->clear_slot)
        rt_fatal("VideoOut memset", "the function import cell is unbound");
    rt_video_out_clear_fn call;
    access_boundary();
    memcpy(&call, imports->clear_slot, sizeof call);
    access_boundary();
    if (!call)
        rt_fatal("VideoOut memset", "the function import target is unbound");
    (void)call(copies, 0, sizeof *copies);
    access_boundary();
}

static void copy_stage(const rt_video_out_record_bindings *imports, void *out, const void *in)
{
    if (!imports->copy_slot)
        rt_fatal("VideoOut memcpy", "the function import cell is unbound");
    rt_video_out_copy_fn call;
    access_boundary();
    memcpy(&call, imports->copy_slot, sizeof call);
    access_boundary();
    if (!call)
        rt_fatal("VideoOut memcpy", "the function import target is unbound");
    (void)call(out, in, copied_stage_bytes);
    access_boundary();
}

static void print_invalid(const rt_video_out_record_bindings *imports, float value)
{
    if (!imports->print_slot)
        rt_fatal("VideoOut printf", "the function import cell is unbound");
    rt_video_out_print_fn call;
    access_boundary();
    memcpy(&call, imports->print_slot, sizeof call);
    access_boundary();
    if (!call)
        rt_fatal("VideoOut printf", "the function import target is unbound");
    (void)call("invalid ratio %f\n", (double)value);
    access_boundary();
}

static uint64_t guarded_return(const rt_video_out_record_bindings *imports,
                               uint64_t pointer, uint64_t saved, uint32_t result)
{
    const uint64_t current = read_quad((const void *)(uintptr_t)pointer);
    access_boundary();
    if (current != saved) {
        if (!imports->stack_fail_slot)
            rt_fatal("VideoOut __stack_chk_fail", "the function import cell is unbound");
        rt_video_out_stack_fail_fn call;
        memcpy(&call, imports->stack_fail_slot, sizeof call);
        access_boundary();
        if (!call)
            rt_fatal("VideoOut __stack_chk_fail", "the function import target is unbound");
        call();
#if defined(__x86_64__)
        __asm__ volatile("ud2" ::: "memory");
#else
        __asm__ volatile("brk #0" ::: "memory");
#endif
        __builtin_unreachable();
    }
    return result;
}

static uint32_t ratio_mode(const rt_video_out_record_bindings *imports, float value)
{
    const fp_relation zero = fp_compare(0.0f, value);
    if (!zero.below) {
        print_invalid(imports, value);
        return 0;
    }
    const uint32_t mode = 2u - fp_compare(1.0f, value).above;
    const fp_relation one = fp_compare(value, 1.0f);
    return !one.equal || one.unordered ? mode : 0;
}

typedef struct { uint32_t factor, rows, subtract; } ratio_band;
static int second_ratio_band(float ratio, ratio_band *band)
{
    *band = (ratio_band){2, 2, 1};
    if (!fp_compare(1.0f, ratio).below) return 1;
    if (fp_compare(ratio, 1.0f).above && !fp_compare(float_word(0x3faaa993), ratio).below) {
        *band = (ratio_band){4, 3, 1}; return 1;
    }
    if (fp_compare(ratio, float_word(0x3faaa993)).above && !fp_compare(1.5f, ratio).below) {
        *band = (ratio_band){6, 4, 1}; return 1;
    }
    *band = (ratio_band){2, 1, 0};
    if (fp_compare(ratio, 1.5f).above && !fp_compare(2.0f, ratio).below) return 1;
    if (fp_compare(ratio, 2.0f).above && !fp_compare(3.0f, ratio).below) {
        band->factor = 3; return 1;
    }
    if (fp_compare(ratio, 3.0f).above && fp_compare(4.0f, ratio).above) {
        band->factor = 4; return 1;
    }
    return 0;
}

/* Keep the three nearby 4/3 constants distinct: they gate different paths. */
static float choose_scale(float numerator, float denominator, float span_ratio,
                          float increment, float budget, float weighted_span)
{
    const float weighted_four = fp_multiply(weighted_span, 4.0f);
    if (!fp_compare(denominator, fp_multiply(numerator, 0.25f)).below &&
        fp_compare(budget, weighted_four).above) return 4.0f;
    if (!fp_compare(denominator, fp_divide(numerator, 3.0f)).below &&
        fp_compare(budget, fp_multiply(weighted_span, 3.0f)).above) return 3.0f;
    if (!fp_compare(denominator, fp_multiply(numerator, 0.5f)).below &&
        fp_compare(budget, fp_add(weighted_span, weighted_span)).above) return 2.0f;
    const float divided = fp_divide(numerator, 1.5f);
    if (!fp_compare(denominator, divided).below) {
        float adjusted = fp_multiply(span_ratio, divided);
        const float weighted_six = fp_multiply(weighted_span, 6.0f);
        adjusted = fp_divide(adjusted, increment);
        adjusted = fp_add(adjusted, 3.0f);
        adjusted = fp_multiply(adjusted, budget);
        if (fp_compare(adjusted, weighted_six).above) return 1.5f;
    }
    float adjusted = fp_divide(numerator, float_word(0x3faaa993));
    const float cutoff = fp_divide(numerator, float_word(0x3faaaa8f));
    adjusted = fp_multiply(span_ratio, adjusted);
    const fp_relation domain = fp_compare(cutoff, denominator);
    adjusted = fp_divide(adjusted, increment);
    adjusted = fp_add(adjusted, 1.0f);
    adjusted = fp_multiply(adjusted, budget);
    const fp_relation weight = fp_compare(weighted_four, adjusted);
    float scale = !weight.below || weight.unordered ? 1.0f : float_word(0x3faaa993);
    if (domain.above || domain.unordered) scale = 1.0f;
    return scale;
}

typedef struct { float word, fraction; } required_metrics;
static required_metrics metrics_without_flag(uint32_t increment_word, uint32_t divisor,
        uint32_t units, uint32_t first_taps, uint32_t second_taps,
        uint32_t span, ratio_band band, float first_ratio, float second_ratio,
        float first_denominator, float span_ratio, float span_float, float rate)
{
    const uint32_t row_term = (band.rows - band.subtract) * increment_word;
    const uint32_t span_term = band.factor * span;
    const float denominator = fp_add(fp_multiply(span_ratio, first_denominator), from_word(row_term));
    const float numerator = fp_multiply(float_word(0x3f866666), from_word(span_term));
    float tap_floor = fp_up(fp_multiply(from_word(first_taps), 0.25f));
    const float tap_float = from_word(second_taps);
    float phase = fp_add(second_ratio, tap_float);
    const float divided_taps = fp_divide(tap_float, 3.0f);
    phase = fp_multiply(0.5f, fp_add(phase, 1.0f));
    phase = fp_down(phase);
    phase = fp_multiply(0.5f, fp_add(phase, 1.0f));
    phase = fp_up(phase);
    phase = fp_add(phase, phase);
    const float tap_bound = fp_multiply(first_ratio, fp_max(1.0f, divided_taps));
    tap_floor = fp_max(tap_floor, tap_bound);
    float other = fp_multiply(span_float, phase);
    other = fp_multiply(other, float_word(0x3f866666));
    const float ratios = fp_max(fp_divide(numerator, denominator),
                                fp_divide(other, from_word(increment_word * 3u)));
    const float required = fp_max(tap_floor, ratios);
    float fraction = fp_multiply(rate, span_float);
    fraction = fp_multiply(fp_up(second_ratio), fraction);
    fraction = fp_multiply(fp_multiply(from_word(units), 0.125f), fraction);
    required_metrics result = {fp_multiply(required, from_word(divisor)),
                               fp_divide(fraction, 1000000.0f)};
    return result;
}

static required_metrics metrics_with_flag(uint32_t first_numerator,
        uint32_t second_numerator, uint32_t divisor, uint32_t units,
        uint32_t first_taps, uint32_t span, float increment, float second_denominator,
        float second_ratio, float budget)
{
    float scale = 1.0f;
    if (fp_compare(1.0f, second_ratio).below) {
        scale = float_word(0x3faaaaab);
        if (fp_compare(scale, second_ratio).below) {
            scale = 1.5f;
            if (fp_compare(scale, second_ratio).below) {
                scale = 2.0f;
                if (fp_compare(scale, second_ratio).below) {
                    const fp_relation r = fp_compare(second_ratio, 3.0f);
                    scale = r.above || r.unordered ? 4.0f : 3.0f;
                }
            }
        }
    }
    const uint32_t budget_word = trunc_word(budget);
    float word = fp_up(fp_multiply(from_word(first_numerator + first_taps), 0.0078125f));
    word = fp_multiply(word, float_word(0x3f866666));
    word = fp_multiply(word, 128.0f);
    word = fp_multiply(word, 1000000.0f);
    word = fp_multiply(word, scale);
    const float fraction_denominator = fp_divide(fp_multiply(increment, second_denominator),
                                                from_word(divisor * 10000u));
    uint32_t fraction_word = (units >> 3) * second_numerator;
    fraction_word *= span + first_taps + 2u;
    required_metrics result = {fp_divide(fp_divide(word, from_word(budget_word)), 10000.0f),
        fp_divide(fp_divide(from_word(fraction_word), fraction_denominator), 1000000000.0f)};
    return result;
}

static void quantize_rows(const rt_video_out_transform_state *state,
                          complete_staging *staging, int half_first)
{
    for (size_t row = 0; row < RT_VIDEO_OUT_TRANSFORM_ROWS; ++row) {
        uint32_t captured[row_words], packed[row_words];
        int negative[row_words];
        access_boundary();
        memcpy(captured, &state->banks[quantized_bank][row], full_row_words * word_bytes);
        memcpy(captured + full_row_words, (const uint8_t *)&state->banks[quantized_bank][row] +
               offsetof(rt_video_out_transform_row, word_20),
               (row_words - full_row_words) * word_bytes);
        access_boundary();
        for (size_t lane = 0; lane < row_words; ++lane) {
            const fp_relation sign = fp_compare(float_word(captured[lane]), 0.0f);
            negative[lane] = sign.below && !sign.unordered;
            if (negative[lane]) captured[lane] ^= UINT32_C(0x80000000);
        }
        float scaled[row_words];
        for (size_t lane = 0; lane < row_words; ++lane)
            scaled[lane] = fp_multiply(float_word(captured[lane]), 1024.0f);
        for (size_t lane = 0; lane < row_words; ++lane)
            scaled[lane] = half_first ? fp_add(0.5f, scaled[lane]) : fp_add(scaled[lane], 0.5f);
        for (size_t lane = 0; lane < row_words; ++lane) {
            const uint32_t shifted = trunc_word(scaled[lane]) << coefficient_shift;
            const uint32_t limited = signed_min(shifted, coefficient_limit);
            packed[lane] = negative[lane] && signed_word(shifted) > 0 ?
                           negative_coefficient_base - limited : limited;
        }
        access_boundary();
        memcpy(staging->prefix.rows[row].leading_00, packed, vector_words * word_bytes);
        access_boundary();
        memcpy(staging->prefix.rows[row].leading_00 + vector_words * word_bytes,
               packed + vector_words, vector_words * word_bytes);
        access_boundary();
        memcpy((uint8_t *)&staging->prefix.rows[row] + offsetof(rt_video_out_transform_row, word_20),
               packed + full_row_words,
               (row_words - full_row_words) * word_bytes);
        access_boundary();
    }
}

static void write_tails(rt_video_out_transform_state *a, rt_video_out_transform_state *b,
                        complete_staging *stage_a, complete_staging *stage_b)
{
    float first = from_word(read_word((uint64_t)(uintptr_t)stage_a->prefix.control_00));
    const float second_ratio = read_float(PARAMETER(b, ratio_00));
    first = fp_add(first, read_float(PARAMETER(a, ratio_00)));
    first = fp_add(first, 1.0f);
    first = fp_multiply(first, 0.5f);
    float second = fp_add(second_ratio, from_word(read_word((uint64_t)(uintptr_t)stage_b->prefix.control_00)));
    const uint32_t first_integer = trunc_word(first);
    float second_half = fp_multiply(second_ratio, 0.5f);
    second = fp_add(second, 1.0f);
    const float first_fraction = fp_subtract(first, from_word(first_integer));
    second = fp_multiply(second, 0.5f);
    const uint32_t second_integer = trunc_word(second);
    second_half = fp_add(second_half, second);
    const float scaled_first = fp_multiply(first_fraction, 524288.0f);
    const uint32_t third_integer = trunc_word(second_half);
    const uint32_t first_fixed = trunc_word(scaled_first) << fixed_fraction_shift;
    float second_fraction = fp_subtract(second, from_word(second_integer));
    staging_tail *tail_a = (staging_tail *)(void *)stage_a->prefix.tail_284;
    staging_tail *tail_b = (staging_tail *)(void *)stage_b->prefix.tail_284;
    write_word(tail_a->fraction_288, first_fixed);
    write_word(tail_a->integer_284, first_integer);
    const uint64_t zero = 0;
    access_boundary();
    memcpy((uint8_t *)tail_a + offsetof(staging_tail, second_integer_28c), &zero, sizeof zero);
    access_boundary();
    second_fraction = fp_multiply(second_fraction, 524288.0f);
    const uint32_t second_fixed = trunc_word(second_fraction) << fixed_fraction_shift;
    float third_fraction = fp_subtract(second_half, from_word(third_integer));
    third_fraction = fp_multiply(third_fraction, 524288.0f);
    write_word(tail_b->fraction_288, second_fixed);
    write_word(tail_b->integer_284, second_integer);
    const uint32_t third_fixed = trunc_word(third_fraction) << fixed_fraction_shift;
    write_word(tail_b->second_fraction_290, third_fixed);
    write_word(tail_b->second_integer_28c, third_integer);
}

static int set_window(rt_video_out_transform_state *state, float taps)
{
    const uint32_t control = read_word((uint64_t)(uintptr_t)PARAMETER(state, control_04));
    if (control == 1) {
        if (!fp_compare(taps, 6.0f).below) {
            write_word(PARAMETER(state, window_20), 3);
            return 1;
        }
        write_word(PARAMETER(state, control_04), 0);
    } else if (control != 0) {
        return 0;
    }
    write_word(PARAMETER(state, window_20), 2);
    return 1;
}

uint64_t rt_video_out_transform_records(const void *source_storage, void *output_storage)
{
    const rt_video_out_record_bindings *imports = rt_video_out_record_imports;
    if (!imports)
        rt_fatal("VideoOut record transformer", "live guard and import cells have not been bound");
    uint64_t guard = guard_pointer(imports);
    const uint64_t saved = read_quad((const void *)(uintptr_t)guard);
    copied_stages copies;
    clear_stages(imports, &copies);
    const uint64_t source = (uint64_t)(uintptr_t)source_storage;
    const uint32_t kind = SOURCE_WORD(source, kind_20);
    const uint32_t ceiling_bits = SOURCE_WORD(source, ceiling_24);
    const uint32_t flags = SOURCE_WORD(source, flags_30);
    const uint32_t divisor = SOURCE_WORD(source, divisor_2c) << (flags & 1u);
    const float ceiling = float_word(ceiling_bits ? ceiling_bits : UINT32_C(0x40123d71));
    if (fp_compare(ceiling, float_word(0x40923d71)).above)
        return guarded_return(imports, guard, saved, parameter_error);
    const uint32_t numerator_b = SOURCE_WORD(source, second_numerator_14);
    const uint32_t numerator_a = SOURCE_WORD(source, first_numerator_10);
    const uint32_t denominator_a = SOURCE_WORD(source, first_denominator_18);
    const uint32_t denominator_b = SOURCE_WORD(source, second_denominator_1c);
    const uint32_t increment_bits = SOURCE_WORD(source, increment_28);
    rt_video_out_transform_state a, b;
    complete_staging stage_a, stage_b;
    (void)rt_video_out_transform_initialize(&a, &stage_a);
    (void)rt_video_out_transform_initialize(&b, &stage_b);
    write_word(PARAMETER(&a, discriminator_0c), 0);
    write_word(PARAMETER(&b, discriminator_0c), 1);
    write_word(PARAMETER(&a, window_24), first_tap_limit);
    write_word(PARAMETER(&b, window_24), second_tap_limit);
    if (signed_word(numerator_a) <= 0 || signed_word(denominator_a) <= 0) {
        guard = guard_pointer(imports);
        return guarded_return(imports, guard, saved, parameter_error);
    }
    const float first_num = from_word(numerator_a);
    const float first_den = from_word(denominator_a);
    const float ratio_a = fp_divide(first_num, first_den);
    if (fp_compare(0.0f, ratio_a).above || signed_word(numerator_b) <= 0 ||
        signed_word(denominator_b) <= 0) {
        guard = guard_pointer(imports);
        return guarded_return(imports, guard, saved, parameter_error);
    }
    const float second_num = from_word(numerator_b);
    const float second_den = from_word(denominator_b);
    const float ratio_b = fp_divide(second_num, second_den);
    if (fp_compare(0.0f, ratio_b).above) {
        guard = guard_pointer(imports);
        return guarded_return(imports, guard, saved, parameter_error);
    }
    const fp_relation first_admission = fp_compare(0.0f, ratio_a);
    guard = guard_pointer(imports);
    uint32_t mode_a;
    if (!first_admission.below) { print_invalid(imports, ratio_a); mode_a = 0; }
    else {
        const uint32_t mode = 2u - fp_compare(1.0f, ratio_a).above;
        const fp_relation one = fp_compare(ratio_a, 1.0f);
        mode_a = !one.equal || one.unordered ? mode : 0;
    }
    const uint32_t mode_b = ratio_mode(imports, ratio_b);
    write_float(PARAMETER(&b, ratio_00), ratio_b);
    write_word(PARAMETER(&b, mode_08), mode_b);
    float fixed_a = fp_multiply(ratio_a, 524288.0f);
    write_float(PARAMETER(&a, ratio_00), ratio_a);
    write_word(PARAMETER(&a, mode_08), mode_a);
    fixed_a = fp_add(0.5f, fixed_a);
    const uint32_t ratio_word_a = trunc_word(fixed_a);
    const uint32_t ratio_word_b = trunc_word(fp_add(0.5f, fp_multiply(ratio_b, 524288.0f)));
    write_word(stage_a.prefix.word_294, signed_min(ratio_word_a, fixed_ratio_limit) << fixed_fraction_shift);
    write_word(stage_b.prefix.word_294, signed_min(ratio_word_b, fixed_ratio_limit) << fixed_fraction_shift);
    float x10 = 1.0f;
    if (flags & 2u) {
        const float increment_float = fp_add(from_word(increment_bits), 1.0f);
        const float extended_numerator = from_word(numerator_a + 10u);
        const float extra = fp_multiply(ratio_a, increment_float);
        x10 = fp_divide(extra, extended_numerator);
    }
    ratio_band band;
    if (!second_ratio_band(ratio_b, &band))
        return guarded_return(imports, guard, saved, parameter_error);
    const uint32_t increment_word = increment_bits + 1u;
    const float increment = from_word(increment_word);
    const float rate = fp_divide(from_word(divisor * 10u), increment);
    float span_value = fp_up(fp_multiply(from_word(numerator_a - 1u), 0.0078125f));
    const float span_ratio = fp_divide(from_word(numerator_a - 256u), first_num);
    span_value = fp_add(fp_multiply(span_value, 128.0f), 128.0f);
    const uint32_t span = trunc_word(span_value);
    const float span_float = from_word(span);
    const float budget = fp_divide(1000.0f, rate);
    const float weighted_span = fp_divide(fp_multiply(span_float, float_word(0x3f866666)), 655.0f);
    const float chosen_scale = choose_scale(first_num, first_den, span_ratio, increment, budget, weighted_span);
    const float tap_budget = fp_divide(65500.0f, from_word(divisor));
    uint32_t tap_b = 1;
    if (mode_b != 0) {
        const uint32_t size_limit = trunc_word(fp_down(fp_divide(13440.0f, first_num))) - 1u;
        float first, second;
        if (mode_b == 2) {
            first = fp_min(4.0f, fp_multiply(tap_budget, 0.75f));
            second = fp_multiply(x10, tap_budget);
            first = fp_min(chosen_scale, first);
            first = fp_divide(3.0f, first);
        } else {
            first = fp_multiply(x10, tap_budget);
            second = fp_divide(3.0f, ratio_a);
        }
        const uint32_t calculated = trunc_word(fp_down(fp_multiply(second, first)));
        tap_b = signed_min(second_tap_limit, signed_min(size_limit, calculated));
    }
    uint32_t tap_a = 1;
    if (mode_a != 0)
        tap_a = signed_min(first_tap_limit, trunc_word(fp_down(fp_multiply(x10, tap_budget))) << coefficient_shift);
    const uint32_t units = ((kind != 2u) ? 1u : 0u) * 32u + 32u;
    required_metrics required = flags & 2u ?
        metrics_with_flag(numerator_a, numerator_b, divisor, units, tap_a,
                          span, increment, second_den, ratio_b, budget) :
        metrics_without_flag(increment_word, divisor, units, tap_a, tap_b,
                             span, band, ratio_a, ratio_b, first_den, span_ratio, span_float, rate);
    if (flags & 2u) required.fraction = fp_add(required.fraction, required.fraction);
    if (fp_compare(ceiling, required.fraction).below)
        return guarded_return(imports, guard, saved, parameter_error);
    const uint32_t required_word = trunc_word(required.word);
    if (signed_word(required_word) > required_word_limit ||
        ((mode_a == 1 || mode_a == 2) && signed_word(tap_a) <= 3) ||
        ((mode_b == 1 || mode_b == 2) && signed_word(tap_b) <= 3))
        return guarded_return(imports, guard, saved, parameter_error);
    write_word(PARAMETER(&a, required_word_1c), required_word);
    write_word(PARAMETER(&a, first_taps_14), tap_a);
    write_word(PARAMETER(&a, second_taps_10), tap_b);
    write_word(PARAMETER(&b, required_word_1c), required_word);
    write_word(PARAMETER(&b, first_taps_14), tap_a);
    write_word(PARAMETER(&b, second_taps_10), tap_b);
    write_float(PARAMETER(&a, required_float_18), required.fraction);
    write_float(PARAMETER(&b, required_float_18), required.fraction);
    /* The native A-class-zero branch skips BOTH count rewrites. */
    if (mode_a != 0) {
        tap_a -= signed_word(tap_a) > 2 ? tap_a & 1u : 0u;
        write_word(stage_a.prefix.control_00, tap_a);
        tap_b -= signed_word(tap_b) > 2 ? tap_b & 1u : 0u;
        write_word(stage_b.prefix.control_00, tap_b);
        float taps = from_word(tap_a);
        if (mode_a == 2) taps = fp_divide(taps, taps);
        if (!set_window(&a, taps))
            return guarded_return(imports, guard, saved, UINT32_MAX);
    }
    if (mode_b != 0) {
        float taps = from_word(read_word((uint64_t)(uintptr_t)stage_b.prefix.control_00));
        if (mode_b == 2) taps = fp_divide(taps, taps);
        if (!set_window(&b, taps))
            return guarded_return(imports, guard, saved, UINT32_MAX);
    }
    (void)rt_video_out_transform_coefficients(&a, &stage_a);
    (void)rt_video_out_transform_coefficients(&b, &stage_b);
    quantize_rows(&a, &stage_a, 0);
    quantize_rows(&b, &stage_b, 1);
    write_tails(&a, &b, &stage_a, &stage_b);
    copy_stage(imports, &copies.stages[0], &stage_a);
    copy_stage(imports, &copies.stages[1], &stage_b);
    copies.ready_538 = 1;
    const uint64_t output = (uint64_t)(uintptr_t)output_storage;
    (void)rt_video_out_transform_pack(&copies.stages[0], (void *)(uintptr_t)(output + first_output_offset));
    (void)rt_video_out_transform_pack(&copies.stages[1], (void *)(uintptr_t)(output + second_output_offset));
    return guarded_return(imports, guard, saved, 0);
}
