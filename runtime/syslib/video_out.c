/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Build a VideoOut buffer attribute; this entry has no display or queue effects.

   Independent static analysis of the supplied libSceVideoOut ELF, SHA-256
   33a6f16349f07637e168db0d6338f0b2ae8aea626b86bc3d28325ecb5155e2c1,
   uniquely binds sceVideoOutSetBufferAttribute to the complete 33-byte leaf at
   module offset 0x2860. It captures the seventh argument's low DWORD first,
   clears 16 reserved bytes, then writes six DWORD arguments in order. There
   are no pointer/value checks or delegated calls. The public declaration is
   void; the native wire additionally returns the zero-extended pitch in RAX.

   This implements ordinary writable memory. Exact machine fault progress and
   compiler store lowering are unverified. No accepted format/tiling, surface
   extent, color interpretation, registration or flip behavior is inferred. */
#include "runtime/syslib.h"
#include "runtime/video_out.h"

#include <string.h>

static void video_out_attribute_word(uint8_t *field, uint32_t value)
{
    memcpy(field, &value, sizeof value);
}

static uint64_t video_out_set_buffer_attribute(
    void *output, uint32_t format, uint32_t tiling, uint32_t aspect,
    uint32_t width, uint32_t height, uint32_t pixel_pitch)
{
    const uint32_t captured_pitch = pixel_pitch;
    rt_video_out_buffer_attribute *attribute = output;
    memset(attribute->reserved, 0, sizeof attribute->reserved);
    video_out_attribute_word(attribute->format, format);
    video_out_attribute_word(attribute->tiling, tiling);
    video_out_attribute_word(attribute->aspect, aspect);
    video_out_attribute_word(attribute->width, width);
    video_out_attribute_word(attribute->height, height);
    video_out_attribute_word(attribute->pixel_pitch, captured_pitch);
    return (uint64_t)captured_pitch;
}
RT_SYSLIB("libSceVideoOut", sceVideoOutSetBufferAttribute, video_out_set_buffer_attribute);
