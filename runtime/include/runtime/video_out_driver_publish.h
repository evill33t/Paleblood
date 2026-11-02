/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_DRIVER_PUBLISH_H
#define RUNTIME_VIDEO_OUT_DRIVER_PUBLISH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned host callable prefix for the genuine memcpy import or its ABI adapter.
   Complete foreign machine ABI and ambient inputs remain unproved. */
typedef void *(*rt_video_out_driver_copy_fn)(void *destination, const void *source, size_t bytes);

/* Immutable descriptor during a call; its storage and import-cell contents are
   borrowed and live. Bind the actual signed-DWORD class map anchor and descriptor
   array anchor from the same module, plus the actual copy import/ABI adapter.
   No array bounds, runtime map values, initialized records, service defaults or
   native exports are supplied here. Missing dependencies stop only if reached. */
typedef struct {
    const void *class_map;
    void *descriptors;
    const void *copy_slot;
} rt_video_out_driver_publish_bindings;

extern const rt_video_out_driver_publish_bindings *volatile rt_video_out_driver_publish_imports;

/* Publish one driver descriptor selected by the signed low32 class map index.
   NULL operations or nonzero full64 reserved_wire returns zero before bindings
   or table memory are read. Required operation cells are checked before map
   access. An occupied descriptor returns zero. There are no index range checks;
   a negative map value selects address zero, which is then read, not rejected.
   Store the class, call the real 264-byte copy import, then optionally copy the
   parameter prefix using two overlapping 32-byte snapshots and high/low stores.
   Only afterward check live source reserved cells: rejection leaves the earlier
   publications intact. Return the full descriptor address or zero.
   Uncompiled/unvalidated flat 64-bit little-endian source progress. This is not
   a complete descriptor/operations type or an executable callback transport.
   Exact emitted accesses/fault progress, physical frames, native SIMD/ambient
   state, concurrency and provider semantics remain unproved. */
uint64_t rt_video_out_publish_driver(int32_t class_index, const void *operations,
    const void *parameters, uint64_t reserved_wire);

#ifdef __cplusplus
}
#endif
#endif
