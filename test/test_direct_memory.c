/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Owned pool-policy tests. CHECK/REQUIRE remain active with NDEBUG/Release. */
#include "runtime/direct_memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

static int parser(void)
{
    const char *bad[] = {"", "0", "1", "16383", "16385", "-16384", "+16384", " 16384", "16384 ",
                         "0x4000", "1e5", "18446744073709551616", "18446744073709551615",
                         "9223372036854775808", "9999999999999999999999999999999999999999"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        uint64_t out = UINT64_C(0xdecafbad12345678);
        CHECK(rt_direct_parse_capacity(bad[i], &out) == RT_DIRECT_INVALID);
        CHECK(out == UINT64_C(0xdecafbad12345678));
    }
    uint64_t out = 0;
    CHECK(rt_direct_parse_capacity(NULL, &out) == RT_DIRECT_INVALID);
    CHECK(rt_direct_parse_capacity("16384", NULL) == RT_DIRECT_INVALID);
    REQUIRE(rt_direct_parse_capacity("00016384", &out) == RT_DIRECT_OK);
    CHECK(out == RT_DIRECT_PAGE_SIZE);
    REQUIRE(rt_direct_parse_capacity("4294983680", &out) == RT_DIRECT_OK);
    CHECK(out == UINT64_C(4294983680));
    return 0;
}

int main(void)
{
    REQUIRE(!parser());
    const uint64_t page = RT_DIRECT_PAGE_SIZE, capacity = 8 * page;
    rt_direct_pool *pool = NULL;
    CHECK(rt_direct_pool_create(0, &pool) == RT_DIRECT_INVALID && !pool);
    CHECK(rt_direct_pool_create(page + 1, &pool) == RT_DIRECT_INVALID && !pool);
    CHECK(rt_direct_pool_create(page, NULL) == RT_DIRECT_INVALID);
    REQUIRE(rt_direct_pool_create(capacity, &pool) == RT_DIRECT_OK);
    CHECK(rt_direct_pool_capacity(pool) == capacity);
    rt_direct_pool *existing = pool;
    CHECK(rt_direct_pool_create(capacity, &pool) == RT_DIRECT_INVALID && pool == existing);
    unsigned char slot[18];
    memset(slot, 0xa5, sizeof slot);
    uint64_t first, lower, hole, offset;
    REQUIRE(rt_direct_pool_allocate(pool, 3 * page, capacity, page, 4 * page, 0, slot + 1) == RT_DIRECT_OK);
    memcpy(&first, slot + 1, 8);
    CHECK(first == 4 * page && slot[0] == 0xa5 && slot[9] == 0xa5 && slot[17] == 0xa5);
    REQUIRE(rt_direct_pool_allocate(pool, 0, capacity, 2 * page, 0, 0, &lower) == RT_DIRECT_OK);
    CHECK(lower == 0);
    REQUIRE(rt_direct_pool_allocate(pool, 0, capacity, page, page, 0, &hole) == RT_DIRECT_OK);
    CHECK(hole == 2 * page);
    REQUIRE(rt_direct_pool_allocate(pool, 0, capacity, page, page, 0, &offset) == RT_DIRECT_OK);
    CHECK(offset == 3 * page);
    memset(slot, 0xa5, sizeof slot);
    CHECK(rt_direct_pool_allocate(pool, 0, 5 * page, page, page, 0, slot + 1) == RT_DIRECT_NO_MEMORY);
    for (size_t i = 0; i < sizeof slot; i++) CHECK(slot[i] == 0xa5);
    CHECK(rt_direct_pool_allocate(pool, 0, capacity + page, page, page, 0, slot + 1) == RT_DIRECT_INVALID);
    CHECK(rt_direct_pool_allocate(pool, 0, capacity, 0, page, 0, slot + 1) == RT_DIRECT_INVALID);
    CHECK(rt_direct_pool_allocate(pool, page + 1, capacity, page, page, 0, slot + 1) == RT_DIRECT_INVALID);
    CHECK(rt_direct_pool_allocate(pool, 0, capacity, page, 3 * page, 0, slot + 1) == RT_DIRECT_INVALID);
    CHECK(rt_direct_pool_allocate(pool, 0, capacity, page, 8, 0, slot + 1) == RT_DIRECT_INVALID);
    CHECK(rt_direct_pool_allocate(pool, 0, capacity, page, 0, 1, slot + 1) == RT_DIRECT_UNSUPPORTED_TYPE);
    CHECK(rt_direct_pool_allocate(pool, UINT64_MAX - page + 1, UINT64_MAX, 2 * page, page, 0, slot + 1)
          == RT_DIRECT_INVALID);
    CHECK(rt_direct_pool_release(pool, lower + page, page) == RT_DIRECT_UNSUPPORTED_RELEASE);
    CHECK(rt_direct_pool_release(pool, first, 2 * page) == RT_DIRECT_UNSUPPORTED_RELEASE);
    CHECK(rt_direct_pool_release(pool, 7 * page, page) == RT_DIRECT_INVALID);
    REQUIRE(rt_direct_pool_release(pool, hole, page) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_allocate(pool, 0, capacity, page, 0, 0, &offset) == RT_DIRECT_OK);
    CHECK(offset == hole);

    uint64_t address = 0;
    memcpy(slot + 1, &address, 8);
    REQUIRE(rt_direct_pool_map(pool, slot + 1, 2 * page, 3, 0, lower, 4 * page) == RT_DIRECT_OK);
    memcpy(&address, slot + 1, 8);
    CHECK(address % (4 * page) == 0 && slot[0] == 0xa5 && slot[9] == 0xa5);
    unsigned char *mapped = (unsigned char *)(uintptr_t)address;
    mapped[0] = 0x31;
    mapped[page - 1] = 0x32;
    mapped[page] = 0x41;
    mapped[2 * page - 1] = 0x42;
    uint64_t alias = address; /* Collision is only a hint; its existing bytes must survive. */
    REQUIRE(rt_direct_pool_map(pool, &alias, page, 3, 0, lower + page, 0) == RT_DIRECT_OK);
    REQUIRE(alias && alias != address);
    unsigned char *other = (unsigned char *)(uintptr_t)alias;
    CHECK(other[0] == 0x41 && other[page - 1] == 0x42);
    other[0] = 0x61;
    CHECK(mapped[0] == 0x31 && mapped[page - 1] == 0x32 && mapped[page] == 0x61);
    mapped[2 * page - 1] = 0x71;
    CHECK(other[page - 1] == 0x71);
    uint64_t invalid = UINT64_C(0x12340000), original = invalid;
    CHECK(rt_direct_pool_map(pool, &invalid, page, 3, 0, 6 * page, 0) == RT_DIRECT_INVALID);
    CHECK(invalid == original);
    CHECK(rt_direct_pool_map(pool, &invalid, page, 7, 0, lower, 0) == RT_DIRECT_UNSUPPORTED_MAPPING);
    CHECK(rt_direct_pool_map(pool, &invalid, page, 3, 0x10, lower, 0) == RT_DIRECT_UNSUPPORTED_MAPPING);
    CHECK(rt_direct_pool_map(pool, &invalid, page, 3, 0, lower, UINT64_C(2147483648)) == RT_DIRECT_INVALID);
    CHECK(rt_direct_pool_map(pool, &invalid, UINT64_MAX, 3, 0, lower, page) == RT_DIRECT_INVALID);
    CHECK(invalid == original);
    CHECK(rt_direct_pool_release(pool, lower, 2 * page) == RT_DIRECT_MAPPED_ALLOCATION);
    CHECK(rt_direct_pool_unmap(pool, mapped, page) == RT_DIRECT_UNSUPPORTED_RELEASE);
    REQUIRE(rt_direct_pool_unmap(pool, mapped, 2 * page) == RT_DIRECT_OK);
    CHECK(other[0] == 0x61 && other[page - 1] == 0x71);
    CHECK(rt_direct_pool_release(pool, lower, 2 * page) == RT_DIRECT_MAPPED_ALLOCATION);
    REQUIRE(rt_direct_pool_unmap(pool, other, page) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_release(pool, lower, 2 * page) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_allocate(pool, 0, capacity, 2 * page, 0, 0, &offset) == RT_DIRECT_OK);
    CHECK(offset == lower);
    address = 0;
    REQUIRE(rt_direct_pool_map(pool, &address, 2 * page, 3, 0, offset, 0) == RT_DIRECT_OK);
    mapped = (unsigned char *)(uintptr_t)address;
    CHECK(mapped[0] == 0x31 && mapped[page] == 0x61); /* Explicit retained-backing reuse policy. */
    uint64_t output_alias = 0;
    memcpy(mapped + 1, &output_alias, 8);
    REQUIRE(rt_direct_pool_map(pool, mapped + 1, page, 3, 0, offset, 0) == RT_DIRECT_OK);
    memcpy(&output_alias, mapped + 1, 8);
    uint64_t observed_alias_slot = 0;
    memcpy(&observed_alias_slot, (unsigned char *)(uintptr_t)output_alias + 1, 8);
    CHECK(output_alias == observed_alias_slot && mapped[0] == 0x31);
    REQUIRE(rt_direct_pool_destroy(&pool) == RT_DIRECT_OK);
    CHECK(pool == NULL);
    CHECK(rt_direct_pool_destroy(&pool) == RT_DIRECT_OK);

    /* Full-width physical offsets, sparse file capacity and the observed 116 MiB shape.
       Only boundary bytes are touched; this does not claim resident capacity. */
    const uint64_t large_capacity = UINT64_C(0x100004000), high = UINT64_C(0x100000000);
    REQUIRE(rt_direct_pool_create(large_capacity, &pool) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_allocate(pool, high, large_capacity, page, page, 0, &offset) == RT_DIRECT_OK);
    CHECK(offset == high && rt_direct_pool_capacity(pool) == large_capacity);
    address = 0;
    REQUIRE(rt_direct_pool_map(pool, &address, page, 3, 0, offset, 0) == RT_DIRECT_OK);
    mapped = (unsigned char *)(uintptr_t)address;
    mapped[0] = 0xa1;
    mapped[page - 1] = 0xb2;
    REQUIRE(rt_direct_pool_destroy(&pool) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_create(UINT64_C(0x8000000), &pool) == RT_DIRECT_OK);
    REQUIRE(rt_direct_pool_allocate(pool, 0, rt_direct_pool_capacity(pool), UINT64_C(0x7400000),
                                    UINT64_C(0x200000), 0, &offset) == RT_DIRECT_OK);
    CHECK(offset == 0);
    address = 0;
    REQUIRE(rt_direct_pool_map(pool, &address, UINT64_C(0x7400000), 3, 0, offset,
                               UINT64_C(0x200000)) == RT_DIRECT_OK);
    mapped = (unsigned char *)(uintptr_t)address;
    CHECK(address % UINT64_C(0x200000) == 0);
    mapped[0] = 0xc3;
    mapped[UINT64_C(0x7400000) - 1] = 0xd4;
    CHECK(mapped[0] == 0xc3 && mapped[UINT64_C(0x7400000) - 1] == 0xd4);
    REQUIRE(rt_direct_pool_destroy(&pool) == RT_DIRECT_OK);
    return failures ? 1 : 0;
}
