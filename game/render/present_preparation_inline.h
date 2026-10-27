// SPDX-License-Identifier: GPL-2.0-or-later
// The native forwarding entry is shorter than a hook. Use its source form in the
// reconstructed consumer, leaving the original entry unhooked. These logical-void
// calls cover the observed consumer that discards incidental native registers.
#pragma once

#include "present_preparation_packets.h"

namespace render {
inline void bind_prepared_record(RecordPreparationState *, RecordCommandBuffer *commands,
                                 uint32_t slot, PresentRecordView *record)
{
    emit_prepared_record_packet(commands, slot, record);
}
// The11-byte entry forwards the entire incoming third argument; it is not a DWORD slot.
inline void select_record_preparation(RecordPreparationState *, RecordCommandBuffer *commands,
                                      uintptr_t descriptor)
{
    emit_prepared_selector_packet(commands, reinterpret_cast<const void *>(descriptor));
}
}  // namespace render
