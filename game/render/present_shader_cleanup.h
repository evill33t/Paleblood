// SPDX-License-Identifier: GPL-2.0-or-later
// Shader completion updates the command context's preparation state before restoring
// its command mode. These are overlapping byte views, not complete owner layouts.
#pragma once

#include "secondary_calls.h"

namespace render {
struct ShaderCleanupState {
    ShaderField<uint32_t> command_mode;
    uint8_t unknown_0x0004[0xb364];
    ShaderField<uint32_t> dirty_flags;
};
static_assert(offsetof(ShaderCleanupState, command_mode) == 0x00);
static_assert(offsetof(ShaderCleanupState, dirty_flags) == 0xb368);
static_assert(sizeof(ShaderCleanupState) == 0xb36c);

struct ShaderCleanupCommands {
    uint8_t unknown_0x0000[0xb468];
    ShaderField<uint32_t> preparation_event;
    uint8_t unknown_0xb46c[0x4];
    ShaderField<uint32_t> event_count;
    uint8_t unknown_0xb474[0x14];
    ShaderField<uint32_t> preparation_mode;
};
static_assert(offsetof(ShaderCleanupCommands, preparation_event) == 0xb468);
static_assert(offsetof(ShaderCleanupCommands, event_count) == 0xb470);
static_assert(offsetof(ShaderCleanupCommands, preparation_mode) == 0xb488);
static_assert(sizeof(ShaderCleanupCommands) == 0xb48c);
}  // namespace render
