// SPDX-License-Identifier: GPL-2.0-or-later
// Draw from index bytes copied into the reserved command storage.
#pragma once
#include "indexed_draw.h"

extern "C" void bb_render_draw_indexed_inline(render::PacketBuffer *, uint32_t index_count,
                                                const void *source, uint32_t byte_count);
