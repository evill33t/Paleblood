/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_PROCESS_FLAG_H
#define RUNTIME_VIDEO_OUT_PROCESS_FLAG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned compatible host prefixes for the observed query wires. The native
   helper forwards incoming RDI unchanged and supplies RSI=frame. These types
   do not establish a complete native/ambient import ABI or provider layout. */
typedef uint32_t (*rt_video_out_process_flag_query_fn)(uint64_t process_bits,
    void *query_storage);
typedef void (*rt_video_out_process_flag_stack_fail_fn)(void);

enum { RT_VIDEO_OUT_PROCESS_FLAG_FRAME_BYTES = 192,
       RT_VIDEO_OUT_PROCESS_FLAG_FRAME_ALIGNMENT = 32 };

/* Immutable descriptor per call; pointed import/data cells stay live. Bind the
   actual module's guard POINTER CELL and compatible real query/failure import
   cells. No host query, invented service result or synthetic guard is supplied. */
typedef struct {
    const void *guard_pointer_slot;
    const void *process_query_slot;
    const void *stack_fail_slot;
} rt_video_out_process_flag_bindings;

extern const rt_video_out_process_flag_bindings *volatile rt_video_out_process_flag_imports;

/* OWNED frame argument, not an inferred native argument. Supply a borrowed
   192-byte, 32-aligned projection with unknown initial bytes preserved. The
   helper saves the entry guard atA0, zeros32-byte spans in descending order
   60,40,20,00, then zeros the QWORD at80. The query receives the frame base;
   its full output layout/footprint remains external. The QWORD at08 is read
   unconditionally after the call, before the cached-pointer guard comparison.
   A zero query low32 yields that QWORD's bit62; ANY nonzero low32 yields
   zeroextendedFFFFFFFF. A returned stack-fail call traps. Uncompiled/unvalidated
   flat64 little-endian source progress: physical-frame/ambient/SIMD/ABI, exact
   access/fault, provider alias/lifetime and concurrency behavior remain unproved. */
uint64_t rt_video_out_query_process_flag_with_frame(uint64_t process_bits,
    void *frame_storage);

#ifdef __cplusplus
}
#endif
#endif
