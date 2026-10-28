// SPDX-License-Identifier: GPL-2.0-or-later
// Six-DWORD release packets and their argument-shuffling delegate. Returns preserve
// the full selected write address, or the opaque callback RAX on AL-zero failure.
#pragma once

#include "present_finish.h"

namespace render {
struct ReleasePacketCommandBuffer;
using ReleasePacketSpaceCallback = uintptr_t (*)(ReleasePacketCommandBuffer *, uint32_t,
                                                uintptr_t);
struct ReleasePacketCommandBuffer {
    uint8_t unknown_0x00[0x8];
    PresentFinishField<uintptr_t> limit;
    PresentFinishField<uintptr_t> cursor;
    PresentFinishField<ReleasePacketSpaceCallback> space_callback;
    PresentFinishField<uintptr_t> callback_argument;
};
static_assert(sizeof(uintptr_t) == sizeof(uint64_t));
static_assert(sizeof(ReleasePacketSpaceCallback) == sizeof(uintptr_t));
static_assert(offsetof(ReleasePacketCommandBuffer, limit) == 0x8);
static_assert(offsetof(ReleasePacketCommandBuffer, cursor) == 0x10);
static_assert(offsetof(ReleasePacketCommandBuffer, space_callback) == 0x18);
static_assert(offsetof(ReleasePacketCommandBuffer, callback_argument) == 0x20);

struct PresentReleasePacket {
    PresentFinishField<uint32_t> header;
    PresentFinishField<uint32_t> control;
    PresentFinishField<uint32_t> address_low;
    PresentFinishField<uint32_t> address_high;
    PresentFinishField<uint32_t> data_low;
    PresentFinishField<uint32_t> data_high;
};
static_assert(offsetof(PresentReleasePacket, header) == 0);
static_assert(offsetof(PresentReleasePacket, control) == 0x4);
static_assert(offsetof(PresentReleasePacket, address_low) == 0x8);
static_assert(offsetof(PresentReleasePacket, address_high) == 0xc);
static_assert(offsetof(PresentReleasePacket, data_low) == 0x10);
static_assert(offsetof(PresentReleasePacket, data_high) == 0x14);
static_assert(sizeof(PresentReleasePacket) == 0x18);

struct alignas(8) ReleasePacketFrame {
    PresentFinishField<uint64_t> saved_data;
};
static_assert(sizeof(ReleasePacketFrame) == 0x8);

// The AMD64 entry passes addresses of the original incoming stack words. Only
// each low DWORD is read, after the callback; upper slot padding is not inspected.
struct ReleasePacketStackWord {
    PresentFinishField<uint32_t> value;
};
static_assert(offsetof(ReleasePacketStackWord, value) == 0);
static_assert(sizeof(ReleasePacketStackWord) == 0x4);

RT_ORIGINAL(0x014778f0, emit_present_release_packet,
            uintptr_t(ReleasePacketCommandBuffer *, uint32_t selector, uint32_t flags,
                      uint64_t address, uint32_t mode, uint64_t data,
                      uint32_t stack_selector, uint32_t stack_mode));
}  // namespace render

// This body can also be called with explicit live slot pointers on other hosts.
// Its saved-data local does not claim the native physical frame relationship.
// The public original-ABI entry is AMD64 System V only and aborts on other CPUs;
// the entry bridge is uncompiled/unvalidated under the current source-only policy.
extern "C" uintptr_t bb_render_emit_present_release_packet_body(
    render::ReleasePacketCommandBuffer *, uint32_t selector, uint32_t flags,
    uint64_t address, uint32_t mode, uint64_t data,
    const render::ReleasePacketStackWord *stack_selector,
    const render::ReleasePacketStackWord *stack_mode);
