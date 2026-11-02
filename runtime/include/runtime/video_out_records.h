/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_RECORDS_H
#define RUNTIME_VIDEO_OUT_RECORDS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*rt_video_out_clear_fn)(void *, int, size_t);
typedef void *(*rt_video_out_copy_fn)(void *, const void *, size_t);
typedef int (*rt_video_out_print_fn)(const char *, ...);
typedef void (*rt_video_out_stack_fail_fn)(void);

/* Host-port bindings, separate from the native function's two data arguments.
   Every field points to a live import cell, not a cached imported value/target.
   Bind genuine guard storage and actual import functions before calling. The
   descriptor and its cell addresses must stay fixed during the call; the cell
   contents can change. No guessed guard, loader binding or device API is supplied.
   Cells are byte storage containing the named 64-bit pointer representation.
   Function targets must be callable with the host ABI; guest code needs a real
   bridge. Missing cell addresses or function targets stop by name at their use;
   the guard pointee is read as supplied. The guard cell holds its 64-bit address. */
typedef struct {
    const void *guard_pointer_slot;
    const void *clear_slot;
    const void *copy_slot;
    const void *print_slot;
    const void *stack_fail_slot;
} rt_video_out_record_bindings;

extern const rt_video_out_record_bindings *volatile rt_video_out_record_imports;

/* Internal VideoOut transformer: source DWORDs at 0x10..0x30, adjacent packed
   records at destination+0x20 and +0x180. The source may overlap the destination;
   its words are captured before publication. Returns zero-extended native status.
   Host scalar port:
   little-endian x86-64 with SSE4.1 or ARM64, ordinary accessible backing, excluding
   native physical-frame aliases. FP environment, unused lanes, exact faults and
   import implementation accuracy remain unverified. Uncompiled source progress;
   no SCE export, runtime registration, display format or GPU behavior is claimed. */
uint64_t rt_video_out_transform_records(const void *source_storage, void *output_storage);

#ifdef __cplusplus
}
#endif
#endif
