/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Isolated owned failure test. No page-zero mapping, raw GS read, thread key or guest execution. */
#include "runtime/boot_darwin.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)
static unsigned maps, unmaps, allocations, frees, creates, clears, destroys, readers;
static size_t owned_size;
struct rt_guest_tls_key { int live; }; /* Test-only fake API handle, no platform-layout claim. */
static struct rt_guest_tls_key fake_key;

static void *zero_map(void *hint, size_t size, int protection, int flags, int descriptor, off_t offset)
{
    CHECK(hint == NULL && size >= 10 && protection == (PROT_READ | PROT_WRITE));
    CHECK(flags == (MAP_PRIVATE | MAP_ANON) && descriptor == -1 && offset == 0);
    maps++; owned_size = size;
    return NULL; /* A synthetic successful mmap result: no real mapping is made. */
}
static int failed_then_successful_unmap(void *address, size_t size)
{
    CHECK(address == NULL && size == owned_size && size != 0);
    unmaps++;
    if (unmaps == 1) { errno = EIO; return -1; }
    return 0;
}
static int counted_allocation(void **out, size_t align, size_t size)
{
    allocations++;
    return posix_memalign(out, align, size);
}
static void counted_free(void *pointer)
{
    if (pointer) frees++;
    free(pointer);
}
/* Compile the exact production helper with aliases only inside this test translation unit. */
#define mmap zero_map
#define munmap failed_then_successful_unmap
#define posix_memalign counted_allocation
#define free counted_free
#include "../runtime/boot_darwin.c"
#undef free
#undef posix_memalign
#undef munmap
#undef mmap

rt_guest_tls_result rt_guest_tls_create(rt_guest_tls_key **out)
{
    CHECK(out && !*out && !fake_key.live); creates++; fake_key.live = 1; *out = &fake_key;
    return (rt_guest_tls_result){RT_GUEST_TLS_OK, 0};
}
rt_guest_tls_result rt_guest_tls_get_candidate(const rt_guest_tls_key *key, rt_guest_tls_candidate *out)
{
    CHECK(key == &fake_key && fake_key.live && out);
    *out = (rt_guest_tls_candidate){256, 2048};
    return (rt_guest_tls_result){RT_GUEST_TLS_OK, 0};
}
rt_guest_tls_result rt_guest_tls_clear(rt_guest_tls_key *key)
{
    CHECK(key == &fake_key && fake_key.live); clears++;
    return (rt_guest_tls_result){RT_GUEST_TLS_OK, 0};
}
rt_guest_tls_result rt_guest_tls_destroy(rt_guest_tls_key **key)
{
    CHECK(key && *key == &fake_key && fake_key.live); destroys++; fake_key.live = 0; *key = NULL;
    return (rt_guest_tls_result){RT_GUEST_TLS_OK, 0};
}
rt_guest_tls_result rt_guest_tls_bind(rt_guest_tls_key *key, void *tcb)
{
    (void)key; (void)tcb; CHECK(0); /* Zero-address rejection must precede all reader bindings. */
    return (rt_guest_tls_result){RT_GUEST_TLS_HOST_ERROR, EIO};
}
rt_guest_tls_result rt_guest_tls_read(const rt_guest_tls_key *key, void **out)
{
    (void)key; (void)out; CHECK(0);
    return (rt_guest_tls_result){RT_GUEST_TLS_HOST_ERROR, EIO};
}
static int never_read(void *user, const void *code, uint64_t *value)
{
    (void)user; (void)code; (void)value; readers++; CHECK(0); return -1;
}
int main(void)
{
    rt_boot_darwin_tls state = {0};
    const rt_elf_tls tls = {0};
    const unsigned char image[1] = {0};
    char error[256];
    CHECK(rt_boot_darwin_tls_prepare(&state, &tls, image, sizeof image,
                                     never_read, NULL, error, sizeof error) == -1);
    CHECK(strstr(error, "unsupported zero code address") && strstr(error, "cleanup incomplete"));
    CHECK(maps == 1 && unmaps == 1 && allocations == 1 && frees == 1);
    CHECK(creates == 1 && clears == 1 && destroys == 1 && !fake_key.live && !readers);
    CHECK(state.read_code == NULL && state.read_code_size == owned_size && owned_size > 0);
    CHECK(!state.key && !state.allocation && !state.tcb && !state.qualified);
    CHECK(rt_boot_darwin_tls_prepare(&state, &tls, image, sizeof image,
                                     never_read, NULL, error, sizeof error) == -1);
    CHECK(strstr(error, "already owns resources"));
    CHECK(maps == 1 && unmaps == 1 && allocations == 1 && frees == 1);
    CHECK(creates == 1 && clears == 1 && destroys == 1 && !readers);
    CHECK(state.read_code == NULL && state.read_code_size == owned_size);
    CHECK(rt_boot_darwin_tls_cleanup(&state) == 0);
    CHECK(unmaps == 2 && state.read_code == NULL && state.read_code_size == 0);
    CHECK(!state.key && !state.allocation && !state.tcb && !state.qualified && !state.offset);
    CHECK(rt_boot_darwin_tls_cleanup(&state) == 0);
    CHECK(unmaps == 2 && maps == 1 && allocations == 1 && frees == 1);
    printf("owned zero-map ownership regression failures=%d\n", failures);
    return failures != 0;
}
