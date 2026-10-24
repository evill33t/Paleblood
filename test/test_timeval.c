/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "runtime/syslib.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while (0)
typedef int (*time_query)(void *, const void *);

static time_query find(const char *library)
{
    for (const rt_syslib_entry *p = rt_syslib_begin(); p != rt_syslib_end(); ++p) {
        if (strcmp(p->library, library) == 0 && strcmp(p->symbol, "gettimeofday") == 0) {
            CHECK(p->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
            return (time_query)p->fn;
        }
    }
    CHECK(0);
    return NULL;
}

int main(void)
{
    time_query posix = find("libScePosix"), kernel = find("libkernel");
    CHECK(posix == kernel);
    for (int i = 0; i != 2; ++i) {
        _Alignas(uint64_t) unsigned char bytes[18];
        CHECK((uintptr_t)(bytes + 1) % sizeof(uint64_t) != 0);
    memset(bytes, 0xa5, sizeof bytes);
        const int result = (i ? kernel : posix)(bytes + 1, NULL);
        CHECK(result == 0);
        int64_t fields[2];
        memcpy(fields, bytes + 1, sizeof fields);
        CHECK(bytes[0] == 0xa5 && bytes[17] == 0xa5);
        CHECK(fields[1] >= 0 && fields[1] < INT64_C(1000000));
        /* No wall-clock ordering assertion: host realtime can be adjusted. Exact values,
           full-width seconds and layout conversion are checked by the injected provider. */
    }
    return 0;
}
