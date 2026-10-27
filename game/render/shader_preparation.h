// SPDX-License-Identifier: GPL-2.0-or-later
// Pending preparation events and optional range state consumed before shader bindings.
#pragma once

#include "secondary_calls.h"

namespace render {
struct ShaderPreparationFields {
    ShaderField<uint32_t> event;
    ShaderField<uint32_t> event_argument;
    ShaderField<uint32_t> event_count;
    ShaderField<uint32_t> first_begin;
    ShaderField<uint32_t> first_end;
    ShaderField<uint32_t> second_begin;
    ShaderField<uint32_t> second_end;
    ShaderField<uint32_t> range_flags;
    ShaderField<uint32_t> mode;
    uint8_t suppressed;
};
static_assert(offsetof(ShaderPreparationFields, event) == 0x00);
static_assert(offsetof(ShaderPreparationFields, event_argument) == 0x04);
static_assert(offsetof(ShaderPreparationFields, event_count) == 0x08);
static_assert(offsetof(ShaderPreparationFields, first_begin) == 0x0c);
static_assert(offsetof(ShaderPreparationFields, first_end) == 0x10);
static_assert(offsetof(ShaderPreparationFields, second_begin) == 0x14);
static_assert(offsetof(ShaderPreparationFields, second_end) == 0x18);
static_assert(offsetof(ShaderPreparationFields, range_flags) == 0x1c);
static_assert(offsetof(ShaderPreparationFields, mode) == 0x20);
static_assert(offsetof(ShaderPreparationFields, suppressed) == 0x24);

RT_ORIGINAL(0x02ab09e0, emit_shader_preparation_barrier,
            void(RecordCommandBuffer *, uint32_t, uint32_t));
RT_ORIGINAL(0x01475e10, emit_shader_preparation_range,
            void(RecordCommandBuffer *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t));
RT_ORIGINAL(0x01477f40, emit_shader_preparation_event,
            void(RecordCommandBuffer *, uint32_t, uint32_t, uint32_t));
}  // namespace render
