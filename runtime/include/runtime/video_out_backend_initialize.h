/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_BACKEND_INITIALIZE_H
#define RUNTIME_VIDEO_OUT_BACKEND_INITIALIZE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned compatible host callable prefixes for observed functional wires.
   Complete native ambient/variadic/machine ABIs remain outside this profile. */
typedef int32_t (*rt_video_out_initialize_open_fn)(const char *, int, ...);
typedef int32_t (*rt_video_out_initialize_sysctl_fn)(const char *, void *old_storage,
    void *length_storage, const void *new_storage, uint64_t new_bytes);
typedef int32_t (*rt_video_out_initialize_attribute_fn)(void *attribute_storage);
typedef int32_t (*rt_video_out_initialize_settype_fn)(void *attribute_storage, int type);
typedef int32_t (*rt_video_out_initialize_mutex_fn)(void *mutex_storage,
    const void *attribute_storage, const char *name);
typedef int32_t (*rt_video_out_initialize_printf_fn)(const char *, ...);
typedef void (*rt_video_out_initialize_stack_fail_fn)(void);

enum { RT_VIDEO_OUT_INITIALIZE_FRAME_BYTES = 96, RT_VIDEO_OUT_INITIALIZE_FRAME_ALIGNMENT = 32 };

/* Immutable descriptor per call; pointed module storage/import cells stay live.
   Bind actual module data addresses, its guard POINTER CELL, genuine literal
   addresses, and compatible real imports/adapters. mutex_storage is the object's
   ADDRESS, not a loaded cell or invented host pthread object. driver_operations
   is the actual table address. Use the same module as AddDriver/Once-word.
   No service defaults, initialized values, synthetic table or export installed.
   Required dependencies fail by name only on reached paths. */
typedef struct {
    const void *guard_pointer_slot;
    const void *stack_fail_slot;
    const void *open_slot;
    const void *sysctl_slot;
    const void *attribute_init_slot;
    const void *attribute_settype_slot;
    const void *mutex_init_slot;
    const void *attribute_destroy_slot;
    const void *printf_slot;
    void *device_descriptor_word;
    void *sdk_word;
    void *driver_word;
    void *mutex_storage;
    const void *driver_operations;
    const char *device_path;
    const char *sdk_name;
    const char *mutex_name;
    const char *error_format;
    const char *driver_name;
} rt_video_out_backend_initialize_bindings;

extern const rt_video_out_backend_initialize_bindings *volatile rt_video_out_backend_initialize_imports;

/* OWNED host frame transport, not an inferred native argument. Supply a borrowed
   96-byte, 32-aligned projection preserving unknown initial bytes. It exposes
   reused SDK/opaque-attribute storage at18, length/driver packet at20 and saved
   guard at58. Provider footprints/aliases remain external; no whole frame clear.
   Native body has no functional data arguments. It opens the actual device,
   stores a nonnegative descriptor, queries SDK with the observed zero fallback,
   attempts all four attribute/mutex steps and diagnoses negatives while
   continuing. Then invoke Once-word, build the ordered overlapping-zero packet,
   AddDriver and publish its full result. Every normal path returns the CURRENT
   full guard value through the ENTRY-CACHED pointer after comparing exposed
   saved guard; this is not an initialization status. A returned stack-fail call
   is followed by a trap. Uncompiled/unvalidated flat64 little-endian progress;
   physical-frame/ambient/SIMD/ABI, exact accesses/faults, unknown provider effects
   and concurrency remain unproved. No device/driver/boot success is claimed. */
uint64_t rt_video_out_initialize_backend_with_frame(void *frame_storage);

#ifdef __cplusplus
}
#endif
#endif
