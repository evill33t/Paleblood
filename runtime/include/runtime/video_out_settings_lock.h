/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_SETTINGS_LOCK_H
#define RUNTIME_VIDEO_OUT_SETTINGS_LOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OWNED bridge metadata, not a native import prototype. The caller must capture
   the incoming native state BEFORE entering ordinary C. The genuine external
   bridge restores that state, substitutes only RDI=mutex_storage and performs
   the actual tail transfer to target. Before transfer, preserve incoming RAX,
   other GP/SIMD registers, flags, native RSP, stack arguments and original
   return address. The bridge owns post-call native state and continuation;
   return the actual import's full raw RAX without int32 normalization. */
typedef uint64_t (*rt_video_out_settings_mutex_tail_fn)(uint64_t target,
    void *mutex_storage, void *native_context);

/* Immutable descriptor per call; import-cell contents remain live. Supply the
   actual settings mutex OBJECT ADDRESS and actual same-module lock/unlock
   import cells. The mutex pointee is not read or initialized by these helpers. */
typedef struct {
    void *mutex_storage;
    const void *lock_slot;
    const void *unlock_slot;
    rt_video_out_settings_mutex_tail_fn invoke_tail;
} rt_video_out_settings_lock_bindings;

extern const rt_video_out_settings_lock_bindings *volatile rt_video_out_settings_lock_imports;

/* Context is OWNED transport, not an inferred native functional argument.
   Each original body only replaces RDI with the actual settings-mutex address
   and tail-jumps to its live import. A zero target is forwarded to the bridge;
   no invented status substitutes for that native transfer. These C wrappers
   do not implement the required machine transport or synchronization service.
   Uncompiled/unvalidated flat64 little-endian progress; exact access/fault,
   physical tail/stack ABI, provider and concurrency behavior remain unproved. */
uint64_t rt_video_out_lock_settings_with_context(void *native_context);
uint64_t rt_video_out_unlock_settings_with_context(void *native_context);

#ifdef __cplusplus
}
#endif
#endif
