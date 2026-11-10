/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_RENDER_INDEX_OBSERVER_H
#define RUNTIME_RENDER_INDEX_OBSERVER_H

/* Indexed packet writers report the final packet after publishing their cursor.
   The base observer API remains shared with replacements that do not write these packets. */
#include "runtime/render_observer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Final packet fields from replacement writers, after their last cursor publication. */
void rt_observe_draw_index_offset_packet(uint64_t buffer, uint32_t offset, uint32_t header, uint32_t max_size,
                                         uint32_t first, uint32_t count, uint32_t initiator);
void rt_observe_draw_indexed(uint64_t buffer, uint32_t offset, uint32_t header, uint32_t max_size,
                              uint64_t address, uint32_t count, uint32_t initiator);

#ifdef __cplusplus
}
#endif

#endif
