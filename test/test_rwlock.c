/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Call the two lifecycle operations through the real system-library registry. */
#include "runtime/syslib.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

typedef int (*init_fn)(void *, const void *, const char *);
typedef int (*destroy_fn)(void *);

static void *entry(const char *symbol)
{
    for (const rt_syslib_entry *e = rt_syslib_begin(); e && e != rt_syslib_end(); e++) {
        if (!strcmp(e->library, "libkernel") && !strcmp(e->symbol, symbol)) {
            CHECK(e->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
            return e->fn;
        }
    }
    CHECK(0);
    return NULL;
}

static uint64_t read_slot(const unsigned char *p)
{
    uint64_t value;
    memcpy(&value, p, sizeof value);
    return value;
}

static void guards(const unsigned char *p, size_t n, size_t offset)
{
    for (size_t i = 0; i < n; i++)
        if (i < offset || i >= offset + 8) CHECK(p[i] == 0xa5);
}

int main(void)
{
    const init_fn init = (init_fn)entry("scePthreadRwlockInit");
    const destroy_fn destroy = (destroy_fn)entry("scePthreadRwlockDestroy");
    if (failures) return 1;
    for (unsigned round = 0; round < 3; round++) {
        _Alignas(uint64_t) unsigned char a[23], b[27];
        memset(a, 0xa5, sizeof a);
        memset(b, 0xa5, sizeof b);
        /* Both slots are deliberately unaligned and initially nonzero poison. */
        CHECK((uintptr_t)(a + 3) % 8 != 0 && (uintptr_t)(b + 5) % 8 != 0);
        CHECK(init(a + 3, NULL, NULL) == 0);
        CHECK(init(b + 5, NULL, NULL) == 0);
        const uint64_t first = read_slot(a + 3), second = read_slot(b + 5);
        CHECK(first != 0 && first != 1 && second != 0 && second != 1 && first != second);
        guards(a, sizeof a, 3);
        guards(b, sizeof b, 5);
        CHECK(destroy(a + 3) == 0 && read_slot(a + 3) == 1);
        CHECK(read_slot(b + 5) == second);
        CHECK(destroy(b + 5) == 0 && read_slot(b + 5) == 1);
        guards(a, sizeof a, 3);
        guards(b, sizeof b, 5);
    }
    _Alignas(uint64_t) unsigned char empty[19], before[19];
    memset(empty, 0xa5, sizeof empty);
    memset(empty + 7, 0, 8);
    memcpy(before, empty, sizeof before);
    CHECK((uintptr_t)(empty + 7) % 8 != 0);
    CHECK(destroy(empty + 7) == 0 && !memcmp(empty, before, sizeof empty));
    if (!failures) puts("rwlock lifecycle: ok");
    return failures != 0;
}
