/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_DIPSWITCH_H
#define RUNTIME_VIDEO_OUT_DIPSWITCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Callable host import/adapter types for the observed wire prefix. Variadic
   open/ioctl/close/printf sites have AL zero on x86-64; this does not establish
   the foreign functions' complete formal declarations or ambient state. */
typedef int32_t (*rt_video_out_dipswitch_open_fn)(const char *, uint32_t, ...);
typedef int32_t (*rt_video_out_dipswitch_ioctl_fn)(int32_t, uint64_t, ...);
typedef int32_t (*rt_video_out_dipswitch_close_fn)(int32_t, ...);
typedef void *(*rt_video_out_dipswitch_error_fn)(void);
typedef uint32_t (*rt_video_out_dipswitch_kernel_error_fn)(uint32_t);
typedef int32_t (*rt_video_out_dipswitch_printf_fn)(const char *, ...);
typedef void (*rt_video_out_dipswitch_stack_fail_fn)(void);

/* Descriptor immutable during one call; import and guard cells remain live.
   Strings identify the genuine module literals for /dev/dipsw, its diagnostic
   format and the open/ioctl operation labels. The guard slot contains a pointer.
   Bind real module storage and callable host imports or actual ABI adapters.
   No native export, module/service bindings or DIP-switch default is installed.
   Required missing cells/targets stop by name only when reached. */
typedef struct {
    const void *guard_pointer_slot;
    const char *device_path;
    const char *diagnostic_format;
    const char *open_operation;
    const char *ioctl_operation;
    const void *open_slot;
    const void *ioctl_slot;
    const void *close_slot;
    const void *error_slot;
    const void *kernel_error_slot;
    const void *printf_slot;
    const void *stack_fail_slot;
} rt_video_out_dipswitch_bindings;

extern const rt_video_out_dipswitch_bindings *volatile rt_video_out_dipswitch_imports;

/* Open /dev/dipsw and request its DWORD with command 0x40048806. The successful
   output is read after close; an open/ioctl failure returns the actual
   sceKernelError result after printing, with close only on the ioctl path.
   Neither output nor mapped error is normalized to a Boolean. Every normal
   full return zero-extends 32 bits. No functional incoming arguments are used.
   Uncompiled/unvalidated flat 64-bit little-endian source progress. Actual
   service results, provider footprints, native-frame aliases, unknown bytes,
   ambient state, concurrency and exact cross-host faults remain unproved. */
uint64_t rt_video_out_read_dipswitch(void);

#ifdef __cplusplus
}
#endif
#endif
