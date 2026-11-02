/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_ONCE_INITIALIZE_H
#define RUNTIME_VIDEO_OUT_ONCE_INITIALIZE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned compatible host prefix, not a complete native ambient ABI. */
typedef int32_t (*rt_video_out_once_mutex_init_fn)(void *mutex_storage,
    const void *attributes, const char *name);

/* OWNED real-call/RAX-capture bridge. target is the freshly read native printf
   import identity. Dispatch native RDI=format, RSI=name, RDX=zeroextended error
   bits and EAX=0 (including AL=0); return its ENTIRE raw RAX unchanged. This is
   not a native printf prototype: ordinary C int printf results cannot establish
   its upper return bits. Never cast the target to an int64-returning printf. */
typedef uint64_t (*rt_video_out_once_diagnostic_bridge_fn)(uint64_t target,
    const char *format, const char *name, uint32_t error_bits);

/* Immutable per call; actual module storage/import-cell contents remain live.
   mutex_storage is its genuine object ADDRESS; class_map and descriptors are
   actual module anchors. Literals/imports/bridge belong to that same instance.
   Only failure requires the live raw printf-target cell and capture bridge.
   Only success requires store anchors and the backend's frame transport.
   No initialized values, whole-object clear, service defaults or export. */
typedef struct {
    void *mutex_storage;
    const void *mutex_init_slot;
    const char *mutex_name;
    const void *printf_target_slot;
    rt_video_out_once_diagnostic_bridge_fn diagnostic_bridge;
    const char *error_format;
    void *class_map;
    void *descriptors;
} rt_video_out_once_initialize_bindings;

extern const rt_video_out_once_initialize_bindings *volatile rt_video_out_once_initialize_imports;

/* OWNED host transport for the backend's borrowed 96-byte frame; the native
   initializer has no functional data arguments. Any nonzero MutexInit low32
   result tails the genuine diagnostic bridge without ten state stores or frame
   access. Success makes ten ordered DWORD writes, then forwards the backend's
   full64 guard result. No abort, result normalization or rollback is inferred.
   Uncompiled/unvalidated flat64 little-endian source progress. Actual callback,
   printf ambient/variadic/physical-frame ABI, provider effects, access/fault
   progress, frame aliases, initialization and concurrency remain unproved. */
uint64_t rt_video_out_initialize_once_with_frame(void *backend_frame_storage);

#ifdef __cplusplus
}
#endif
#endif
