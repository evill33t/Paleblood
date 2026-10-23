/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The production core is compiled once with private libc-name aliases for this target only.
   Test wrappers inject host failures; no production hooks or alternative allocator are used. */
#define _GNU_SOURCE
#include "runtime/direct_memory.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static int failures, mmap_calls, munmap_calls, calloc_calls, close_calls, open_files;
static int fail_mmap, fail_munmap, fail_calloc, fail_tmpfile, fail_truncate, fail_close;
static int return_zero_once;
static size_t modeled_zero_size;
static struct { void *base; size_t length; } reservations[32];
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

void *rt_test_calloc(size_t count, size_t size)
{
    if (++calloc_calls == fail_calloc) { errno = ENOMEM; return NULL; }
    return calloc(count, size);
}
FILE *rt_test_tmpfile(void)
{
    if (fail_tmpfile) { errno = EMFILE; return NULL; }
    FILE *file = tmpfile();
    if (file) open_files++;
    return file;
}
int rt_test_ftruncate(int fd, off_t length)
{
    if (fail_truncate) { errno = EIO; return -1; }
    return ftruncate(fd, length);
}
int rt_test_fclose(FILE *file)
{
    close_calls++;
    const int result = fclose(file);
    open_files--;
    if (fail_close) { errno = EIO; return EOF; }
    return result;
}
void *rt_test_mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset)
{
    mmap_calls++;
    if (flags & MAP_FIXED) {
        int owned = 0;
        const uintptr_t p = (uintptr_t)address;
        for (size_t i = 0; i < sizeof reservations / sizeof reservations[0]; i++) {
            const uintptr_t base = (uintptr_t)reservations[i].base;
            if (reservations[i].length && p >= base && p - base <= reservations[i].length
                && length <= reservations[i].length - (p - base)) owned = 1;
        }
        CHECK(owned && protection == (PROT_READ | PROT_WRITE) && (flags & MAP_SHARED));
        if (!owned) { errno = EINVAL; return MAP_FAILED; }
    }
    if (mmap_calls == fail_mmap) { errno = ENOMEM; return MAP_FAILED; }
    if (return_zero_once && !(flags & MAP_FIXED)) {
        return_zero_once = 0;
        modeled_zero_size = length; /* Model the rare host-zero result without mapping page zero. */
        return NULL;
    }
    void *result = mmap(address, length, protection, flags, fd, offset);
    if (result != MAP_FAILED && !(flags & MAP_FIXED)) {
        int recorded = 0;
        for (size_t i = 0; i < sizeof reservations / sizeof reservations[0]; i++)
            if (!reservations[i].length) {
                reservations[i].base = result;
                reservations[i].length = length;
                recorded = 1;
                break;
            }
        CHECK(recorded && protection == PROT_NONE);
    }
    return result;
}
int rt_test_munmap(void *address, size_t length)
{
    if (++munmap_calls == fail_munmap) { errno = EIO; return -1; }
    if (!address && length == modeled_zero_size && modeled_zero_size) {
        modeled_zero_size = 0;
        return 0;
    }
    const int result = munmap(address, length);
    if (!result)
        for (size_t i = 0; i < sizeof reservations / sizeof reservations[0]; i++)
            if (reservations[i].base == address && reservations[i].length == length)
                reservations[i].length = 0;
    return result;
}

static int live_reservations(void)
{
    int count = modeled_zero_size != 0;
    for (size_t i = 0; i < sizeof reservations / sizeof reservations[0]; i++)
        count += reservations[i].length != 0;
    return count;
}

int main(void)
{
    const uint64_t page = RT_DIRECT_PAGE_SIZE;
    rt_direct_pool *pool = NULL;
    fail_calloc = calloc_calls + 1;
    CHECK(rt_direct_pool_create(4 * page, &pool) == RT_DIRECT_NO_MEMORY && !pool);
    fail_calloc = 0;
    fail_tmpfile = 1;
    CHECK(rt_direct_pool_create(4 * page, &pool) == RT_DIRECT_HOST_ERROR && !pool);
    CHECK(open_files == 0);
    fail_tmpfile = 0;
    fail_truncate = 1;
    const int closes_before = close_calls;
    CHECK(rt_direct_pool_create(4 * page, &pool) == RT_DIRECT_HOST_ERROR && !pool);
    CHECK(open_files == 0 && close_calls == closes_before + 1);
    fail_truncate = 0;
    REQUIRE(rt_direct_pool_create(4 * page, &pool) == RT_DIRECT_OK);
    uint64_t offset = UINT64_C(0xfedcba9876543210), address = 0;
    fail_calloc = calloc_calls + 1;
    CHECK(rt_direct_pool_allocate(pool, 0, 4 * page, page, 0, 0, &offset) == RT_DIRECT_NO_MEMORY);
    CHECK(offset == UINT64_C(0xfedcba9876543210));
    fail_calloc = 0;
    REQUIRE(rt_direct_pool_allocate(pool, 0, 4 * page, page, 0, 0, &offset) == RT_DIRECT_OK);
    CHECK(offset == 0); /* Failed ledger insertion consumed no physical space. */
    return_zero_once = 1;
    CHECK(rt_direct_pool_map(pool, &address, page, 3, 0, offset, 0) == RT_DIRECT_UNSUPPORTED_HOST);
    CHECK(address == 0 && live_reservations() == 0);
    fail_calloc = calloc_calls + 1;
    CHECK(rt_direct_pool_map(pool, &address, page, 3, 0, offset, 0) == RT_DIRECT_NO_MEMORY);
    CHECK(address == 0 && live_reservations() == 0);
    fail_calloc = 0;
    fail_mmap = mmap_calls + 1;
    CHECK(rt_direct_pool_map(pool, &address, page, 3, 0, offset, 0) == RT_DIRECT_NO_MEMORY);
    CHECK(address == 0 && live_reservations() == 0);
    fail_mmap = mmap_calls + 2; /* Reserve succeeds, backing map fails. */
    CHECK(rt_direct_pool_map(pool, &address, page, 3, 0, offset, 0) == RT_DIRECT_NO_MEMORY);
    CHECK(address == 0 && live_reservations() == 0);
    REQUIRE(rt_direct_pool_release(pool, offset, page) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_allocate(pool, 0, 4 * page, page, 0, 0, &offset) == RT_DIRECT_OK);
    CHECK(offset == 0);
    fail_mmap = mmap_calls + 2;
    fail_munmap = munmap_calls + 1;
    CHECK(rt_direct_pool_map(pool, &address, page, 3, 0, offset, 0) == RT_DIRECT_HOST_ERROR);
    CHECK(address == 0 && live_reservations() == 1);
    CHECK(rt_direct_pool_release(pool, offset, page) == RT_DIRECT_MAPPED_ALLOCATION);
    fail_mmap = fail_munmap = 0;
    REQUIRE(rt_direct_pool_destroy(&pool) == RT_DIRECT_OK);
    CHECK(pool == NULL && live_reservations() == 0 && open_files == 0);

    REQUIRE(rt_direct_pool_create(4 * page, &pool) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_allocate(pool, 0, 4 * page, page, 0, 0, &offset) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_map(pool, &address, page, 3, 0, offset, 0) == RT_DIRECT_OK);
    ((unsigned char *)(uintptr_t)address)[0] = 0x5c;
    fail_munmap = munmap_calls + 1;
    CHECK(rt_direct_pool_unmap(pool, (void *)(uintptr_t)address, page) == RT_DIRECT_HOST_ERROR);
    CHECK(live_reservations() == 1 && ((unsigned char *)(uintptr_t)address)[0] == 0x5c);
    fail_munmap = munmap_calls + 1;
    CHECK(rt_direct_pool_destroy(&pool) == RT_DIRECT_HOST_ERROR);
    CHECK(pool != NULL && live_reservations() == 1 && open_files == 1);
    fail_munmap = 0;
    REQUIRE(rt_direct_pool_destroy(&pool) == RT_DIRECT_OK);
    CHECK(!pool && !open_files && !live_reservations());
    REQUIRE(rt_direct_pool_create(page, &pool) == RT_DIRECT_OK);
    fail_close = 1;
    CHECK(rt_direct_pool_destroy(&pool) == RT_DIRECT_HOST_ERROR);
    CHECK(!pool && !open_files); /* fclose failure is reported without a stale FILE pointer. */
    fail_close = 0;
    return failures ? 1 : 0;
}
