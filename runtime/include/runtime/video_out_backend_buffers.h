/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_BACKEND_BUFFERS_H
#define RUNTIME_VIDEO_OUT_BACKEND_BUFFERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned callable host import/adapter types for the observed functional wires.
   Foreign native ambient inputs and complete machine ABIs remain unproved. */
typedef int32_t (*rt_video_out_backend_buffers_mutex_fn)(void *mutex_storage);
typedef int32_t (*rt_video_out_backend_buffers_printf_fn)(const char *, ...);

/* Descriptor immutable during one call; import cell contents stay live.
   mutex_storage is the actual module object's ADDRESS, not a pointer-cell value
   or an invented host pthread object. Its layout and initialization are external.
   Bind genuine module storage, its diagnostic literal and compatible callable
   host imports or real ABI adapters, using the same module as the registration
   helper. No bindings, service defaults or native export are installed here.
   Missing required dependencies stop by name only on reached paths. */
typedef struct {
    void *mutex_storage;
    const void *lock_slot;
    const void *unlock_slot;
    const void *printf_slot;
    const char *lock_error_format;
} rt_video_out_backend_buffers_bindings;

extern const rt_video_out_backend_buffers_bindings *volatile rt_video_out_backend_buffers_imports;

/* Default backend registration wrapper. NULL address storage returns the native
   address error before even reading module bindings. Otherwise attempt the mutex
   lock, print a negative lock result, then read the live backend state word.
   Nonzero state delegates registration with a zero-extended index and mode0.
   Every nonNULL normal path calls unlock, even after a failed lock; its result
   cannot replace the previously captured 32-bit status. Normal full returns are
   zero-extended32. Further array/attribute/range admission stays delegated.
   Uncompiled/unvalidated flat 64-bit little-endian source progress. Actual mutex
   semantics/services, provider footprints, native physical-frame/ambient state,
   concurrency, emitted access widths and exact faults remain unproved. The
   existing handle-based host-port API is separate from this raw backend helper. */
uint64_t rt_video_out_register_backend_buffers(void *backend_storage, uint32_t index,
    const void *address_storage, uint32_t count, const void *attribute_storage);

#ifdef __cplusplus
}
#endif
#endif
