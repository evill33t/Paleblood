/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_DIRECT_MEMORY_H
#define RUNTIME_DIRECT_MEMORY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit Paleblood port policy, not a model of the console physical-memory manager.
   A configured temporary file supplies this many logical bytes; resident RAM is not reserved.
   Core calls require caller serialization and valid caller-owned input/output slots. */
#define RT_DIRECT_PAGE_SIZE UINT64_C(16384)
#define RT_DIRECT_MAX_MAP_ALIGNMENT UINT64_C(1073741824)

typedef struct rt_direct_pool rt_direct_pool;
typedef enum {
    RT_DIRECT_OK,
    RT_DIRECT_INVALID,
    RT_DIRECT_NO_MEMORY,
    RT_DIRECT_UNSUPPORTED_TYPE,
    RT_DIRECT_UNSUPPORTED_MAPPING,
    RT_DIRECT_UNSUPPORTED_RELEASE,
    RT_DIRECT_MAPPED_ALLOCATION,
    RT_DIRECT_UNSUPPORTED_HOST,
    RT_DIRECT_HOST_ERROR,
    RT_DIRECT_ALREADY_CONFIGURED,
} rt_direct_result;

/* Strict positive decimal, a 16 KiB multiple representable as a POSIX file length.
   Output is unchanged on failure. */
rt_direct_result rt_direct_parse_capacity(const char *text, uint64_t *capacity);
/* *pool must initially be NULL; an owned pool is never overwritten. */
rt_direct_result rt_direct_pool_create(uint64_t capacity, rt_direct_pool **pool);
uint64_t rt_direct_pool_capacity(const rt_direct_pool *pool);
/* On an unmap failure, retains the pool and remaining mappings for a later cleanup attempt.
   If only fclose fails, resources have been consumed and *pool is NULL; the error is reported. */
rt_direct_result rt_direct_pool_destroy(rt_direct_pool **pool);

/* [start,end) is end-exclusive; all geometry is in bytes and uses full uint64_t values.
   Alignment 0 means 16 KiB. Only type 0 is supported. First fit, no overcommit of this ledger.
   out_offset is an 8-byte slot, possibly unaligned, written only after success. */
rt_direct_result rt_direct_pool_allocate(rt_direct_pool *pool, uint64_t start, uint64_t end,
                                        uint64_t length, uint64_t alignment, int32_t type,
                                        void *out_offset);
/* Only CPU read/write protection 3 and nonfixed flags 0 are supported. The input 8-byte
   address slot is a hint, including nonnull hints; it never authorizes clobbering other memory.
   Maps must be wholly contained in one live physical allocation. Aliases share backing bytes.
   Writes the full address only after success. No GPU coherency or target hint equivalence. */
rt_direct_result rt_direct_pool_map(rt_direct_pool *pool, void *inout_address, uint64_t length,
                                   int32_t protection, int32_t flags, uint64_t offset,
                                   uint64_t alignment);
/* Core-only exact map removal, for owned consumers and tests. No guest munmap API is added. */
rt_direct_result rt_direct_pool_unmap(rt_direct_pool *pool, void *address, uint64_t length);
/* Only an entire, currently unmapped allocation may be released. Backing bytes are retained
   for reuse; neither reuse zeroing nor console alias-lifetime semantics are claimed. */
rt_direct_result rt_direct_pool_release(rt_direct_pool *pool, uint64_t offset, uint64_t length);

/* The registry owns one pool and serializes its calls and shutdown. No implicit capacity.
   Configure before guest execution; shutdown when that consumer is finished. */
rt_direct_result rt_direct_runtime_configure(uint64_t capacity);
rt_direct_result rt_direct_runtime_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
