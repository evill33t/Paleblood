// SPDX-License-Identifier: GPL-2.0-or-later
// Capture packet placement before aliasing stores; read payloads after cursor publication.
#pragma once

#include <cstdio>
#include <cstdlib>

#include "byte_field.h"
#include "command_packets.h"
#include "runtime/render_observer.h"

namespace render {
struct IndexObservation {
    uint64_t buffer{};
    uint32_t offset{};
    bool available{};
};

inline IndexObservation observe_index_call(const PacketBuffer *commands, uintptr_t admitted_cursor)
{
    if (!rt_render_observer_available()) return {};
    const auto *bytes = reinterpret_cast<const uint8_t *>(commands);
    const uintptr_t begin = Field<uintptr_t>::read(bytes + offsetof(PacketBuffer, begin));
    if (admitted_cursor < begin || admitted_cursor - begin > UINT32_MAX) {
        std::fputs("renderer observer: command offset does not fit the recording format\n", stderr);
        std::abort();
    }
    return {begin, static_cast<uint32_t>(admitted_cursor - begin), true};
}
} // namespace render
