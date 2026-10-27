// SPDX-License-Identifier: GPL-2.0-or-later
// The upload arena partitions 64 KiB blocks into lists selected by remaining capacity.
// List indexing uses integer byte addresses: the native zero-size path can select 16,
// which aliases the full-block head immediately after the sixteen ordinary bins.
#pragma once

#include "secondary_calls.h"

namespace render {
struct ShaderUploadManager;
struct ShaderUploadBlock;

struct ShaderUploadBlockFields {
    ShaderField<uint64_t> used;
    ShaderField<uintptr_t> data;
    uint8_t unknown_0x10[0x18];
    ShaderField<ShaderUploadBlock *> next;
};
static_assert(offsetof(ShaderUploadBlockFields, used) == 0x00);
static_assert(offsetof(ShaderUploadBlockFields, data) == 0x08);
static_assert(offsetof(ShaderUploadBlockFields, next) == 0x28);
static_assert(sizeof(ShaderUploadBlockFields) == 0x30);

struct ShaderUploadArenaFields {
    ShaderField<ShaderUploadManager *> manager;
    ShaderField<ShaderUploadBlock *> bins[16];
    ShaderField<ShaderUploadBlock *> full_blocks;
    ShaderField<uint32_t> nonempty_bins;
};
static_assert(offsetof(ShaderUploadArenaFields, manager) == 0x00);
static_assert(offsetof(ShaderUploadArenaFields, bins) == 0x08);
static_assert(offsetof(ShaderUploadArenaFields, full_blocks) == 0x88);
static_assert(offsetof(ShaderUploadArenaFields, nonempty_bins) == 0x90);
static_assert(sizeof(ShaderUploadArenaFields) == 0x94);

RT_ORIGINAL(0x02aa9d50, acquire_shader_upload_block,
            ShaderUploadBlock *(ShaderUploadManager *, uint32_t));
RT_ORIGINAL(0x02aaa860, reclaim_shader_upload_blocks,
            void(ShaderUploadManager *, RecordCommandBuffer **));

inline ShaderField<ShaderUploadBlock *> *shader_upload_bin(ShaderUploadArenaFields *arena,
                                                           uint32_t index)
{
    const uintptr_t address = reinterpret_cast<uintptr_t>(arena) +
                              offsetof(ShaderUploadArenaFields, bins) +
                              uint64_t(index) * sizeof(ShaderField<ShaderUploadBlock *>);
    return reinterpret_cast<ShaderField<ShaderUploadBlock *> *>(address);
}
}  // namespace render
