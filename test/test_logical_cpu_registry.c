/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Owned public registry behavior; no hardware query, pinning or guest-created thread. */
#include "runtime/process.h"
#include "runtime/syslib.h"
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)
typedef int (*attr_one)(void **);
typedef int (*attr_copy)(void *, void **);
typedef int (*attr_affinity)(void **, void *);
static void *(*get_self)(void);
static attr_one init_attr, destroy_attr;
static attr_copy copy_attr;
static attr_affinity get_affinity;

static void *lookup(const char *symbol)
{
    for (const rt_syslib_entry *entry = rt_syslib_begin(); entry && entry != rt_syslib_end(); entry++)
        if (!strcmp(entry->library, "libkernel") && !strcmp(entry->symbol, symbol)) return entry->fn;
    return NULL;
}

static rt_cpu_status forbidden_query(void *context, uint32_t **ids, size_t *count)
{
    unsigned *calls = context;
    (*calls)++;
    *ids = NULL;
    *count = 0;
    return RT_CPU_QUERY_FAILED;
}

static void success_case(uint64_t count)
{
    unsigned char stack[32];
    errno = EDOM;
    CHECK(rt_thread_register_main_logical(stack, sizeof stack, 0).status == RT_LOGICAL_CPU_COUNT_OUT_OF_RANGE);
    CHECK(rt_thread_register_main_logical(stack, sizeof stack, 65).status == RT_LOGICAL_CPU_COUNT_OUT_OF_RANGE);
    CHECK(rt_thread_register_main_logical(stack, sizeof stack, UINT64_MAX).status == RT_LOGICAL_CPU_COUNT_OUT_OF_RANGE);
    CHECK(errno == EDOM);
    CHECK(rt_thread_register_main_logical(stack, sizeof stack, count).status == RT_LOGICAL_CPU_OK);
    void *self = get_self();
    CHECK(self != NULL);
    void *snapshot = NULL, *fresh = NULL;
    CHECK(init_attr(&snapshot) == 0 && snapshot);
    CHECK(init_attr(&fresh) == 0 && fresh);
    if (!snapshot || !fresh) return;
    uint64_t mask = 0;
    CHECK(get_affinity(&snapshot, &mask) == 0 && mask == UINT64_MAX);
    CHECK(copy_attr(self, &snapshot) == 0);
    const uint64_t expected = count == 64 ? UINT64_MAX : (UINT64_C(1) << count) - 1;
    for (unsigned i = 0; i < 3; i++) {
        _Alignas(uint64_t) unsigned char bytes[10];
        CHECK((uintptr_t)(bytes + 1) % sizeof(uint64_t) != 0);
        memset(bytes, 0xa5, sizeof bytes);
        CHECK(get_affinity(&snapshot, bytes + 1) == 0);
        memcpy(&mask, bytes + 1, sizeof mask);
        CHECK(mask == expected && bytes[0] == 0xa5 && bytes[9] == 0xa5);
    }
    CHECK(get_affinity(&fresh, &mask) == 0 && mask == UINT64_MAX);
    /* A different value constructed by an embedder cannot change the registered snapshot. */
    rt_logical_cpu_slots unrelated;
    CHECK(rt_logical_cpu_slots_init(count == 1 ? 64 : 1, &unrelated).status == RT_LOGICAL_CPU_OK);
    CHECK(get_affinity(&snapshot, &mask) == 0 && mask == expected);
    unsigned calls = 0;
    CHECK(rt_thread_register_main_cpu(stack, sizeof stack, forbidden_query, &calls, NULL) == RT_CPU_BAD_CONFIGURATION);
    CHECK(calls == 0);
    CHECK(rt_thread_register_main_logical(stack, sizeof stack, 1).status == RT_LOGICAL_CPU_INVALID_STATE);
    CHECK(get_self() == self && get_affinity(&snapshot, &mask) == 0 && mask == expected);
    /* Eight-byte publication may alias the pointer slot; restore its owned pointer before destroy. */
    void *saved = snapshot;
    CHECK(get_affinity(&snapshot, &snapshot) == 0);
    memcpy(&mask, &snapshot, sizeof mask);
    snapshot = saved;
    CHECK(mask == expected);
    _Alignas(uint64_t) unsigned char output[10];
    unsigned char poison[10];
    CHECK((uintptr_t)(output + 1) % sizeof(uint64_t) != 0);
    memset(poison, 0x7b, sizeof poison); memcpy(output, poison, sizeof output);
    void *empty = NULL;
    CHECK(get_affinity(NULL, output + 1) == (int)0x80020016);
    CHECK(get_affinity(&empty, output + 1) == (int)0x80020016);
    CHECK(memcmp(output, poison, sizeof output) == 0);
    CHECK(copy_attr(NULL, &snapshot) == (int)0x80020016);
    CHECK(copy_attr(self, &empty) == (int)0x80020016);
    CHECK(get_affinity(&snapshot, &mask) == 0 && mask == expected);
    const pid_t child = fork();
    if (child == 0) { alarm(3); get_affinity(&snapshot, NULL); _exit(1); }
    int status = 0;
    CHECK(child > 0 && waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status) && (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS));
    CHECK(lookup("scePthreadAttrSetaffinity") == NULL);
    CHECK(lookup("scePthreadCreate") == NULL);
    CHECK(destroy_attr(&fresh) == 0 && fresh == NULL);
    CHECK(destroy_attr(&snapshot) == 0 && snapshot == NULL);
}

int main(void)
{
    get_self = (void *(*)(void))lookup("scePthreadSelf");
    init_attr = (attr_one)lookup("scePthreadAttrInit");
    destroy_attr = (attr_one)lookup("scePthreadAttrDestroy");
    copy_attr = (attr_copy)lookup("scePthreadAttrGet");
    get_affinity = (attr_affinity)lookup("scePthreadAttrGetaffinity");
    CHECK(get_self && init_attr && destroy_attr && copy_attr && get_affinity);
    if (failures) return 1;
    const uint64_t counts[] = {1, 3, 63, 64};
    for (size_t i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        const pid_t child = fork();
        if (child == 0) { alarm(6); success_case(counts[i]); _exit(failures ? 1 : 0); }
        int status = 0;
        CHECK(child > 0 && waitpid(child, &status, 0) == child);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    if (!failures) puts("logical slots: public registry snapshots and fresh sentinel ok");
    return failures != 0;
}
