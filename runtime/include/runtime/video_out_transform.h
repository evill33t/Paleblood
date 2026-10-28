/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_TRANSFORM_H
#define RUNTIME_VIDEO_OUT_TRANSFORM_H

#include <stddef.h>
#include <stdint.h>

/* Internal VideoOut transformation storage, described only where the native
   initializer writes it. Byte fields admit unaligned storage and retain raw
   values without assigning a GPU format or display policy. */
enum {
    RT_VIDEO_OUT_TRANSFORM_ROWS = 16,
    RT_VIDEO_OUT_TRANSFORM_BANKS = 4,
    RT_VIDEO_OUT_TRANSFORM_WORD_BYTES = 4,
    RT_VIDEO_OUT_TRANSFORM_VECTOR_BYTES = 32,
    RT_VIDEO_OUT_TRANSFORM_TAIL_BYTES = 16,
};

typedef struct {
    uint8_t leading_00[RT_VIDEO_OUT_TRANSFORM_VECTOR_BYTES];
    uint8_t word_20[RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
    uint8_t word_24[RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
} rt_video_out_transform_row;

typedef struct {
    uint8_t header_00[8];
    uint8_t header_08[8];
    uint8_t unknown_10[16];
    uint8_t header_20[8];
    uint8_t row_count_28[RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
    uint8_t lane_2c[RT_VIDEO_OUT_TRANSFORM_ROWS][RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
    uint8_t lane_6c[RT_VIDEO_OUT_TRANSFORM_ROWS][RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
    uint8_t lane_ac[RT_VIDEO_OUT_TRANSFORM_ROWS][RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
    rt_video_out_transform_row banks[RT_VIDEO_OUT_TRANSFORM_BANKS][RT_VIDEO_OUT_TRANSFORM_ROWS];
} rt_video_out_transform_state;

typedef struct {
    uint8_t control_00[RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
    rt_video_out_transform_row rows[RT_VIDEO_OUT_TRANSFORM_ROWS];
    uint8_t tail_284[RT_VIDEO_OUT_TRANSFORM_TAIL_BYTES];
    uint8_t word_294[RT_VIDEO_OUT_TRANSFORM_WORD_BYTES];
} rt_video_out_transform_staging_prefix;

#ifdef __cplusplus
#define RT_VIDEO_OUT_TRANSFORM_ASSERT static_assert
extern "C" {
#else
#define RT_VIDEO_OUT_TRANSFORM_ASSERT _Static_assert
#endif
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_row, leading_00) == 0x00, "row vector");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_row, word_20) == 0x20, "row word 20");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_row, word_24) == 0x24, "row word 24");
RT_VIDEO_OUT_TRANSFORM_ASSERT(sizeof(rt_video_out_transform_row) == 0x28, "row stride");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, header_00) == 0x00, "header 00");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, header_08) == 0x08, "header 08");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, unknown_10) == 0x10, "unknown 10");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, header_20) == 0x20, "header 20");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, row_count_28) == 0x28, "row count");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, lane_2c) == 0x2c, "lane 2c");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, lane_6c) == 0x6c, "lane 6c");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, lane_ac) == 0xac, "lane ac");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_state, banks) == 0xec, "first bank");
RT_VIDEO_OUT_TRANSFORM_ASSERT(sizeof(rt_video_out_transform_state) == 0xaec, "state prefix");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_staging_prefix, control_00) == 0x00, "control");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_staging_prefix, rows) == 0x04, "staging rows");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_staging_prefix, tail_284) == 0x284, "staging tail");
RT_VIDEO_OUT_TRANSFORM_ASSERT(offsetof(rt_video_out_transform_staging_prefix, word_294) == 0x294, "staging word");
RT_VIDEO_OUT_TRANSFORM_ASSERT(sizeof(rt_video_out_transform_staging_prefix) == 0x298, "staging prefix");
#undef RT_VIDEO_OUT_TRANSFORM_ASSERT

/* Native wire returns the address of the first state bank; the caller ignores it.
   This is an internal helper, without a public SCE export or device side effects. */
uint64_t rt_video_out_transform_initialize(void *state_storage, void *staging_storage);

#ifdef __cplusplus
}
#endif
#endif
