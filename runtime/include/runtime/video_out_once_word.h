/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_ONCE_WORD_H
#define RUNTIME_VIDEO_OUT_ONCE_WORD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned host adapter prefix, not the native callback prototype. Transport the
   actual native initializer target through a genuine callback ABI bridge. */
typedef int32_t (*rt_video_out_word_once_adapter_fn)(void *once_storage,
    uint64_t initializer_target);

/* Immutable per call; actual module storage/import-cell contents remain live.
   Supply the genuine Once-control ADDRESS, actual initializer code address,
   compatible real Once adapter cell and actual result-DWORD storage address,
   all from the same module instance. No initialized values, no-op initializer,
   native callback cast, host pthread object, service defaults or export. */
typedef struct {
    void *once_storage;
    uint64_t initializer_target;
    const void *once_adapter_slot;
    const void *result_word;
} rt_video_out_once_word_bindings;

extern const rt_video_out_once_word_bindings *volatile rt_video_out_once_word_imports;

/* Call actual Once, ignore its result, then freshly read the module DWORD.
   Return that word zero-extended to full64; its meaning/value is not inferred.
   No functional input arguments, guard, local store, error branch or fallback.
   Uncompiled/unvalidated flat64 little-endian source progress. Real initializer
   effects/provider semantics, native callback/ambient/physical-frame ABI,
   emitted accesses/faults, concurrent visibility and object lifetime unproved. */
uint64_t rt_video_out_read_once_word(void);

#ifdef __cplusplus
}
#endif
#endif
