/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_SETTINGS_INITIALIZE_H
#define RUNTIME_VIDEO_OUT_SETTINGS_INITIALIZE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned compatible host prefixes for observed functional wires. The native
   ambient GP/SSE/flags/stack ABI is not established by these declarations. */
typedef uint32_t (*rt_video_out_settings_mutex_fn)(void *mutex_storage,
    const void *attribute_storage, const char *name);
typedef int32_t (*rt_video_out_settings_mode_fn)(void);
typedef uint32_t (*rt_video_out_settings_pid_fn)(void);
typedef int32_t (*rt_video_out_settings_app_info_fn)(uint64_t process_bits,
    void *output_storage);
typedef void (*rt_video_out_settings_stack_fail_fn)(void);

enum { RT_VIDEO_OUT_SETTINGS_FRAME_BYTES = 88,
       RT_VIDEO_OUT_SETTINGS_FRAME_ALIGNMENT = 16 };

/* Immutable descriptor per call; pointed storage/import cells remain live.
   Supply the actual module's guard POINTER CELL, mutex OBJECT ADDRESS, literal
   SceVideoOutSetting address, result/flag DWORDs and compatible real imports.
   Use the same module as the process-flag helper and Once-word caller. No
   service values, initialized pthread object, callback bridge or export supplied. */
typedef struct {
    const void *guard_pointer_slot;
    const void *mutex_init_slot;
    const void *is_neo_slot;
    const void *has_neo_slot;
    const void *getpid_slot;
    const void *app_info_slot;
    const void *stack_fail_slot;
    void *mutex_storage;
    const char *mutex_name;
    void *mutex_result_word;
    void *has_neo_word;
    void *combined_mode_word;
    void *app_info_mask_word;
} rt_video_out_settings_initialize_bindings;

extern const rt_video_out_settings_initialize_bindings *volatile rt_video_out_settings_initialize_imports;

/* Both arguments are OWNED frame transport. Native body has no directly read
   functional data arguments. Supply an88-byte,16-aligned frame and a distinct
   simultaneous192-byte,32-aligned process-helper frame, preserving initial bytes.
   This body only saves the guard at50; AppInfo output at08 is NOT cleared.
   Preserve raw MutexInit result publication even on error, signed Neo predicates,
   two fresh pid calls and ordered flag writes. Negative AppInfo skips its output
   reads; otherwise read output+2c before conditional output+28 and preserve the
   raw20000000 mask. Return the CURRENT full guard through the ENTRY-CACHED
   pointer after comparing exposed saved guard; a returned stack-fail call traps.
   Uncompiled/unvalidated flat64 little-endian progress. Physical-frame/ambient
   ABI, exact access/fault/concurrency and unknown provider effects remain
   unproved. In particular MutexInit sees guard-valued RAX and IsNeo sees EAX0
   natively; ordinary host prefixes do not materialize these known ambient wires.
   Actual Once/callback/native bridging and provider installation remain external. */
uint64_t rt_video_out_initialize_settings_with_frames(void *frame_storage,
    void *process_frame_storage);

#ifdef __cplusplus
}
#endif
#endif
