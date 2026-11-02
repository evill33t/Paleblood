/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_ATTRIBUTES_H
#define RUNTIME_VIDEO_OUT_ATTRIBUTES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*rt_video_out_attribute_mode_fn)(void);
typedef int32_t (*rt_video_out_attribute_ioctl_fn)(int32_t, uint64_t, ...);
typedef void *(*rt_video_out_attribute_error_fn)(void);
typedef void (*rt_video_out_attribute_stack_fail_fn)(void);

/* Owned host adapter ABI, not the unknown native callback's formal signature.
   Dispatch the actual target with the known event/record/argument prefix, RCX
   target, RAX tail word and 32-byte YMM0 snapshot. The last pointer supplies raw
   register metadata, not an additional native argument. Native arity, remaining
   registers/flags, read footprint and callback semantics remain unproved. */
typedef void (*rt_video_out_attribute_callback_bridge_fn)(
    uint64_t target, uint32_t event, void *record, uint64_t argument,
    uint64_t inherited_rcx, uint64_t inherited_rax, const void *inherited_ymm0_32);

/* Descriptor immutable during one call; pointed-to cells remain live. Bind
   genuine module guard/descriptor/callback/table storage and compatible callable
   host imports or actual ABI adapters. The guard slot contains a pointer.
   Table fields identify the native 18-row, 3-row and 4-row triple tables; the
   18-row scan also reads its following DWORD sentinel. Only reached table/tag
   fields are read. No file-image NULL/default or synthetic table is inferred.
   The required group and post-attribute helpers use the same module instance.
   Missing cells/targets/bridges stop by name only when their paths are reached. */
typedef struct {
    const void *guard_pointer_slot;
    const void *descriptor_word;
    const void *dimension_table_18;
    const void *dimension_table_3;
    const void *dimension_table_4;
    const void *mode_slot;
    const void *ioctl_slot;
    const void *error_slot;
    const void *stack_fail_slot;
    const void *callback_target_slot;
    const void *callback_argument_slot;
    rt_video_out_attribute_callback_bridge_fn callback_bridge;
} rt_video_out_attribute_bindings;

extern const rt_video_out_attribute_bindings *volatile rt_video_out_attribute_imports;

/* Admit and apply a group's 40-byte attributes, then run post processing and
   optional notification. Nonzero update and exact update==1 select different
   behavior. Auxiliary/alignment stores may survive later failures; errno zero
   after a negative ioctl returns zero without success publication. Every normal
   full return zero-extends 32 bits, including a negative computed size's raw bits.
   Uncompiled/unvalidated flat 64-bit little-endian source progress. Real services,
   provider footprints, native-frame aliases, untouched bytes, ambient state,
   concurrency and exact cross-host faults remain unproved. No SCE export or
   module bindings are installed. */
uint64_t rt_video_out_apply_group_attributes(void *backend_storage, uint32_t group,
    const void *attribute_storage, uint32_t update);

#ifdef __cplusplus
}
#endif
#endif
