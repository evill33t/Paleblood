/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_NATIVE_BUFFERS_H
#define RUNTIME_VIDEO_OUT_NATIVE_BUFFERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OWNED dispatch/low-EAX capture bridge, not a native callback prototype.
   target is the live native code address: dispatch with RAX=target, RDI=backend,
   RSI=zeroextended index32, RDX=addresses, RCX=zeroextended count32 and R8=attrs.
   Return only the observed native low32 result. R9/other ambient inputs are
   unclosed; do not invent a sixth native argument or cast the target to a
   guessed host prototype. Use genuine compatible callback transport. */
typedef uint32_t (*rt_video_out_native_register_bridge_fn)(uint64_t target,
    uint64_t backend, uint32_t index, const void *addresses, uint32_t count,
    const void *attributes);
typedef int32_t (*rt_video_out_native_mutex_fn)(void *mutex_storage);
typedef int32_t (*rt_video_out_native_broadcast_fn)(void *condition_storage);

/* Immutable per call; actual module storage and import cells remain live.
   mutex_storage is the genuine object's ADDRESS, not a loaded pointer-cell or
   invented host pthread object. Use the same module instance as acquire_handle.
   register_bridge is needed only for a nonNULL callback target. No native
   export, initialized handle/object/table, service defaults or host-port mapping
   installed. Missing reached dependencies stop by name. */
typedef struct {
    void *mutex_storage;
    const void *lock_slot;
    const void *broadcast_slot;
    const void *unlock_slot;
    rt_video_out_native_register_bridge_fn register_bridge;
} rt_video_out_native_buffer_bindings;

extern const rt_video_out_native_buffer_bindings *volatile rt_video_out_native_buffer_imports;

/* Native dispatch/cleanup wrapper; the existing host-port API stays separate.
   NULL addresses then NULL attrs return the native address error before even
   reading bindings or acquiring. The known acquirer consumes only handle low32.
   A NULL acquired object returns the native object error without cleanup.
   Otherwise read the live descriptor/callback. Only a nonNULL callback reads
   the backend word and invokes the real bridge. Both callback-return and missing
   callback paths attempt lock, live decrement of the CAPTURED object's count,
   condition broadcast and unlock, ignoring all synchronization results. Return
   cached callback low32 or native missing-callback error, zeroextended to full64.
   No descriptor NULL guard, count/range admission, object reload or canary is
   added. Uncompiled/unvalidated flat64 little-endian ordinary-memory progress;
   real callback/import behavior, lifetime, native ambient/physical ABI, emitted
   accesses/fault progress and concurrent RMW remain unproved. */
uint64_t rt_video_out_register_native_buffers(uint32_t handle_bits, uint32_t index,
    const void *addresses, uint32_t count, const void *attributes);

#ifdef __cplusplus
}
#endif
#endif
