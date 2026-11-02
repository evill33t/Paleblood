/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_GROUP_H
#define RUNTIME_VIDEO_OUT_GROUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal read-only predicate. A signed group chooses the state and sixteen
   group words; their paired backend words use the unindexed base. Only state 1
   scans pairs, with live reads and immediate success at the first nonzero pair.
   There is no group bounds, pointer or ownership check. Returns full 0 or 1.
   Host port: accessible flat 64-bit little-endian storage; exact machine faults,
   register/flag effects and concurrent consistency remain unverified. */
uint64_t rt_video_out_group_has_pair(const void *backend_storage, int32_t group);

#ifdef __cplusplus
}
#endif
#endif
