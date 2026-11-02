/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Generate, normalize and quantize VideoOut's strided coefficients.

   Static reading binds the complete 1496-byte internal leaf at module offset
   0x8940 in libSceVideoOut, SHA-256
   33a6f16349f07637e168db0d6338f0b2ae8aea626b86bc3d28325ecb5155e2c1.
   Capture the window word, publish it through the other base, then read the row
   count. Only supported modes read the live tap count each row. Neither count
   has a generic capacity bound; negative tap counts still update the centers.

   The supported host port uses flat 64-bit little-endian accessible storage and
   ordered scalar FP instructions on x86-64 with SSE4.1, or ARM64. ARM's
   invalid integer conversion is corrected to the native INT32_MIN result. SIMD
   chunks become separate captured 16-byte stores. Full vector/register state,
   packed unmasked FP traps, exact machine-fault progress, PS4/host libm accuracy,
   ARM FPCR versus MXCSR/NaN/DAZ/FTZ/status equivalence and physical-frame aliases
   remain unverified. This is uncompiled source progress, without a device API. */
#include "runtime/video_out_coefficients.h"

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#if !defined(__x86_64__) && !defined(__aarch64__)
#error "VideoOut coefficient operations need an x86-64 or ARM64 scalar FP port"
#endif
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "VideoOut coefficient storage currently requires a little-endian host"
#endif

/* Keep these as real external calls, with builtins disabled for this source.
   Do not attach purity attributes or replace the calls with approximations. */
extern float sinf(float);
extern float roundf(float);

_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "64-bit addresses");
_Static_assert(sizeof(float) == sizeof(uint32_t) && FLT_RADIX == 2 &&
               FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128, "binary32 floats");

enum {
    word_bytes = sizeof(uint32_t),
    row_stride_bytes = 40,
    coordinate_chunk_bytes = 16,
    coordinate_chunk_lanes = 4,
    mode_one_chunk_lanes = 16,
    mode_two_chunk_lanes = 4,
    phase_mode = 1,
    scaled_phase_mode = 2,
};

/* These are address anchors, not fixed-size arrays or a complete object type.
   Dynamic cells can cross nominal rows/banks, as the native loops permit. */
typedef struct {
    uint8_t scale_00[word_bytes];
    uint8_t unknown_04[word_bytes];
    uint8_t mode_08[word_bytes];
    uint8_t unknown_0c[0x14];
    uint8_t window_20[word_bytes];
    uint8_t unknown_24[word_bytes];
    uint8_t row_count_28[word_bytes];
    uint8_t weight_sum_2c[word_bytes];
    uint8_t unknown_30[0x3c];
    uint8_t normalized_sum_6c[word_bytes];
    uint8_t unknown_70[0x3c];
    uint8_t quantized_sum_ac[word_bytes];
    uint8_t unknown_b0[0x3c];
    uint8_t coordinate_ec[word_bytes];
    uint8_t unknown_f0[0x27c];
    uint8_t weight_36c[word_bytes];
    uint8_t unknown_370[0x27c];
    uint8_t normalized_5ec[word_bytes];
    uint8_t unknown_5f0[0x278];
    uint8_t preceding_center_868[word_bytes];
    uint8_t quantized_86c[word_bytes];
} workspace_anchors;

typedef struct {
    uint8_t tap_count_00[word_bytes];
    uint8_t unknown_04[0x294];
    uint8_t window_298[word_bytes];
} control_anchors;

_Static_assert(offsetof(workspace_anchors, scale_00) == 0x00, "scale");
_Static_assert(offsetof(workspace_anchors, mode_08) == 0x08, "mode");
_Static_assert(offsetof(workspace_anchors, window_20) == 0x20, "window");
_Static_assert(offsetof(workspace_anchors, row_count_28) == 0x28, "rows");
_Static_assert(offsetof(workspace_anchors, weight_sum_2c) == 0x2c, "weight sum");
_Static_assert(offsetof(workspace_anchors, normalized_sum_6c) == 0x6c, "normalized sum");
_Static_assert(offsetof(workspace_anchors, quantized_sum_ac) == 0xac, "quantized sum");
_Static_assert(offsetof(workspace_anchors, coordinate_ec) == 0xec, "coordinates");
_Static_assert(offsetof(workspace_anchors, weight_36c) == 0x36c, "weights");
_Static_assert(offsetof(workspace_anchors, normalized_5ec) == 0x5ec, "normalized");
_Static_assert(offsetof(workspace_anchors, preceding_center_868) == 0x868, "first center");
_Static_assert(offsetof(workspace_anchors, quantized_86c) == 0x86c, "quantized");
_Static_assert(offsetof(control_anchors, tap_count_00) == 0x00, "taps");
_Static_assert(offsetof(control_anchors, window_298) == 0x298, "published window");

static void access_boundary(void)
{
    __asm__ volatile("" ::: "memory");
}

static uint32_t read_word(uint64_t address)
{
    uint32_t value;
    access_boundary();
    memcpy(&value, (const void *)(uintptr_t)address, sizeof value);
    access_boundary();
    return value;
}

static void write_word(uint64_t address, uint32_t value)
{
    access_boundary();
    memcpy((void *)(uintptr_t)address, &value, sizeof value);
    access_boundary();
}

static int32_t signed_word(uint32_t bits)
{
    int32_t value;
    memcpy(&value, &bits, sizeof value);
    return value;
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

static float read_float(uint64_t address)
{
    return float_word(read_word(address));
}

static void write_float(uint64_t address, float value)
{
    write_word(address, float_bits(value));
}

/* Use machine-width unsigned address arithmetic, including negative centers.
   Actual backing and pointer provenance are the host-port caller's obligation;
   computing an address does not validate it or promise portable fault behavior. */
static uint64_t row_cell(uint64_t first, uint64_t row, uint64_t column)
{
    return first + row * row_stride_bytes + column * word_bytes;
}

static uint64_t summary_cell(uint64_t first, uint64_t row)
{
    return first + row * word_bytes;
}

static void write_coordinate_chunk(uint64_t address, const float *values)
{
    access_boundary();
    memcpy((void *)(uintptr_t)address, values, coordinate_chunk_bytes);
    access_boundary();
}

/* Use the layouts only as offset dictionaries. They do not impose an invented
   complete C object or fixed-capacity array on the foreign memory. */
typedef struct {
    uint64_t scale, mode, window, row_count;
    uint64_t weight_sum, normalized_sum, quantized_sum;
    uint64_t coordinate, weight, normalized, preceding_center, quantized;
} workspace_addresses;

typedef struct {
    uint64_t tap_count, window;
} control_addresses;

static workspace_addresses map_workspace(uint64_t base)
{
    return (workspace_addresses){
        .scale = base + offsetof(workspace_anchors, scale_00),
        .mode = base + offsetof(workspace_anchors, mode_08),
        .window = base + offsetof(workspace_anchors, window_20),
        .row_count = base + offsetof(workspace_anchors, row_count_28),
        .weight_sum = base + offsetof(workspace_anchors, weight_sum_2c),
        .normalized_sum = base + offsetof(workspace_anchors, normalized_sum_6c),
        .quantized_sum = base + offsetof(workspace_anchors, quantized_sum_ac),
        .coordinate = base + offsetof(workspace_anchors, coordinate_ec),
        .weight = base + offsetof(workspace_anchors, weight_36c),
        .normalized = base + offsetof(workspace_anchors, normalized_5ec),
        .preceding_center = base + offsetof(workspace_anchors, preceding_center_868),
        .quantized = base + offsetof(workspace_anchors, quantized_86c),
    };
}

static control_addresses map_control(uint64_t base)
{
    return (control_addresses){
        .tap_count = base + offsetof(control_anchors, tap_count_00),
        .window = base + offsetof(control_anchors, window_298),
    };
}

/* Tied x86 destinations and explicit ARM operands keep commutative operations
   in source order. Memory clobbers also pin them around live buffer accesses.
   These operations use the current host FP environment; they do not reset it. */
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

static float fp_integral_toward_zero(float value)
{
#if defined(__x86_64__)
    __asm__ volatile("roundss $11, %0, %0" : "+x"(value) : : "memory");
    return value;
#else
    float result;
    __asm__ volatile("frintz %s0, %s1" : "=w"(result) : "w"(value) : "memory");
    return result;
#endif
}

static int fp_ordered_zero(float value)
{
    const float zero = 0.0f;
#if defined(__x86_64__)
    uint8_t equal, ordered;
    __asm__ volatile("ucomiss %3, %2\n\tsete %0\n\tsetnp %1"
                     : "=q"(equal), "=q"(ordered) : "x"(value), "x"(zero) : "cc", "memory");
    return equal && ordered;
#else
    uint32_t equal;
    __asm__ volatile("fcmp %s1, %s2\n\tcset %w0, eq"
                     : "=r"(equal) : "w"(value), "w"(zero) : "cc", "memory");
    return equal != 0;
#endif
}

static int32_t wrapped_absolute(int32_t value)
{
    const uint32_t bits = (uint32_t)value;
    return signed_word(value < 0 ? UINT32_C(0) - bits : bits);
}

static float coordinate_value(uint64_t index, int32_t half_taps, uint32_t mode,
                              float denominator, float phase)
{
    const uint32_t coordinate_bits = (uint32_t)index - (uint32_t)half_taps + 1;
    float value = fp_from_signed(signed_word(coordinate_bits));
    if (mode == scaled_phase_mode)
        value = fp_divide(value, denominator);
    return fp_subtract(value, phase);
}

uint64_t rt_video_out_transform_coefficients(void *workspace_storage, void *control_storage)
{
    const workspace_addresses workspace = map_workspace((uint64_t)(uintptr_t)workspace_storage);
    const control_addresses control = map_control((uint64_t)(uintptr_t)control_storage);
    const uint32_t window_bits = read_word(workspace.window);
    write_word(control.window, window_bits);
    const int32_t rows = signed_word(read_word(workspace.row_count));
    if (rows <= 0)
        return (uint64_t)window_bits;

    const int32_t window = signed_word(window_bits);
    const float float_window = fp_from_signed(window);
    const uint32_t mode = read_word(workspace.mode);
    const int32_t half_rows = rows / 2;
    const float float_rows = fp_from_signed(rows);
    if (mode != phase_mode && mode != scaled_phase_mode)
        return (uint64_t)(uint32_t)half_rows;

    const float unit = float_word(UINT32_C(0x3f800000));
    const float pi = float_word(UINT32_C(0x40490fdb));
    const float pi_squared = float_word(UINT32_C(0x411de9e7));
    const float scale = float_word(UINT32_C(0x44800000));
    const float inverse_scale = float_word(UINT32_C(0x3a800000));
    const float half = float_word(UINT32_C(0x3f000000));
    int32_t last_half_taps = 0;

    for (uint64_t row = 0; row < (uint64_t)rows; ++row) {
        const int32_t taps = signed_word(read_word(control.tap_count));
        const int32_t half_taps = taps / 2;
        last_half_taps = half_taps;
        float quantized_sum = 0.0f;

        if (taps > 0) {
            float phase;
            float denominator = unit;
            const float float_row = fp_from_signed((int32_t)row);
            if (mode == phase_mode) {
                phase = fp_divide(float_row, float_rows);
            } else {
                denominator = read_float(workspace.scale);
                phase = fp_divide(float_row, denominator);
                phase = fp_divide(phase, float_rows);
            }

            const uint64_t chunk_lanes = mode == phase_mode ?
                mode_one_chunk_lanes : mode_two_chunk_lanes;
            const uint64_t vector_count = (uint64_t)taps & ~(chunk_lanes - 1);
            uint64_t sample = 0;
            for (; sample < vector_count; sample += chunk_lanes) {
                float coordinates[mode_one_chunk_lanes];
                /* Finish each packed arithmetic phase before any chunk store. */
                for (uint64_t lane = 0; lane < chunk_lanes; ++lane) {
                    const uint32_t bits = (uint32_t)(sample + lane) - (uint32_t)half_taps + 1;
                    coordinates[lane] = fp_from_signed(signed_word(bits));
                }
                if (mode == scaled_phase_mode) {
                    for (uint64_t lane = 0; lane < chunk_lanes; ++lane)
                        coordinates[lane] = fp_divide(coordinates[lane], denominator);
                }
                for (uint64_t lane = 0; lane < chunk_lanes; ++lane)
                    coordinates[lane] = fp_subtract(coordinates[lane], phase);
                for (uint64_t lane = 0; lane < chunk_lanes; lane += coordinate_chunk_lanes)
                    write_coordinate_chunk(row_cell(workspace.coordinate, row, sample + lane),
                                           coordinates + lane);
            }
            for (; sample < (uint64_t)taps; ++sample) {
                const float value = coordinate_value(sample, half_taps, mode, denominator, phase);
                write_float(row_cell(workspace.coordinate, row, sample), value);
            }

            write_word(summary_cell(workspace.weight_sum, row), 0);
            float weight_sum = 0.0f;
            for (sample = 0; sample < (uint64_t)taps; ++sample) {
                const float x = read_float(row_cell(workspace.coordinate, row, sample));
                float weight;
                if (fp_ordered_zero(x)) {
                    weight = unit;
                } else {
                    const int32_t truncated = fp_truncate_signed(x);
                    weight = 0.0f;
                    if (wrapped_absolute(truncated) < window) {
                        const float angle = fp_multiply(x, pi);
                        const float first_sine = sinf(angle);
                        const float first = fp_multiply(first_sine, float_window);
                        const float reduced_angle = fp_divide(angle, float_window);
                        const float second_sine = sinf(reduced_angle);
                        const float numerator = fp_multiply(second_sine, first);
                        float divisor = fp_multiply(x, x);
                        divisor = fp_multiply(divisor, pi_squared);
                        weight = fp_divide(numerator, divisor);
                    }
                }
                weight_sum = fp_add(weight, weight_sum);
                write_float(row_cell(workspace.weight, row, sample), weight);
            }
            write_float(summary_cell(workspace.weight_sum, row), weight_sum);
            write_word(summary_cell(workspace.normalized_sum, row), 0);
            float normalized_sum = 0.0f;
            for (sample = 0; sample < (uint64_t)taps; ++sample) {
                const float weight = read_float(row_cell(workspace.weight, row, sample));
                const float normalized = fp_divide(weight, weight_sum);
                normalized_sum = fp_add(normalized, normalized_sum);
                write_float(row_cell(workspace.normalized, row, sample), normalized);
            }
            write_float(summary_cell(workspace.normalized_sum, row), normalized_sum);
            write_word(summary_cell(workspace.quantized_sum, row), 0);
            for (sample = 0; sample < (uint64_t)taps; ++sample) {
                const float normalized = read_float(row_cell(workspace.normalized, row, sample));
                const float scaled = fp_multiply(scale, normalized);
                const float rounded = roundf(scaled);
                const float quantized = fp_multiply(inverse_scale, rounded);
                quantized_sum = fp_add(quantized_sum, quantized);
                write_float(row_cell(workspace.quantized, row, sample), quantized);
            }
            write_float(summary_cell(workspace.quantized_sum, row), quantized_sum);
        } else {
            write_word(summary_cell(workspace.weight_sum, row), 0);
            write_word(summary_cell(workspace.normalized_sum, row), 0);
            write_word(summary_cell(workspace.quantized_sum, row), 0);
        }

        const float residual = fp_subtract(unit, quantized_sum);
        float correction = fp_multiply(residual, half);
        correction = fp_multiply(scale, correction);
        correction = fp_integral_toward_zero(correction);
        correction = fp_multiply(inverse_scale, correction);
        const float remainder = fp_subtract(residual, correction);
        const float first_correction = row <= (uint64_t)half_rows ? remainder : correction;
        const float second_correction = row <= (uint64_t)half_rows ? correction : remainder;
        const uint64_t center = (uint64_t)(int64_t)half_taps;
        const uint64_t first_center = row_cell(workspace.preceding_center, row, center);
        float center_value = read_float(first_center);
        center_value = fp_add(first_correction, center_value);
        write_float(first_center, center_value);
        const uint64_t second_center = row_cell(workspace.quantized, row, center);
        center_value = read_float(second_center);
        center_value = fp_add(second_correction, center_value);
        write_float(second_center, center_value);
    }
    return (uint64_t)(int64_t)last_half_taps;
}
