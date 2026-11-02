/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_DEVICE_DESCRIPTOR_H
#define RUNTIME_VIDEO_OUT_DEVICE_DESCRIPTOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned host adapter prefix, not the native initializer's callback prototype.
   Call the actual live Once import and route the original initializer target
   through a genuine callback bridge. The host result is deliberately ignored. */
typedef int32_t (*rt_video_out_device_once_fn)(void *once_storage,
    uint64_t initializer_target);

/* Immutable descriptor per call; pointed module storage and adapter cell stay
   live. Supply the actual Once-control ADDRESS, original initializer code
   identity, compatible real adapter cell and device DWORD from the same module.
   No initialized control value, cached descriptor, no-op callback, provider
   implementation, native callback cast or export installation is supplied. */
typedef struct {
    void *once_storage;
    uint64_t initializer_target;
    const void *once_adapter_slot;
    const void *device_word;
} rt_video_out_device_descriptor_bindings;

extern const rt_video_out_device_descriptor_bindings *volatile rt_video_out_device_descriptor_imports;

/* Both entries have no directly read native functional data arguments.
   Initialization calls real Once and returns full zero after any normal return;
   this does not report synchronization or initializer success. Descriptor access
   then freshly reads the module DWORD, regardless of the ignored Once result,
   returning that value zero-extended to 64 bits. It checks device-word binding
   only after Once returns; standalone initialization never reads that word.
   Uncompiled/unvalidated flat64 little-endian owned host profile. The native
   ambient registers/flags/SIMD, physical frame/callback ABI, exact access/fault/
   concurrency and actual provider effects remain unproved. Genuine native Once
   and callback bridging remain external obligations. */
uint64_t rt_video_out_ensure_device_initialization(void);
uint64_t rt_video_out_read_device_descriptor(void);

#ifdef __cplusplus
}
#endif
#endif
