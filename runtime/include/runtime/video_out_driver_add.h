/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_DRIVER_ADD_H
#define RUNTIME_VIDEO_OUT_DRIVER_ADD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned compatible host adapters for the observed functional wires. The Once
   adapter transports the actual native initializer code address to a real ABI
   bridge. This is not a native callback prototype or permission to cast that
   address to a host function pointer. Its return and mutex returns are ignored. */
typedef int32_t (*rt_video_out_driver_once_adapter_fn)(void *once_storage,
    uint64_t initializer_target);
typedef int32_t (*rt_video_out_driver_mutex_fn)(void *mutex_storage);

/* Immutable during a call; addressed module objects/import cells remain live.
   Bind actual Once-control and mutex storage ADDRESSES from the same module as
   the publisher, the actual initializer target, and compatible real adapters.
   No host pthread objects, no-op initializer, default state or native export is
   installed. The initializer target and Once fields are needed only for a
   nonzero low32 class. Missing reached dependencies stop by name. */
typedef struct {
    void *once_storage;
    uint64_t initializer_target;
    const void *once_adapter_slot;
    void *mutex_storage;
    const void *lock_slot;
    const void *unlock_slot;
} rt_video_out_driver_add_bindings;

extern const rt_video_out_driver_add_bindings *volatile rt_video_out_driver_add_imports;

/* Nonzero low32 class invokes real Once; every normal path then attempts the
   lock, calls descriptor publication and unlocks, ignoring all synchronization
   results. The four functional inputs reach publication after synchronization;
   no pointer/reserved-field admission is moved outside the lock. Preserve the
   full64 publication result across unlock, including zero after partial writes.
   Uncompiled/unvalidated flat64 little-endian source progress. Genuine Once
   callback dispatch, module initialization and import adapters remain external.
   Native callback formal types, ambient GP/SIMD state, physical frames, service
   footprints, emitted access/fault behavior and concurrency remain unproved. */
uint64_t rt_video_out_add_driver(int32_t class_index, const void *operations,
    const void *parameters, uint64_t reserved_wire);

#ifdef __cplusplus
}
#endif
#endif
