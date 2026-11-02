/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_ADDRESSES_H
#define RUNTIME_VIDEO_OUT_ADDRESSES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*rt_video_out_address_query_fn)(uint64_t, uint32_t, void *, uint64_t);
typedef int32_t (*rt_video_out_address_ioctl_fn)(int32_t, uint64_t, ...);
typedef void *(*rt_video_out_address_error_fn)(void);
typedef void (*rt_video_out_address_stack_fail_fn)(void);

/* This is a host adapter ABI, not a declaration of the opaque native callback.
   Its arguments carry the selected target, known event/record/argument prefix,
   and known inherited RCX/RAX wires. The bridge must dispatch the real target;
   remaining registers, flags, native arity and callback behavior are unproved.
   It must not substitute an invented callback or callback result. */
typedef void (*rt_video_out_address_callback_bridge_fn)(
    uint64_t target, uint32_t event, void *record, uint64_t argument,
    uint64_t inherited_rcx, uint64_t inherited_rax);

/* Separate host-port dependencies for the six-argument internal helper.
   Storage and import cells must belong to the genuine module instance, with
   compatible host-callable targets or actual ABI adapters. The descriptor is
   immutable during a call; pointed-to cell contents are loaded when reached.
   The guard cell contains a pointer, not a guard value. The callback target is
   opaque address bits; a null VALUE skips it, whereas an unbound CELL is an
   unresolved dependency. No host query/ioctl/errno/guard policy is substituted. */
typedef struct {
    const void *guard_pointer_slot;
    const void *query_slot;
    const void *ioctl_slot;
    const void *error_slot;
    const void *stack_fail_slot;
    const void *callback_target_slot;
    const void *callback_argument_slot;
    rt_video_out_address_callback_bridge_fn callback_bridge;
} rt_video_out_address_bindings;

extern const rt_video_out_address_bindings *volatile rt_video_out_address_imports;

/* Check queried ranges, submit their addresses, then publish the index/group
   entries and optionally notify the module callback. Mode!=0 enables the second
   range; exact mode==1 supplies the callback boolean. Completed range counts can
   change the backend before later failures. All normal results zero-extend 32 bits.
   Flat 64-bit little-endian source progress, uncompiled/unvalidated. Unknown provider
   writes, untouched stack bytes, arbitrary native-frame aliases, ambient state
   and exact cross-host faults remain unproved. No SCE export is installed. */
uint64_t rt_video_out_register_addresses(void *backend_storage, int32_t index,
    int32_t group, uint64_t first, uint64_t second, uint32_t mode);

#ifdef __cplusplus
}
#endif
#endif
