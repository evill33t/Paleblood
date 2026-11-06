/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Owned GFX7 programs for the Metal executor's profile checks. Encodings from LLVM:
   llvm-mc -triple=amdgcn -mcpu=bonaire --show-encoding. No game code is used. */
#ifndef TEST_METAL_EXECUTOR_PROGRAMS_H
#define TEST_METAL_EXECUTOR_PROGRAMS_H
#include <stdint.h>

static const uint32_t vertex_clip_eight[] = {
    0xebf32000, 0x80000100, /* tbuffer_load_format_xyzw v[1:4], v0, s[0:3], 0 format:[BUF_DATA_FORMAT_32_32_32_32,BUF_NUM_FORMAT_FLOAT] idxen */
    0xebf32000, 0x80010500, /* tbuffer_load_format_xyzw v[5:8], v0, s[4:7], 0 format:[BUF_DATA_FORMAT_32_32_32_32,BUF_NUM_FORMAT_FLOAT] idxen */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0xf800020f, 0x08070605, /* exp param0, v5, v6, v7, v8 */
    0xf80000cf, 0x04030201, /* exp pos0, v1, v2, v3, v4 */
    0xf80000df, 0x02010201, /* exp pos1, v1, v2, v1, v2 */
    0xf80008ef, 0x02010201, /* exp pos2, v1, v2, v1, v2 done */
    0xbf810000, /* s_endpgm */
};

static const uint32_t vertex_two_parameters[] = {
    0xebf32000, 0x80000100, /* tbuffer_load_format_xyzw v[1:4], v0, s[0:3], 0 format:[BUF_DATA_FORMAT_32_32_32_32,BUF_NUM_FORMAT_FLOAT] idxen */
    0xebf32000, 0x80010500, /* tbuffer_load_format_xyzw v[5:8], v0, s[4:7], 0 format:[BUF_DATA_FORMAT_32_32_32_32,BUF_NUM_FORMAT_FLOAT] idxen */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0xf800020f, 0x08070605, /* exp param0, v5, v6, v7, v8 */
    0xf800021f, 0x08060507, /* exp param1, v7, v5, v6, v8 */
    0xf80008cf, 0x04030201, /* exp pos0, v1, v2, v3, v4 done */
    0xbf810000, /* s_endpgm */
};

static const uint32_t pixel_two_parameters[] = {
    0xbefc0300, /* s_mov_b32 m0, s0 */
    0xc8080100, /* v_interp_p1_f32 v2, v0, attr0.y */
    0xc8090101, /* v_interp_p2_f32 v2, v1, attr0.y */
    0xc80c0200, /* v_interp_p1_f32 v3, v0, attr0.z */
    0xc80d0201, /* v_interp_p2_f32 v3, v1, attr0.z */
    0xc8100400, /* v_interp_p1_f32 v4, v0, attr1.x */
    0xc8110401, /* v_interp_p2_f32 v4, v1, attr1.x */
    0xc8140500, /* v_interp_p1_f32 v5, v0, attr1.y */
    0xc8150501, /* v_interp_p2_f32 v5, v1, attr1.y */
    0x060c0903, /* v_add_f32_e32 v6, v3, v4 */
    0x7e0e02f2, /* v_mov_b32_e32 v7, 1.0 */
    0x5e100b02, /* v_cvt_pkrtz_f16_f32_e32 v8, v2, v5 */
    0x5e120f06, /* v_cvt_pkrtz_f16_f32_e32 v9, v6, v7 */
    0xf8001c0f, 0x00000908, /* exp mrt0, v8, v8, v9, v9 done compr vm */
    0xbf810000, /* s_endpgm */
};

static const uint32_t pixel_user_float32[] = {
    0x7e040200, /* v_mov_b32_e32 v2, s0 */
    0x7e060201, /* v_mov_b32_e32 v3, s1 */
    0x7e080202, /* v_mov_b32_e32 v4, s2 */
    0x7e0a0203, /* v_mov_b32_e32 v5, s3 */
    0xf800180f, 0x05040302, /* exp mrt0, v2, v3, v4, v5 done vm */
    0xbf810000, /* s_endpgm */
};

static const uint32_t vertex_descriptor_format[] = {
    0xe00c2000, 0x80000100, /* buffer_load_format_xyzw v[1:4], v0, s[0:3], 0 idxen */
    0xe00c2000, 0x80010500, /* buffer_load_format_xyzw v[5:8], v0, s[4:7], 0 idxen */
    0xbf8c0f70, /* s_waitcnt vmcnt(0)                      */
    0xf800020f, 0x08070605, /* exp param0, v5, v6, v7, v8              */
    0xf80008cf, 0x04030201, /* exp pos0, v1, v2, v3, v4 done           */
    0xbf810000, /* s_endpgm                                */
};

static const uint32_t vertex_store_format[] = {
    0xebf32000, 0x80000100, /* tbuffer_load_format_xyzw v[1:4], v0, s[0:3], 0 format:[BUF_DATA_FORMAT_32_32_32_32,BUF_NUM_FORMAT_FLOAT] idxen */
    0xebf32000, 0x80010500, /* tbuffer_load_format_xyzw v[5:8], v0, s[4:7], 0 format:[BUF_DATA_FORMAT_32_32_32_32,BUF_NUM_FORMAT_FLOAT] idxen */
    0xbf8c0f70, /* s_waitcnt vmcnt(0)                      */
    0xe01c2000, 0x80020500, /* buffer_store_format_xyzw v[5:8], v0, s[8:11], 0 idxen */
    0xf800020f, 0x08070605, /* exp param0, v5, v6, v7, v8              */
    0xf80008cf, 0x04030201, /* exp pos0, v1, v2, v3, v4 done           */
    0xbf810000, /* s_endpgm                                */
};

static const uint32_t pixel_descriptor_format[] = {
    0x7e040280, /* v_mov_b32_e32 v2, 0                     */
    0xe00c2000, 0x80000202, /* buffer_load_format_xyzw v[2:5], v2, s[0:3], 0 idxen */
    0xbf8c0f70, /* s_waitcnt vmcnt(0)                      */
    0xf800180f, 0x05040302, /* exp mrt0, v2, v3, v4, v5 done vm        */
    0xbf810000, /* s_endpgm                                */
};

#endif
