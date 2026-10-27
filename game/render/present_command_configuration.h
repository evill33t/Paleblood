// SPDX-License-Identifier: GPL-2.0-or-later
// Presentation parameter call profiles and exact local scalar records. Packet
// interfaces use logical void for known ignored returns; the register setter keeps
// its stable DWORD result. Arbitrary RAX, upper SIMD and stack-frame equivalence
// are unclaimed.
// The parameter path preserves both original guard reads and the real failure import.
// A returning failure import is outside the recovered body and stops by name.
#pragma once

#include "present_register_packets.h"
#include "initialize_record_configuration_register.h"

namespace render {
RT_ORIGINAL(0x01477150, configure_record_option,
            void(RecordCommandBuffer *, uint32_t first, uint32_t second));
RT_ORIGINAL(0x01496f60, configure_record_parameters,
            void(RecordCommandBuffer *, uint32_t x, uint32_t y, uint32_t end_x,
                 uint32_t end_y, float scalar_a, float scalar_b));

struct RecordConfigurationFloat3 {
    float first;
    float second;
    float third;
};
static_assert(offsetof(RecordConfigurationFloat3, first) == 0x00);
static_assert(offsetof(RecordConfigurationFloat3, second) == 0x04);
static_assert(offsetof(RecordConfigurationFloat3, third) == 0x08);
static_assert(sizeof(RecordConfigurationFloat3) == 0x0c);

struct RecordConfigurationRegister {
    uint32_t word;
    uint8_t unknown_0x04[0x04];
};
static_assert(offsetof(RecordConfigurationRegister, word) == 0x00);
static_assert(sizeof(RecordConfigurationRegister) == 0x08);

struct RecordConfigurationScratch {
    RecordConfigurationRegister register_state;
    RecordConfigurationFloat3 translation;
    RecordConfigurationFloat3 scale;
};
static_assert(offsetof(RecordConfigurationScratch, register_state) == 0x00);
static_assert(offsetof(RecordConfigurationScratch, translation) == 0x08);
static_assert(offsetof(RecordConfigurationScratch, scale) == 0x14);
static_assert(sizeof(RecordConfigurationScratch) == 0x20);

// Pointer/scalar/low-SIMD profiles for the reconstructed parameter helper bodies.
// Ambient callback inputs, flags and physical stack-frame behavior stay unclaimed.
// The complete transform body reads three DWORDs from each triplet and ignores R8/R9.
// Retain the known caller's extra end_y argument for source compatibility; the body
// consumes no value from it. Ambient callback inputs and upper SIMD remain unclaimed.
RT_ORIGINAL(0x014765e0, configure_record_transform,
            void(RecordCommandBuffer *, uint32_t zero, RecordConfigurationFloat3 *scale,
                 RecordConfigurationFloat3 *translation, uint32_t inherited_end_y,
                 float first, float second));
RT_ORIGINAL(0x01485220, set_record_configuration_register,
            uint32_t(RecordConfigurationRegister *, uint32_t));
RT_ORIGINAL(0x01476980, emit_record_configuration_register,
            void(RecordCommandBuffer *, uint32_t));
RT_ORIGINAL(0x01476790, configure_record_extent,
            void(RecordCommandBuffer *, uint32_t, uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x01473650, configure_record_integer_pair,
            void(RecordCommandBuffer *, uint32_t, uint32_t));
RT_ORIGINAL(0x014736d0, configure_record_ratio_pair,
            void(RecordCommandBuffer *, float, float));
RT_ORIGINAL(0x01473750, configure_record_final_pair,
            void(RecordCommandBuffer *, float, float));
RT_ORIGINAL(0x02fbe4f8, fail_record_configuration_guard, void());

RT_GLOBAL(0x04cd168c, record_configuration_half, const float);
RT_GLOBAL(0x04cd1690, record_configuration_negative_half, const float);
RT_GLOBAL(0x04cd1694, record_configuration_one, const float);
RT_GLOBAL(0x04cd1698, record_configuration_sixteenth, const float);
RT_GLOBAL(0x04cd169c, record_configuration_margin, const float);
RT_GLOBAL(0x04cd16a0, record_configuration_limit, const float);
RT_GLOBAL(0x04cd16b0, record_configuration_magnitude_mask, const uint32_t);
RT_GLOBAL(0x057e3be0, record_configuration_guard_pointer, const uint64_t *);

inline int32_t record_configuration_signed_word(uint32_t word)
{
    int32_t value;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

inline float mask_record_configuration_float(float value, uint32_t mask)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    bits &= mask;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline uint64_t read_record_configuration_guard()
{
    const uint64_t *pointer = record_configuration_guard_pointer.get();
    uint64_t value;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}
}  // namespace render
