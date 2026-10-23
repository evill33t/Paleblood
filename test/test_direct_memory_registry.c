/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise the real RT_SYSLIB entries, with typed guest argument wires and Release-active checks. */
#define _GNU_SOURCE
#include "runtime/direct_memory.h"
#include "runtime/syslib.h"

#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

typedef uint64_t (*size_fn)(void);
typedef int32_t (*allocate_fn)(uint64_t, uint64_t, uint64_t, uint64_t, int32_t, void *);
typedef int32_t (*map_fn)(void *, uint64_t, int32_t, int32_t, uint64_t, uint64_t);
typedef int32_t (*release_fn)(uint64_t, uint64_t);
static size_fn size_entry;
static allocate_fn allocate_entry;
static map_fn map_entry;
static release_fn release_entry;
static unsigned char *shared_slot;

static void *entry(const char *name)
{
    for (const rt_syslib_entry *e = rt_syslib_begin(); e && e != rt_syslib_end(); e++)
        if (!strcmp(e->library, "libkernel") && !strcmp(e->symbol, name)) {
            CHECK(e->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
            return e->fn;
        }
    return NULL;
}

static void fatal_call(int which)
{
    const uint64_t page = RT_DIRECT_PAGE_SIZE;
    if (which == 0) { (void)size_entry(); return; }
    if (which == 1) { (void)allocate_entry(0, 8 * page, page, 0, 2, shared_slot + 1); return; }
    if (which == 2) { (void)map_entry(shared_slot + 1, page, 7, 0, 0, 0); return; }
    if (which == 3) { (void)map_entry(shared_slot + 1, page, 3, 0x10, 0, 0); return; }
    if (which == 4) { (void)release_entry(0, page); return; }
    if (which == 5) {
        uint64_t address = 0;
        if (map_entry(&address, 2 * page, 3, 0, 0, 0)) _exit(10);
        (void)release_entry(0, 2 * page);
    }
}

static void expect_fatal(int which)
{
    memset(shared_slot, 0x8d, 16);
    const pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) { alarm(2); fatal_call(which); _exit(11); }
    if (child < 0) return;
    int result = 0;
    CHECK(waitpid(child, &result, 0) == child);
    CHECK(WIFSIGNALED(result) && WTERMSIG(result) == SIGABRT);
    for (size_t i = 0; i < 16; i++) CHECK(shared_slot[i] == 0x8d);
}

static void expect_slot_fault(int which)
{
    const pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        alarm(2);
        if (which == 0)
            (void)allocate_entry(0, 8 * RT_DIRECT_PAGE_SIZE, RT_DIRECT_PAGE_SIZE, 0, 0, NULL);
        else
            (void)map_entry(NULL, RT_DIRECT_PAGE_SIZE, which == 1 ? 3 : 7, 0, 0, 0);
        _exit(12);
    }
    if (child < 0) return;
    int result = 0;
    CHECK(waitpid(child, &result, 0) == child);
    CHECK(WIFSIGNALED(result) && (WTERMSIG(result) == SIGSEGV || WTERMSIG(result) == SIGBUS));
}

typedef struct { uint64_t offset; int32_t result; } worker_result;
static void *worker_allocate(void *arg)
{
    worker_result *worker = arg;
    worker->result = allocate_entry(0, 8 * RT_DIRECT_PAGE_SIZE, RT_DIRECT_PAGE_SIZE, 0, 0, &worker->offset);
    return NULL;
}

int main(void)
{
    size_entry = (size_fn)entry("sceKernelGetDirectMemorySize");
    allocate_entry = (allocate_fn)entry("sceKernelAllocateDirectMemory");
    map_entry = (map_fn)entry("sceKernelMapDirectMemory");
    release_entry = (release_fn)entry("sceKernelReleaseDirectMemory");
    REQUIRE(size_entry && allocate_entry && map_entry && release_entry);
    CHECK(!entry("sceKernelGetDirectMemoryType"));
    shared_slot = mmap(NULL, 16384, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    REQUIRE(shared_slot != MAP_FAILED);
    expect_fatal(0);
    const uint64_t page = RT_DIRECT_PAGE_SIZE;
    REQUIRE(rt_direct_runtime_configure(UINT64_C(0x100004000)) == RT_DIRECT_OK);
    CHECK(size_entry() == UINT64_C(0x100004000)); /* The registry return retains its upper word. */
    REQUIRE(rt_direct_runtime_shutdown() == RT_DIRECT_OK);
    REQUIRE(rt_direct_runtime_configure(8 * page) == RT_DIRECT_OK);
    CHECK(rt_direct_runtime_configure(8 * page) == RT_DIRECT_ALREADY_CONFIGURED);
    CHECK(size_entry() == 8 * page);
    uint64_t offset;
    memset(shared_slot, 0x8d, 16);
    REQUIRE(allocate_entry(0, 8 * page, 2 * page, 0, 0, shared_slot + 1) == 0);
    memcpy(&offset, shared_slot + 1, 8);
    CHECK(offset == 0 && shared_slot[0] == 0x8d && shared_slot[9] == 0x8d && shared_slot[15] == 0x8d);
    expect_fatal(1);
    expect_fatal(2);
    expect_fatal(3);
    expect_fatal(4); /* Partial allocation release. */
    expect_fatal(5); /* An exact release with a live map is a separate unsupported policy. */
    expect_slot_fault(0); /* Valid allocation commits before the borrowed NULL output faults. */
    expect_slot_fault(1); /* Valid mapping arguments do not invent a NULL-slot status. */
    expect_slot_fault(2); /* The Map input read precedes even unsupported protection checks. */
    memset(shared_slot, 0x8d, 16);
    CHECK(allocate_entry(0, 8 * page, page + 1, 0, 0, shared_slot + 1) == (int32_t)UINT32_C(0x80020016));
    CHECK(allocate_entry(0, 2 * page, page, 0, 0, shared_slot + 1) == (int32_t)UINT32_C(0x8002000c));
    CHECK(allocate_entry(0, 2 * page, page, 0, 0, NULL) == (int32_t)UINT32_C(0x8002000c));
    /* A failed allocation never touches its output, including a borrowed NULL slot. */
    for (size_t i = 0; i < 16; i++) CHECK(shared_slot[i] == 0x8d);
    uint64_t address = 0;
    memcpy(shared_slot + 1, &address, 8);
    REQUIRE(map_entry(shared_slot + 1, 2 * page, 3, 0, offset, 0) == 0);
    memcpy(&address, shared_slot + 1, 8);
    CHECK(shared_slot[0] == 0x8d && shared_slot[9] == 0x8d);
    unsigned char *first = (unsigned char *)(uintptr_t)address;
    first[0] = 0x37;
    uint64_t alias = address;
    REQUIRE(map_entry(&alias, 2 * page, 3, 0, offset, 0) == 0);
    REQUIRE(alias && alias != address);
    CHECK(((unsigned char *)(uintptr_t)alias)[0] == 0x37);
    ((unsigned char *)(uintptr_t)alias)[0] = 0x58;
    CHECK(first[0] == 0x58);
    uint64_t unchanged = UINT64_C(0x12340000), before = unchanged;
    CHECK(map_entry(&unchanged, page, 3, 0, 4 * page, 0) == (int32_t)UINT32_C(0x80020016));
    CHECK(unchanged == before);
    REQUIRE(rt_direct_runtime_shutdown() == RT_DIRECT_OK); /* Owns both aliases and backing. */
    expect_fatal(0);
    REQUIRE(rt_direct_runtime_configure(8 * page) == RT_DIRECT_OK);
    REQUIRE(allocate_entry(0, 8 * page, page, 0, 0, &offset) == 0);
    CHECK(offset == 0);
    REQUIRE(release_entry(offset, page) == 0);
    REQUIRE(allocate_entry(0, 8 * page, page, 0, 0, &offset) == 0);
    CHECK(offset == 0);
    worker_result workers[2] = {{0, -1}, {0, -1}};
    pthread_t threads[2];
    REQUIRE(pthread_create(&threads[0], NULL, worker_allocate, &workers[0]) == 0);
    REQUIRE(pthread_create(&threads[1], NULL, worker_allocate, &workers[1]) == 0);
    REQUIRE(pthread_join(threads[0], NULL) == 0);
    REQUIRE(pthread_join(threads[1], NULL) == 0);
    CHECK(workers[0].result == 0 && workers[1].result == 0);
    CHECK(workers[0].offset != workers[1].offset && workers[0].offset != offset && workers[1].offset != offset);
    REQUIRE(rt_direct_runtime_shutdown() == RT_DIRECT_OK);
    CHECK(rt_direct_runtime_shutdown() == RT_DIRECT_OK);
    CHECK(munmap(shared_slot, 16384) == 0);
    return failures ? 1 : 0;
}
