/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_REMOVE_ADDRESS_H
#define RUNTIME_VIDEO_OUT_REMOVE_ADDRESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*rt_video_out_remove_ioctl_fn)(int32_t, uint64_t, ...);
typedef void *(*rt_video_out_remove_error_fn)(void);
typedef void (*rt_video_out_remove_stack_fail_fn)(void);

/* Owned host adapter ABI, not a six-argument native callback declaration.
   Dispatch the actual target with the known event/record/argument prefix and
   inherited RCX/RAX values. Native arity, remaining registers/flags, SIMD state
   and read footprint are unproved; no zero SIMD state is inferred after ioctl. */
typedef void (*rt_video_out_remove_callback_bridge_fn)(
    uint64_t target, uint32_t event, void *record, uint64_t argument,
    uint64_t inherited_rcx, uint64_t inherited_rax);

/* Descriptor immutable during one call; its pointed-to cells remain live.
   Bind genuine module storage and callable host imports or actual ABI adapters.
   The guard slot contains a pointer. A NULL callback VALUE skips notification;
   an unbound cell or required target stops by name when reached. No module
   bindings or SCE export are installed by this source. */
typedef struct {
    const void *guard_pointer_slot;
    const void *descriptor_word;
    const void *ioctl_slot;
    const void *error_slot;
    const void *stack_fail_slot;
    const void *callback_target_slot;
    const void *callback_argument_slot;
    rt_video_out_remove_callback_bridge_fn callback_bridge;
} rt_video_out_remove_bindings;

extern const rt_video_out_remove_bindings *volatile rt_video_out_remove_imports;

/* Remove one registered index after its busy check. Successful ioctl clears
   its group mapping before the group's per-index active word and notification.
   Negative ioctl with errno zero returns zero without those updates. Index and
   group values are used as signed offsets without added bounds checks. There is
   no direct cached-address-pair clear. Every normal full return zero-extends 32 bits.
   Uncompiled/unvalidated flat 64-bit little-endian source progress. Services,
   provider footprints, native-frame aliases, untouched bytes, ambient state,
   concurrency and exact cross-host faults remain unproved. */
uint64_t rt_video_out_remove_address(void *backend_storage, int32_t index);

#ifdef __cplusplus
}
#endif
#endif
