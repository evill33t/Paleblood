/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_ERRNO_MAP_H
#define RUNTIME_VIDEO_OUT_ERRNO_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RT_VIDEO_OUT_ERRNO_TABLE_ENTRIES = 46 };

/* Immutable descriptor per call; supply the actual module's live signed-DWORD
   relative dispatch table, with at least46 entries and its original code/table
   relationship. File bytes are not an initialized runtime value. No copied
   fallback table, import service, export or native entry installation supplied. */
typedef struct {
    const void *relative_table;
} rt_video_out_errno_map_bindings;

extern const rt_video_out_errno_map_bindings *volatile rt_video_out_errno_map_imports;

/* Only low32 error_bits participate. Unsigned values above45 return the
   observed default before any binding/table access. Otherwise read exactly
   the selected live DWORD; its signed displacement selects a recovered return
   action. A direct RET returns the preloaded input DWORD, including when a live
   entry changes. Other, unrecovered computed targets fail explicitly instead
   of receiving a guessed mapping. Normal return is zeroextended32.
   Uncompiled/unvalidated flat64 little-endian source progress. No physical
   native ABI, emitted access/fault/concurrency or arbitrary changed-code/target
   behavior is proved. There are no directly observed imports or memory writes. */
uint64_t rt_video_out_map_errno(uint64_t error_bits);

#ifdef __cplusplus
}
#endif
#endif
