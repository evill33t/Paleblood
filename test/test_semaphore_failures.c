/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Test-only aliases; no production fault hooks or reduced ID limit. */
#include "runtime/process.h"
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *test_calloc(size_t, size_t);
static void test_free(void *);
static _Noreturn void test_fatal(const char *, const char *);
#define calloc test_calloc
#define free test_free
#define rt_fatal test_fatal
#include "../runtime/syslib/kernel_semaphore.c"
#undef calloc
#undef free
#undef rt_fatal

static int failures, allocations, frees, fail_allocate, alarm_on_allocate, alarm_on_free;
static void *owned;
static jmp_buf fatal_context;
static sigjmp_buf timeout_context;
static volatile sig_atomic_t alarm_seen;
static const char *fatal_name, *fatal_message;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define FATAL(call, name) do { fatal_name = NULL; fatal_message = NULL; if (!setjmp(fatal_context)) { (void)(call); CHECK(0); } CHECK(fatal_name && !strcmp(fatal_name, name)); CHECK(fatal_message && *fatal_message); } while (0)

static uint64_t value(const void *p)
{
    uint64_t n;
    memcpy(&n, p, sizeof n);
    return n;
}

static int timeout_blocked(void)
{
    sigset_t mask;
    CHECK(pthread_sigmask(SIG_BLOCK, NULL, &mask) == 0);
    return sigismember(&mask, SIGALRM) == 1;
}

static void table_available(void)
{
    const int result = pthread_mutex_trylock(&semaphore_table_lock);
    CHECK(result == 0);
    if (!result) CHECK(pthread_mutex_unlock(&semaphore_table_lock) == 0);
}

static void *test_calloc(size_t count, size_t size)
{
    allocations++;
    CHECK(count == 1 && size == sizeof(semaphore_record) && !owned);
    CHECK(timeout_blocked());
    const int result = pthread_mutex_trylock(&semaphore_table_lock);
    CHECK(result != 0); /* allocation and table registration are serialized */
    if (!result) CHECK(pthread_mutex_unlock(&semaphore_table_lock) == 0);
    if (alarm_on_allocate) {
        alarm_on_allocate = 0;
        CHECK(raise(SIGALRM) == 0);
        CHECK(!alarm_seen); /* deferred until publication and mutex release */
    }
    if (fail_allocate) return NULL;
    owned = calloc(count, size);
    CHECK(owned != NULL);
    return owned;
}

static void test_free(void *p)
{
    CHECK(p == owned && owned != NULL && !semaphores);
    CHECK(timeout_blocked());
    table_available(); /* removed-record free is outside the mutex */
    frees++;
    free(p);
    owned = NULL;
    if (alarm_on_free) {
        alarm_on_free = 0;
        CHECK(raise(SIGALRM) == 0);
        CHECK(!alarm_seen); /* cleanup completes before restoring the signal mask */
    }
}

static _Noreturn void test_fatal(const char *name, const char *message)
{
    fatal_name = name;
    fatal_message = message;
    longjmp(fatal_context, 1);
}

static void timeout_handler(int signum)
{
    (void)signum;
    alarm_seen = 1;
    siglongjmp(timeout_context, 1);
}

int main(void)
{
    _Alignas(uint64_t) static unsigned char bytes[23], saved[23];
    memset(bytes, 0xa5, sizeof bytes);
    memcpy(saved, bytes, sizeof saved);
    CHECK((uintptr_t)(bytes + 3) % 8 != 0);
    sigset_t original_mask, enabled_mask;
    CHECK(pthread_sigmask(SIG_BLOCK, NULL, &original_mask) == 0);
    enabled_mask = original_mask;
    CHECK(sigdelset(&enabled_mask, SIGALRM) == 0);
    CHECK(pthread_sigmask(SIG_SETMASK, &enabled_mask, NULL) == 0);
    CHECK(!timeout_blocked());

    CHECK(sema_create((void *)(uintptr_t)1, (const char *)(uintptr_t)1,
                      UINT32_MAX, INT32_MIN, INT32_MIN, (const void *)(uintptr_t)1)
          == (int32_t)UINT32_C(0x80020016));
    CHECK(!allocations && !semaphores && next_semaphore_id == 1);
    FATAL(sema_create(bytes + 3, "", 1, 0, 1, NULL), "sceKernelCreateSema");
    FATAL(sema_create(bytes + 3, "", 2, 1, 1, NULL), "sceKernelCreateSema");
    FATAL(sema_create(bytes + 3, "", 2, 0, 2, NULL), "sceKernelCreateSema");
    FATAL(sema_create(bytes + 3, "named", 2, 0, 1, NULL), "sceKernelCreateSema");
    FATAL(sema_create(bytes + 3, NULL, 2, 0, 1, NULL), "sceKernelCreateSema");
    CHECK(!allocations && !semaphores && next_semaphore_id == 1 && !memcmp(bytes, saved, sizeof bytes));

    fail_allocate = 1;
    FATAL(sema_create(bytes + 3, "", 2, 0, 1, NULL), "sceKernelCreateSema");
    FATAL(sema_create(NULL, "", 2, 0, 1, NULL), "sceKernelCreateSema");
    CHECK(allocations == 2 && !owned && !frees && !semaphores && next_semaphore_id == 1);
    CHECK(!memcmp(bytes, saved, sizeof bytes) && !timeout_blocked());
    table_available();
    fail_allocate = 0;
    CHECK(sema_create(bytes + 3, "", 2, 0, 1, NULL) == 0);
    const uint64_t id = value(bytes + 3);
    CHECK(id == 1 && id != (uint64_t)(uintptr_t)owned && next_semaphore_id == 2);
    memcpy(saved, bytes, sizeof saved);
    FATAL(sema_delete(id | (UINT64_C(1) << 32)), "sceKernelDeleteSema");
    FATAL(sema_delete(UINT64_MAX), "sceKernelDeleteSema");
    FATAL(sema_delete(0), "sceKernelDeleteSema");
    CHECK(owned && semaphores == owned && !frees && !memcmp(bytes, saved, sizeof bytes));
    CHECK(!timeout_blocked());
    table_available();
    CHECK(sema_delete(id) == 0 && !owned && !semaphores && frees == 1);
    FATAL(sema_delete(id), "sceKernelDeleteSema");
    CHECK(!memcmp(bytes, saved, sizeof bytes) && !owned && frees == 1);

    /* Actual pending SIGALRM, using the same nonlocal-exit shape as the boot timeout.
       The test never exits an operation while its table mutex is held. */
    struct sigaction action = {0}, previous_action;
    action.sa_handler = timeout_handler;
    CHECK(sigemptyset(&action.sa_mask) == 0);
    CHECK(sigaction(SIGALRM, &action, &previous_action) == 0);
    alarm_seen = 0;
    alarm_on_allocate = 1;
    if (!sigsetjmp(timeout_context, 1)) {
        (void)sema_create(bytes + 3, "", 2, 0, 1, NULL);
        CHECK(0);
    }
    CHECK(alarm_seen && owned && semaphores == owned && value(bytes + 3) == 2);
    CHECK(!timeout_blocked());
    table_available();
    CHECK(sema_delete(value(bytes + 3)) == 0 && !owned && !semaphores && frees == 2);
    CHECK(sema_create(bytes + 3, "", 2, 0, 1, NULL) == 0 && value(bytes + 3) == 3);
    memcpy(saved, bytes, sizeof saved);
    alarm_seen = 0;
    alarm_on_free = 1;
    if (!sigsetjmp(timeout_context, 1)) {
        (void)sema_delete(value(bytes + 3));
        CHECK(0);
    }
    CHECK(alarm_seen && !owned && !semaphores && frees == 3 && !memcmp(bytes, saved, sizeof bytes));
    CHECK(!timeout_blocked());
    table_available();
    CHECK(sigaction(SIGALRM, &previous_action, NULL) == 0);

    /* Test-only access to private state reaches the real int32 limit without billions
       of allocations. Production has no reduced limit or ID-reset API. */
    next_semaphore_id = INT32_MAX;
    CHECK(sema_create(bytes + 3, "", 2, 0, 1, NULL) == 0 && value(bytes + 3) == INT32_MAX);
    CHECK(next_semaphore_id == (uint64_t)INT32_MAX + 1);
    memcpy(saved, bytes, sizeof saved);
    const int before_limit = allocations;
    FATAL(sema_create(bytes + 3, "", 2, 0, 1, NULL), "sceKernelCreateSema");
    CHECK(allocations == before_limit && owned && semaphores == owned && !memcmp(bytes, saved, sizeof bytes));
    CHECK(sema_delete(INT32_MAX) == 0 && !owned && !semaphores && frees == 4);
    FATAL(sema_create(bytes + 3, "", 2, 0, 1, NULL), "sceKernelCreateSema");
    CHECK(allocations == before_limit && !owned && !semaphores && next_semaphore_id == (uint64_t)INT32_MAX + 1);
    CHECK(!memcmp(bytes, saved, sizeof bytes) && !timeout_blocked());
    table_available();
    CHECK(pthread_sigmask(SIG_SETMASK, &original_mask, NULL) == 0);
    if (!failures) puts("semaphore ownership, limits and timeout deferral: ok");
    return failures != 0;
}
