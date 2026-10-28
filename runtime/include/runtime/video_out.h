/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_H
#define RUNTIME_VIDEO_OUT_H

#include <stddef.h>
#include <stdint.h>

/* Byte-backed ABI view: the native setter accepts unaligned writable storage.
   Format and tiling are opaque words here, without a host pixel-layout mapping. */
enum {
    RT_VIDEO_OUT_ATTRIBUTE_WORD_BYTES = sizeof(uint32_t),
    RT_VIDEO_OUT_ATTRIBUTE_RESERVED_BYTES = 16,
};
typedef struct {
    uint8_t format[RT_VIDEO_OUT_ATTRIBUTE_WORD_BYTES];
    uint8_t tiling[RT_VIDEO_OUT_ATTRIBUTE_WORD_BYTES];
    uint8_t aspect[RT_VIDEO_OUT_ATTRIBUTE_WORD_BYTES];
    uint8_t width[RT_VIDEO_OUT_ATTRIBUTE_WORD_BYTES];
    uint8_t height[RT_VIDEO_OUT_ATTRIBUTE_WORD_BYTES];
    uint8_t pixel_pitch[RT_VIDEO_OUT_ATTRIBUTE_WORD_BYTES];
    uint8_t reserved[RT_VIDEO_OUT_ATTRIBUTE_RESERVED_BYTES];
} rt_video_out_buffer_attribute;

#ifdef __cplusplus
#define RT_VIDEO_OUT_LAYOUT_ASSERT static_assert
#else
#define RT_VIDEO_OUT_LAYOUT_ASSERT _Static_assert
#endif
RT_VIDEO_OUT_LAYOUT_ASSERT(offsetof(rt_video_out_buffer_attribute, format) == 0x00, "format");
RT_VIDEO_OUT_LAYOUT_ASSERT(offsetof(rt_video_out_buffer_attribute, tiling) == 0x04, "tiling");
RT_VIDEO_OUT_LAYOUT_ASSERT(offsetof(rt_video_out_buffer_attribute, aspect) == 0x08, "aspect");
RT_VIDEO_OUT_LAYOUT_ASSERT(offsetof(rt_video_out_buffer_attribute, width) == 0x0c, "width");
RT_VIDEO_OUT_LAYOUT_ASSERT(offsetof(rt_video_out_buffer_attribute, height) == 0x10, "height");
RT_VIDEO_OUT_LAYOUT_ASSERT(offsetof(rt_video_out_buffer_attribute, pixel_pitch) == 0x14, "pitch");
RT_VIDEO_OUT_LAYOUT_ASSERT(offsetof(rt_video_out_buffer_attribute, reserved) == 0x18, "reserved");
RT_VIDEO_OUT_LAYOUT_ASSERT(sizeof(rt_video_out_buffer_attribute) == 0x28, "attribute size");
#undef RT_VIDEO_OUT_LAYOUT_ASSERT

#endif
