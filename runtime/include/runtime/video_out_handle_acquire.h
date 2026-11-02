/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_HANDLE_ACQUIRE_H
#define RUNTIME_VIDEO_OUT_HANDLE_ACQUIRE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned host callable prefixes only. Once transports the actual native
   initializer target to a genuine callback ABI bridge; this declares no native
   initializer prototype and never casts that address to a host function. */
typedef int32_t (*rt_video_out_handle_once_adapter_fn)(void *once_storage,
    uint64_t initializer_target);
typedef int32_t (*rt_video_out_handle_mutex_fn)(void *mutex_storage);
typedef int32_t (*rt_video_out_handle_wait_fn)(void *condition_storage, void *mutex_storage);

/* Immutable descriptor during one call; actual module storage and pointed
   import cells remain borrowed/live. Bind genuine Once/mutex storage ADDRESSES,
   the actual initializer target, compatible real synchronization/callback
   adapters, and the four-slot module table anchor. No initialized handles,
   object lifetime, host pthread objects, service defaults or export supplied.
   The slot table is required only after handle admission; missing reached
   dependencies stop by name. Use the same module instance as other helpers. */
typedef struct {
    void *once_storage;
    uint64_t initializer_target;
    const void *once_adapter_slot;
    void *mutex_storage;
    const void *lock_slot;
    const void *unlock_slot;
    const void *wait_slot;
    const void *slots_storage;
} rt_video_out_handle_acquire_bindings;

extern const rt_video_out_handle_acquire_bindings *volatile rt_video_out_handle_acquire_imports;

/* Attempt Once and lock before inspecting the signed low32 handle. Positive
   handles whose low byte is <=3 select a slot, including low-byte zero; require
   its full key to match. Reject NULL objects or their nonzero state words.
   Read the descriptor gate then the acquisition count, even for a zero gate.
   Wait while the gate is nonzero and the signed count positive. After wait,
   decrement the OLD object's waiter count, recheck the key, then reload the
   object pointer. Success increments the captured acquisition count modulo32.
   Every normal path calls unlock and returns the captured full object address or0,
   ignoring all synchronization results. No iteration bound or NULL-descriptor
   fallback is added. Uncompiled/unvalidated flat64 little-endian source progress;
   native callback/ambient/frame ABI, exact access/fault progress, concurrent RMW,
   provider semantics, initialization and complete lifetime remain unproved. */
uint64_t rt_video_out_acquire_handle(uint32_t handle_bits);

#ifdef __cplusplus
}
#endif
#endif
