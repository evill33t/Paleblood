/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_COEFFICIENTS_H
#define RUNTIME_VIDEO_OUT_COEFFICIENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal VideoOut numerical work-area helper. Both areas are read and written
   in native order, with dynamic signed counts and possible overlap. No capacity
   or positive-count precondition is inferred from the known 16-row caller.
   The full native return depends on the early path or last captured tap count;
   it is not an integer success status. There is no public SCE export here.
   Host port: little-endian x86-64 with SSE4.1, or little-endian ARM64. */
uint64_t rt_video_out_transform_coefficients(void *workspace_storage, void *control_storage);

#ifdef __cplusplus
}
#endif
#endif
