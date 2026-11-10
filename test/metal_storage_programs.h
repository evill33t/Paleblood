/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Owned GFX7 programs assembled with LLVM llvm-mc -triple=amdgcn -mcpu=bonaire. */
#ifndef PB_METAL_STORAGE_PROGRAMS_H
#define PB_METAL_STORAGE_PROGRAMS_H
#include <stdint.h>

static const uint32_t storage_load_1d[] = {
    0x7e060300, /* v_mov_b32_e32 v3, v0 */
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0xf0000f00, 0x00020400, /* image_load v[4:7], v0, s[8:15] dmask:0xf */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x34060684, /* v_lshlrev_b32_e32 v3, 4, v3 */
    0xe0781000, 0x80000403, /* buffer_store_dwordx4 v[4:7], v3, s[0:3], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_store_1d[] = {
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e080200, /* v_mov_b32_e32 v4, s0 */
    0x7e0a0201, /* v_mov_b32_e32 v5, s1 */
    0x7e0c0202, /* v_mov_b32_e32 v6, s2 */
    0x7e0e0203, /* v_mov_b32_e32 v7, s3 */
    0xf0200f00, 0x00020400, /* image_store v[4:7], v0, s[8:15] dmask:0xf */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_load_2d[] = {
    0x7e060300, /* v_mov_b32_e32 v3, v0 */
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020204, /* v_mov_b32_e32 v1, s4 */
    0xf0000f00, 0x00020400, /* image_load v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x34060684, /* v_lshlrev_b32_e32 v3, 4, v3 */
    0xe0781000, 0x80000403, /* buffer_store_dwordx4 v[4:7], v3, s[0:3], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_store_2d[] = {
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020204, /* v_mov_b32_e32 v1, s4 */
    0x7e080200, /* v_mov_b32_e32 v4, s0 */
    0x7e0a0201, /* v_mov_b32_e32 v5, s1 */
    0x7e0c0202, /* v_mov_b32_e32 v6, s2 */
    0x7e0e0203, /* v_mov_b32_e32 v7, s3 */
    0xf0200f00, 0x00020400, /* image_store v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_load_1d_array[] = {
    0x7e060300, /* v_mov_b32_e32 v3, v0 */
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020205, /* v_mov_b32_e32 v1, s5 */
    0xf0004f00, 0x00020400, /* image_load v[4:7], v[0:1], s[8:15] dmask:0xf da */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x34060684, /* v_lshlrev_b32_e32 v3, 4, v3 */
    0xe0781000, 0x80000403, /* buffer_store_dwordx4 v[4:7], v3, s[0:3], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_store_1d_array[] = {
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020205, /* v_mov_b32_e32 v1, s5 */
    0x7e080200, /* v_mov_b32_e32 v4, s0 */
    0x7e0a0201, /* v_mov_b32_e32 v5, s1 */
    0x7e0c0202, /* v_mov_b32_e32 v6, s2 */
    0x7e0e0203, /* v_mov_b32_e32 v7, s3 */
    0xf0204f00, 0x00020400, /* image_store v[4:7], v[0:1], s[8:15] dmask:0xf da */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_load_2d_array[] = {
    0x7e060300, /* v_mov_b32_e32 v3, v0 */
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020204, /* v_mov_b32_e32 v1, s4 */
    0x7e040205, /* v_mov_b32_e32 v2, s5 */
    0xf0004f00, 0x00020400, /* image_load v[4:7], v[0:2], s[8:15] dmask:0xf da */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x34060684, /* v_lshlrev_b32_e32 v3, 4, v3 */
    0xe0781000, 0x80000403, /* buffer_store_dwordx4 v[4:7], v3, s[0:3], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_store_2d_array[] = {
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020204, /* v_mov_b32_e32 v1, s4 */
    0x7e040205, /* v_mov_b32_e32 v2, s5 */
    0x7e080200, /* v_mov_b32_e32 v4, s0 */
    0x7e0a0201, /* v_mov_b32_e32 v5, s1 */
    0x7e0c0202, /* v_mov_b32_e32 v6, s2 */
    0x7e0e0203, /* v_mov_b32_e32 v7, s3 */
    0xf0204f00, 0x00020400, /* image_store v[4:7], v[0:2], s[8:15] dmask:0xf da */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_inplace[] = {
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020204, /* v_mov_b32_e32 v1, s4 */
    0xf0000f00, 0x00020400, /* image_load v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x060808f0, /* v_add_f32_e32 v4, 0.5, v4 */
    0xf0200f00, 0x00020400, /* image_store v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_alias_copy[] = {
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0x7e040280, /* v_mov_b32_e32 v2, 0 */
    0xf0000f00, 0x00000401, /* image_load v[4:7], v[1:2], s[0:7] dmask:0xf */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e020281, /* v_mov_b32_e32 v1, 1 */
    0xf0200f00, 0x00020401, /* image_store v[4:7], v[1:2], s[8:15] dmask:0xf */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_store_then_stop[] = {
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0x7e0802f0, /* v_mov_b32_e32 v4, 0.5 */
    0x7e0a02f0, /* v_mov_b32_e32 v5, 0.5 */
    0x7e0c02f0, /* v_mov_b32_e32 v6, 0.5 */
    0x7e0e02f0, /* v_mov_b32_e32 v7, 0.5 */
    0xf0200f00, 0x00020400, /* image_store v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xe0001000, 0x80000800, /* buffer_load_format_x v8, v0, s[0:3], 0 offen */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_load_memory[] = {
    0xc0c40508, /* s_load_dwordx8 s[8:15], s[4:5], 0x8 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0xf0000f00, 0x00020400, /* image_load v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x34060084, /* v_lshlrev_b32_e32 v3, 4, v0 */
    0xe0781000, 0x80000403, /* buffer_store_dwordx4 v[4:7], v3, s[0:3], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_8_images[] = {
    0x7e000280, /* v_mov_b32_e32 v0, 0 */
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0xc0c40100, /* s_load_dwordx8 s[8:15], s[0:1], 0x0 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060280, /* v_mov_b32_e32 v3, 0 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40108, /* s_load_dwordx8 s[8:15], s[0:1], 0x8 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060284, /* v_mov_b32_e32 v3, 4 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40110, /* s_load_dwordx8 s[8:15], s[0:1], 0x10 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060288, /* v_mov_b32_e32 v3, 8 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40118, /* s_load_dwordx8 s[8:15], s[0:1], 0x18 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e06028c, /* v_mov_b32_e32 v3, 12 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40120, /* s_load_dwordx8 s[8:15], s[0:1], 0x20 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060290, /* v_mov_b32_e32 v3, 16 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40128, /* s_load_dwordx8 s[8:15], s[0:1], 0x28 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060294, /* v_mov_b32_e32 v3, 20 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40130, /* s_load_dwordx8 s[8:15], s[0:1], 0x30 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060298, /* v_mov_b32_e32 v3, 24 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40138, /* s_load_dwordx8 s[8:15], s[0:1], 0x38 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e06029c, /* v_mov_b32_e32 v3, 28 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_9_images[] = {
    0x7e000280, /* v_mov_b32_e32 v0, 0 */
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0xc0c40100, /* s_load_dwordx8 s[8:15], s[0:1], 0x0 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060280, /* v_mov_b32_e32 v3, 0 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40108, /* s_load_dwordx8 s[8:15], s[0:1], 0x8 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060284, /* v_mov_b32_e32 v3, 4 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40110, /* s_load_dwordx8 s[8:15], s[0:1], 0x10 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060288, /* v_mov_b32_e32 v3, 8 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40118, /* s_load_dwordx8 s[8:15], s[0:1], 0x18 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e06028c, /* v_mov_b32_e32 v3, 12 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40120, /* s_load_dwordx8 s[8:15], s[0:1], 0x20 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060290, /* v_mov_b32_e32 v3, 16 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40128, /* s_load_dwordx8 s[8:15], s[0:1], 0x28 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060294, /* v_mov_b32_e32 v3, 20 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40130, /* s_load_dwordx8 s[8:15], s[0:1], 0x30 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e060298, /* v_mov_b32_e32 v3, 24 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40138, /* s_load_dwordx8 s[8:15], s[0:1], 0x38 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e06029c, /* v_mov_b32_e32 v3, 28 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xc0c40140, /* s_load_dwordx8 s[8:15], s[0:1], 0x40 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0xf0000100, 0x00020400, /* image_load v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x7e0602a0, /* v_mov_b32_e32 v3, 32 */
    0xe0701000, 0x80010403, /* buffer_store_dword v4, v3, s[4:7], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_store_and_buffer[] = {
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0x7e0802f0, /* v_mov_b32_e32 v4, 0.5 */
    0x7e0a02f0, /* v_mov_b32_e32 v5, 0.5 */
    0x7e0c02f0, /* v_mov_b32_e32 v6, 0.5 */
    0x7e0e02f0, /* v_mov_b32_e32 v7, 0.5 */
    0xf0200f00, 0x00020400, /* image_store v[4:7], v[0:1], s[8:15] dmask:0xf */
    0x7e060280, /* v_mov_b32_e32 v3, 0 */
    0xe0701000, 0x80000403, /* buffer_store_dword v4, v3, s[0:3], 0 offen */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_8_stores[] = {
    0x7e000280, /* v_mov_b32_e32 v0, 0 */
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0xc0c40100, /* s_load_dwordx8 s[8:15], s[0:1], 0x0 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802ff, 0x3d800000, /* v_mov_b32_e32 v4, 0x3d800000 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xc0c40108, /* s_load_dwordx8 s[8:15], s[0:1], 0x8 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802ff, 0x3e000000, /* v_mov_b32_e32 v4, 0x3e000000 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xc0c40110, /* s_load_dwordx8 s[8:15], s[0:1], 0x10 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802ff, 0x3e400000, /* v_mov_b32_e32 v4, 0x3e400000 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xc0c40118, /* s_load_dwordx8 s[8:15], s[0:1], 0x18 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802ff, 0x3e800000, /* v_mov_b32_e32 v4, 0x3e800000 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xc0c40120, /* s_load_dwordx8 s[8:15], s[0:1], 0x20 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802ff, 0x3ea00000, /* v_mov_b32_e32 v4, 0x3ea00000 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xc0c40128, /* s_load_dwordx8 s[8:15], s[0:1], 0x28 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802ff, 0x3ec00000, /* v_mov_b32_e32 v4, 0x3ec00000 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xc0c40130, /* s_load_dwordx8 s[8:15], s[0:1], 0x30 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802ff, 0x3ee00000, /* v_mov_b32_e32 v4, 0x3ee00000 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xc0c40138, /* s_load_dwordx8 s[8:15], s[0:1], 0x38 */
    0xbf8c007f, /* s_waitcnt lgkmcnt(0) */
    0x7e0802f0, /* v_mov_b32_e32 v4, 0.5 */
    0xf0200100, 0x00020400, /* image_store v4, v[0:1], s[8:15] dmask:0x1 */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_inplace_bits[] = {
    0x4a000006, /* v_add_i32_e32 v0, vcc, s6, v0 */
    0x7e020204, /* v_mov_b32_e32 v1, s4 */
    0xf0000f00, 0x00020400, /* image_load v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xbf8c0f70, /* s_waitcnt vmcnt(0) */
    0x4a080881, /* v_add_i32_e32 v4, vcc, 1, v4 */
    0xf0200f00, 0x00020400, /* image_store v[4:7], v[0:1], s[8:15] dmask:0xf */
    0xbf810000, /* s_endpgm */
};

static const uint32_t storage_array_store_and_buffer[] = {
    0x7e020280, /* v_mov_b32_e32 v1, 0 */
    0x7e040280, /* v_mov_b32_e32 v2, 0 */
    0x7e0802f0, /* v_mov_b32_e32 v4, 0.5 */
    0x7e0a02f0, /* v_mov_b32_e32 v5, 0.5 */
    0x7e0c02f0, /* v_mov_b32_e32 v6, 0.5 */
    0x7e0e02f0, /* v_mov_b32_e32 v7, 0.5 */
    0xf0204f00, 0x00020400, /* image_store v[4:7], v[0:1], s[8:15] dmask:0xf da */
    0x7e060280, /* v_mov_b32_e32 v3, 0 */
    0xe0701000, 0x80000403, /* buffer_store_dword v4, v3, s[0:3], 0 offen */
    0xbf810000, /* s_endpgm */
};

#endif
