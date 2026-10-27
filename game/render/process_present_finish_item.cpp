// SPDX-License-Identifier: GPL-2.0-or-later
// Count and address arithmetic wrap at 64 bits, including the zero-count case.
// The final original is a native tail call; only its full RAX result is forwarded
// here, without claiming physical tail-stack or ambient-register equivalence.
#include "present_finish_item.h"

namespace {
constexpr uintptr_t finish_item_word_bytes = sizeof(uint64_t);
constexpr uint64_t processed_item_word = 2;
constexpr uint32_t item_packet_selector = 4;
}  // namespace

extern "C" uintptr_t bb_render_process_present_finish_item(
    render::FinishItemProcessor *processor, render::GpuCommandState *commands,
    uintptr_t item)
{
    const uint64_t count = processor->count.get();
    const uintptr_t address = item + count * finish_item_word_bytes - finish_item_word_bytes;
    auto *trailer = reinterpret_cast<render::FinishItemTrailer *>(address);
    trailer->word.set(processed_item_word);
    return render::emit_processed_present_finish_item(
        commands, item_packet_selector, trailer, 0, 0, commands);
}
