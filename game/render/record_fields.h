// SPDX-License-Identifier: GPL-2.0-or-later
// Record bit fields used by the builder. Masked setters return the preserved
// pre-write bits, not the stored word or a status. Names describe known offsets
// and widths without assigning a physical resource format.
#pragma once

#include <cstdint>

#include "record_constructor.h"
#include "runtime/original.h"

namespace render {
RT_ORIGINAL(0x01486520, record_set_word_0c_fields,
            uint32_t(RecordConstructorPrefix *record, uint32_t first, uint32_t second));
RT_ORIGINAL(0x014866e0, record_set_word_04_repeated_field,
            uint32_t(RecordConstructorPrefix *record, uint32_t value));
RT_ORIGINAL(0x01486710, record_set_word_08_low22,
            uint32_t(RecordConstructorPrefix *record, uint32_t value));
RT_ORIGINAL(0x01486760, record_set_word_14_second_low5_field,
            uint32_t(RecordConstructorPrefix *record, uint32_t value));
RT_ORIGINAL(0x014867c0, record_set_word_10_byte_at_bit13,
            uint32_t(RecordConstructorPrefix *record, uint8_t value));
RT_ORIGINAL(0x014867e0, record_set_word_10_byte_at_bit14,
            uint32_t(RecordConstructorPrefix *record, uint8_t value));
RT_ORIGINAL(0x01486800, record_set_word_28_low22,
            uint32_t(RecordConstructorPrefix *record, uint32_t value));
}  // namespace render
