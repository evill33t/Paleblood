/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include "runtime/direct_memory.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

_Static_assert(sizeof(uintptr_t) == 8, "direct memory requires 64-bit address slots");
_Static_assert(sizeof(off_t) <= sizeof(uint64_t), "unsupported POSIX file offset width");

typedef struct allocation {
    uint64_t offset, length;
    struct allocation *next;
} allocation;

typedef struct mapping {
    void *reservation, *address;
    size_t reservation_size;
    uint64_t length;
    allocation *owner;
    struct mapping *next;
} mapping;

struct rt_direct_pool {
    FILE *backing;
    int fd;
    uint64_t capacity;
    allocation *allocations;
    mapping *mappings;
};

static int valid_capacity(uint64_t capacity)
{
    const uint64_t max_offset = (UINT64_C(1) << (sizeof(off_t) * CHAR_BIT - 1)) - 1;
    return capacity && !(capacity % RT_DIRECT_PAGE_SIZE) && capacity <= max_offset;
}

static int power_of_two(uint64_t value) { return value && !(value & (value - 1)); }

static int align_up(uint64_t value, uint64_t alignment, uint64_t *out)
{
    const uint64_t mask = alignment - 1;
    if (value > UINT64_MAX - mask) return 0;
    *out = (value + mask) & ~mask;
    return 1;
}

static int geometry(uint64_t offset, uint64_t length)
{
    return length && !(offset % RT_DIRECT_PAGE_SIZE) && !(length % RT_DIRECT_PAGE_SIZE)
           && offset <= UINT64_MAX - length;
}

rt_direct_result rt_direct_parse_capacity(const char *text, uint64_t *capacity)
{
    if (!text || !*text || !capacity) return RT_DIRECT_INVALID;
    uint64_t value = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p < '0' || *p > '9') return RT_DIRECT_INVALID;
        const uint64_t digit = (uint64_t)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10) return RT_DIRECT_INVALID;
        value = value * 10 + digit;
    }
    if (!valid_capacity(value)) return RT_DIRECT_INVALID;
    *capacity = value;
    return RT_DIRECT_OK;
}

rt_direct_result rt_direct_pool_create(uint64_t capacity, rt_direct_pool **out)
{
    if (!out || *out || !valid_capacity(capacity)) return RT_DIRECT_INVALID;
    const long host_page = sysconf(_SC_PAGESIZE);
    if (host_page <= 0 || !power_of_two((uint64_t)host_page)
        || RT_DIRECT_PAGE_SIZE % (uint64_t)host_page) return RT_DIRECT_UNSUPPORTED_HOST;
    rt_direct_pool *pool = calloc(1, sizeof *pool);
    if (!pool) return RT_DIRECT_NO_MEMORY;
    pool->backing = tmpfile();
    if (!pool->backing) {
        free(pool);
        return RT_DIRECT_HOST_ERROR;
    }
    pool->fd = fileno(pool->backing);
    if (pool->fd < 0 || ftruncate(pool->fd, (off_t)capacity)) {
        /* fclose owns the descriptor, even when capacity initialization failed. */
        (void)fclose(pool->backing);
        free(pool);
        return RT_DIRECT_HOST_ERROR;
    }
    pool->capacity = capacity;
    *out = pool;
    return RT_DIRECT_OK;
}

uint64_t rt_direct_pool_capacity(const rt_direct_pool *pool) { return pool ? pool->capacity : 0; }

rt_direct_result rt_direct_pool_allocate(rt_direct_pool *pool, uint64_t start, uint64_t end,
                                        uint64_t length, uint64_t alignment, int32_t type,
                                        void *out_offset)
{
    if (type != 0) return RT_DIRECT_UNSUPPORTED_TYPE;
    if (!alignment) alignment = RT_DIRECT_PAGE_SIZE;
    if (!pool || !out_offset || start >= end || end > pool->capacity
        || end % RT_DIRECT_PAGE_SIZE || !geometry(start, length)
        || !power_of_two(alignment) || alignment < RT_DIRECT_PAGE_SIZE) return RT_DIRECT_INVALID;
    uint64_t candidate;
    if (!align_up(start, alignment, &candidate)) return RT_DIRECT_NO_MEMORY;
    allocation **link = &pool->allocations;
    while (*link) {
        const allocation *current = *link;
        if (candidate <= current->offset && length <= current->offset - candidate) break;
        const uint64_t after = current->offset + current->length; /* ledger is checked on insertion */
        if (candidate < after && !align_up(after, alignment, &candidate)) return RT_DIRECT_NO_MEMORY;
        link = &(*link)->next;
    }
    if (candidate >= end || length > end - candidate) return RT_DIRECT_NO_MEMORY;
    allocation *item = calloc(1, sizeof *item);
    if (!item) return RT_DIRECT_NO_MEMORY;
    item->offset = candidate;
    item->length = length;
    item->next = *link;
    *link = item;
    memcpy(out_offset, &candidate, sizeof candidate);
    return RT_DIRECT_OK;
}

static allocation *containing_allocation(rt_direct_pool *pool, uint64_t offset, uint64_t length)
{
    for (allocation *item = pool->allocations; item; item = item->next)
        if (offset >= item->offset && offset - item->offset < item->length
            && length <= item->length - (offset - item->offset)) return item;
    return NULL;
}

rt_direct_result rt_direct_pool_map(rt_direct_pool *pool, void *inout_address, uint64_t length,
                                   int32_t protection, int32_t flags, uint64_t offset,
                                   uint64_t alignment)
{
    if (protection != 3 || flags != 0) return RT_DIRECT_UNSUPPORTED_MAPPING;
    if (!alignment) alignment = RT_DIRECT_PAGE_SIZE;
    if (!pool || !inout_address || !geometry(offset, length)
        || !power_of_two(alignment) || alignment < RT_DIRECT_PAGE_SIZE
        || alignment > RT_DIRECT_MAX_MAP_ALIGNMENT || length > SIZE_MAX - alignment)
        return RT_DIRECT_INVALID;
    uint64_t hint;
    memcpy(&hint, inout_address, sizeof hint);
    if (hint % RT_DIRECT_PAGE_SIZE) return RT_DIRECT_INVALID;
    allocation *owner = containing_allocation(pool, offset, length);
    if (!owner) return RT_DIRECT_INVALID;
    mapping *item = calloc(1, sizeof *item);
    if (!item) return RT_DIRECT_NO_MEMORY;
    item->length = length;
    item->owner = owner;
    item->reservation_size = (size_t)(length + alignment);
    item->reservation = mmap((void *)(uintptr_t)hint, item->reservation_size, PROT_NONE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (item->reservation == MAP_FAILED) {
        free(item);
        return RT_DIRECT_NO_MEMORY;
    }
    /* A zero mapping cannot be represented by our nonnull published-map handle policy. */
    if (!item->reservation) {
        if (munmap(item->reservation, item->reservation_size)) {
            item->next = pool->mappings;
            pool->mappings = item;
            return RT_DIRECT_HOST_ERROR;
        }
        free(item);
        return RT_DIRECT_UNSUPPORTED_HOST;
    }
    uint64_t address;
    const uint64_t base = (uint64_t)(uintptr_t)item->reservation;
    if (!align_up(base, alignment, &address) || address < base
        || address - base > item->reservation_size - length) {
        /* Retain ownership if the host rejects cleanup; destroy can retry it. */
        if (munmap(item->reservation, item->reservation_size)) {
            item->next = pool->mappings;
            pool->mappings = item;
        } else free(item);
        return RT_DIRECT_HOST_ERROR;
    }
    item->address = (void *)(uintptr_t)address;
    /* MAP_FIXED replaces only bytes in the PROT_NONE reservation we just acquired.
       Prefix and suffix remain owned guards until this map is removed or the pool is destroyed. */
    void *mapped = mmap(item->address, (size_t)length, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_FIXED, pool->fd, (off_t)offset);
    if (mapped == MAP_FAILED) {
        if (munmap(item->reservation, item->reservation_size)) {
            item->address = NULL;
            item->next = pool->mappings;
            pool->mappings = item;
            return RT_DIRECT_HOST_ERROR;
        }
        free(item);
        return RT_DIRECT_NO_MEMORY;
    }
    /* POSIX MAP_FIXED returns the requested address on success. */
    item->next = pool->mappings;
    pool->mappings = item;
    memcpy(inout_address, &address, sizeof address);
    return RT_DIRECT_OK;
}

rt_direct_result rt_direct_pool_unmap(rt_direct_pool *pool, void *address, uint64_t length)
{
    if (!pool || !address || !length) return RT_DIRECT_INVALID;
    mapping **link = &pool->mappings;
    while (*link && (*link)->address != address) link = &(*link)->next;
    if (!*link) return RT_DIRECT_INVALID;
    mapping *item = *link;
    if (item->length != length) return RT_DIRECT_UNSUPPORTED_RELEASE;
    if (munmap(item->reservation, item->reservation_size)) return RT_DIRECT_HOST_ERROR;
    *link = item->next;
    free(item);
    return RT_DIRECT_OK;
}

rt_direct_result rt_direct_pool_release(rt_direct_pool *pool, uint64_t offset, uint64_t length)
{
    if (!pool || !geometry(offset, length)) return RT_DIRECT_INVALID;
    allocation **link = &pool->allocations;
    while (*link && (*link)->offset != offset) link = &(*link)->next;
    if (!*link) {
        for (allocation *item = pool->allocations; item; item = item->next)
            if (offset > item->offset && offset - item->offset < item->length)
                return RT_DIRECT_UNSUPPORTED_RELEASE;
        return RT_DIRECT_INVALID;
    }
    allocation *item = *link;
    if (length != item->length) return RT_DIRECT_UNSUPPORTED_RELEASE;
    for (mapping *map = pool->mappings; map; map = map->next)
        if (map->owner == item) return RT_DIRECT_MAPPED_ALLOCATION;
    *link = item->next;
    free(item);
    return RT_DIRECT_OK;
}

rt_direct_result rt_direct_pool_destroy(rt_direct_pool **out)
{
    if (!out) return RT_DIRECT_INVALID;
    rt_direct_pool *pool = *out;
    if (!pool) return RT_DIRECT_OK;
    mapping **link = &pool->mappings;
    int failed = 0;
    while (*link) {
        mapping *item = *link;
        if (munmap(item->reservation, item->reservation_size)) {
            failed = 1;
            link = &item->next;
        } else {
            *link = item->next;
            free(item);
        }
    }
    if (failed) return RT_DIRECT_HOST_ERROR;
    while (pool->allocations) {
        allocation *item = pool->allocations;
        pool->allocations = item->next;
        free(item);
    }
    const int closed = fclose(pool->backing);
    free(pool);
    *out = NULL;
    return closed ? RT_DIRECT_HOST_ERROR : RT_DIRECT_OK;
}
