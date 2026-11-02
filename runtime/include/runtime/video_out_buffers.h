/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_BUFFERS_H
#define RUNTIME_VIDEO_OUT_BUFFERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Callable host imports/adapters for the directly observed functional wires;
   these do not declare unknown foreign ambient inputs or complete native ABIs. */
typedef int32_t (*rt_video_out_buffer_ioctl_fn)(int32_t, uint64_t, ...);
typedef void *(*rt_video_out_buffer_error_fn)(void);
typedef uint32_t (*rt_video_out_buffer_getpid_fn)(void);
typedef int32_t (*rt_video_out_buffer_printf_fn)(const char *, ...);
typedef int32_t (*rt_video_out_buffer_puts_fn)(const char *);
typedef void (*rt_video_out_buffer_stack_fail_fn)(void);

/* Owned host adapter ABI, not a six-argument native callback declaration.
   Dispatch the actual event4 target with its known event/record/argument prefix
   and inherited RCX/RAX metadata. Here RCX is the backend word and RAX is the
   target; the neighboring helpers have different wire metadata. Native arity,
   remaining registers/flags/SIMD and callback footprints remain unproved. */
typedef void (*rt_video_out_buffer_callback_bridge_fn)(
    uint64_t target, uint32_t event, void *record, uint64_t argument,
    uint64_t inherited_rcx, uint64_t inherited_rax);

/* Descriptor immutable during one call; pointed-to import/data cells stay live.
   Bind genuine module storage, warning literals and compatible callable host
   imports or real ABI adapters. The guard slot contains a pointer. Attribute,
   address, removal and DIP-switch helpers must use this same module instance.
   A NULL callback value skips notification; missing reached cells/targets stop
   by name. This source installs no module bindings, service defaults or export. */
typedef struct {
    const void *guard_pointer_slot;
    const void *descriptor_word;
    const void *ioctl_slot;
    const void *error_slot;
    const void *getpid_slot;
    const void *printf_slot;
    const void *puts_slot;
    const void *stack_fail_slot;
    const char *warning_format;
    const char *warning_limit_message;
    const void *callback_target_slot;
    const void *callback_argument_slot;
    rt_video_out_buffer_callback_bridge_fn callback_bridge;
} rt_video_out_buffer_bindings;

extern const rt_video_out_buffer_bindings *volatile rt_video_out_buffer_imports;

/* Select the first free group, then register using its late-read group ID.
   The full index wire is retained in borrowed storage; operational indices use only
   its low32 bits. Count/mode also use low32; any nonzero mode selects pairs.
   Clears precede range checks. Success returns a live saved group ID, while a
   buffer failure preserves its original error across partial cleanup whose
   results are ignored. Every normal full return zero-extends 32 bits.
   Uncompiled/unvalidated flat 64-bit little-endian source progress. No real
   registration/boot/Metal claim: services, provider footprints, physical native
   frame aliases, unknown bytes/ambient state, concurrency, emitted access widths
   and exact cross-host faults remain unproved. */
uint64_t rt_video_out_register_buffers(void *backend_storage, uint64_t initial_index_wire,
    const void *address_storage, uint32_t count, const void *attribute_storage,
    uint32_t mode);

#ifdef __cplusplus
}
#endif
#endif
