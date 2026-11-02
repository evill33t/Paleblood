/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_POST_ATTRIBUTES_H
#define RUNTIME_VIDEO_OUT_POST_ATTRIBUTES_H

#include "runtime/video_out_records.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The variadic ioctl packet is passed as void *. The error adapter returns
   a byte-storage address; the caller requests a DWORD read, without imposing
   a typed foreign int object or an alignment requirement on that address. */
typedef int32_t (*rt_video_out_ioctl_fn)(int32_t, uint64_t, ...);
typedef void *(*rt_video_out_error_fn)(void);
typedef int32_t (*rt_video_out_mutex_fn)(void *);
typedef uint64_t (*rt_video_out_time_fn)(void);

/* Separate host-port bindings for the one-argument internal caller. Data fields
   identify genuine module storage; function fields identify live import cells
   containing compatible callable host targets. Descriptor and cell addresses
   stay fixed for a call; contents remain live at their native read epochs.
   Foreign guest code and SCE mutex objects require real ABI bridges. No host
   ioctl, errno, pthread object, clock, feature gate or guard value is substituted.
   Missing required cell addresses/targets stop by name only when reached.
   The record transformer's own bindings must refer to the same module instance. */
typedef struct {
    const void *guard_pointer_slot;
    const void *feature_word;
    const void *descriptor_word;
    void *mutex_storage;
    const void *ioctl_slot;
    const void *error_slot;
    const void *lock_slot;
    const void *print_slot;
    const void *time_slot;
    const void *unlock_slot;
    const void *stack_fail_slot;
} rt_video_out_post_bindings;

extern const rt_video_out_post_bindings *volatile rt_video_out_post_imports;

/* Poll operation 12, transform each nonempty result, and submit operation 8.
   At most eight completed submits occur. The empty path locks, optionally writes
   process time, and unlocks even after a negative lock result. Backend storage
   is not touched below the feature gate. Returns the full native result.
   Flat 64-bit little-endian scalar host port, uncompiled and unvalidated. Service
   initialization/footprints, untouched scratch bytes, ambient registers, complete
   native-stack aliases, exact faults and real provider behavior remain unproved.
   This does not register an SCE export or install module/service bindings. */
uint64_t rt_video_out_post_attributes(void *backend_storage);

#ifdef __cplusplus
}
#endif
#endif
