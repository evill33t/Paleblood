// SPDX-License-Identifier: GPL-2.0-or-later
// Complete five-byte jump to the resolved GNM import. The original entry is unhooked.
// This source form preserves the known five-GP ignored-result submission boundary;
// it does not reconstruct the imported service or arbitrary inherited machine state.
#pragma once

#include <cstdint>
#include "runtime/original.h"

namespace render {
RT_ORIGINAL(0x01489a10, original_present_submit_forwarder,
            void(uint32_t, void *, void *, uintptr_t, uintptr_t));
namespace present_submit_calls {
RT_ORIGINAL(0x02fc05e8, submit_command_buffers_import,
            void(uint32_t, void *, void *, uintptr_t, uintptr_t));
}
inline void submit_present_buffers_source(uint32_t count, void *begins, void *sizes,
                                          uintptr_t auxiliary_begins, uintptr_t auxiliary_sizes)
{
    present_submit_calls::submit_command_buffers_import(
        count, begins, sizes, auxiliary_begins, auxiliary_sizes);
}
}  // namespace render
