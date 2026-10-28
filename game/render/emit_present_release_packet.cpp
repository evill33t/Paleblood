// SPDX-License-Identifier: GPL-2.0-or-later
// Write a release packet after six words are available. The callback can mutate
// the original incoming stack arguments, so the AMD64 entry passes their addresses
// to the readable body. No compiler-generated frame layout is assumed.
#include "present_release_packet.h"

namespace {
constexpr uint32_t release_packet_words = 6;
constexpr uint32_t command_word_shift = 2;
constexpr uint32_t release_packet_header = 0xc0044700;
constexpr uint32_t selector_mask = 0x3f;
constexpr uint32_t stack_selector_shift = 12;
constexpr uint32_t stack_mode_mask = 0x3;
constexpr uint32_t stack_mode_shift = 25;
constexpr uint32_t control_flag_mask = 0x10;
constexpr uint32_t control_flag_shift = 23;
constexpr uint32_t release_control_base = 0x500;
constexpr uint32_t address_low_mask = 0xfffffff8;
constexpr uint32_t address_high_mask = 0xffff;
constexpr uint32_t address_flag_mask = 0x1;
constexpr uint32_t address_flag_shift = 16;
constexpr uint32_t address_mode_shift = 29;
constexpr uint32_t high_word_shift = 32;
}  // namespace

extern "C" __attribute__((visibility("hidden"), noinline, used))
uintptr_t bb_render_emit_present_release_packet_body(
    render::ReleasePacketCommandBuffer *buffer, uint32_t selector, uint32_t flags,
    uint64_t address, uint32_t mode, uint64_t data,
    const render::ReleasePacketStackWord *stack_selector,
    const render::ReleasePacketStackWord *stack_mode)
{
    using namespace render;
    const uintptr_t limit = buffer->limit.get();
    uintptr_t write_pointer = buffer->cursor.get();
    const uint32_t available = static_cast<uint32_t>((limit - write_pointer) >> command_word_shift);
    ReleasePacketFrame frame;
    if (available < release_packet_words) {
        frame.saved_data.set(data);
        const uintptr_t callback_argument = buffer->callback_argument.get();
        const ReleasePacketSpaceCallback callback = buffer->space_callback.get();
        const uintptr_t callback_result = callback(buffer, release_packet_words, callback_argument);
        if (static_cast<uint8_t>(callback_result) == 0) return callback_result;
        write_pointer = buffer->cursor.get();
        data = frame.saved_data.get();
    }

    // These are live reads in native order, after the optional callback. The
    // bridge preserves the original slots instead of copying by-value arguments.
    const uint32_t stack_mode_value = stack_mode->value.get();
    const uint32_t stack_selector_value = stack_selector->value.get();
    const uint32_t control = (selector & selector_mask) |
        ((stack_selector_value & selector_mask) << stack_selector_shift) |
        ((stack_mode_value & stack_mode_mask) << stack_mode_shift) |
        ((flags & control_flag_mask) << control_flag_shift) | release_control_base;
    const uint32_t address_low = static_cast<uint32_t>(address) & address_low_mask;
    const uint32_t address_high =
        (static_cast<uint32_t>(address >> high_word_shift) & address_high_mask) |
        ((flags & address_flag_mask) << address_flag_shift) |
        (mode << address_mode_shift);
    const uint32_t data_low = static_cast<uint32_t>(data);
    const uint32_t data_high = static_cast<uint32_t>(data >> high_word_shift);

    auto *packet = reinterpret_cast<PresentReleasePacket *>(write_pointer);
    packet->header.set(release_packet_header);
    packet->control.set(control);
    packet->address_low.set(address_low);
    packet->address_high.set(address_high);
    packet->data_low.set(data_low);
    packet->data_high.set(data_high);
    // Packet stores can alias the buffer itself: ADD reads the live cursor.
    buffer->cursor.set(buffer->cursor.get() + sizeof(PresentReleasePacket));
    return write_pointer;
}

#if defined(__x86_64__)
extern "C" __attribute__((naked, no_stack_protector))
uintptr_t bb_render_emit_present_release_packet(
    render::ReleasePacketCommandBuffer *, uint32_t, uint32_t, uint64_t,
    uint32_t, uint64_t, uint32_t, uint32_t)
{
    // Entry RSP is 8 mod 16. Subtracting 24 aligns the body call and provides
    // its two stack pointer arguments without changing the original stack slots.
    __asm__(
        "leaq 8(%rsp), %rax\n"
        "leaq 16(%rsp), %r10\n"
        "subq $24, %rsp\n"
        "movq %rax, (%rsp)\n"
        "movq %r10, 8(%rsp)\n"
#if defined(__APPLE__)
        "callq _bb_render_emit_present_release_packet_body\n"
#else
        "callq bb_render_emit_present_release_packet_body\n"
#endif
        "addq $24, %rsp\n"
        "retq\n");
}
#else
extern "C" uintptr_t bb_render_emit_present_release_packet(
    render::ReleasePacketCommandBuffer *, uint32_t, uint32_t, uint64_t,
    uint32_t, uint64_t, uint32_t, uint32_t)
{
    std::fputs("render release packet: native entry requires AMD64 System V; "
               "use the explicit live-slot body on other CPUs\n", stderr);
    std::abort();
}
#endif
