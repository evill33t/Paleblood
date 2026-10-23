/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Isolate a boot-style timeout during registry bookkeeping. The linked production core has
   test-only calloc/free aliases; neither production source nor API has an injection hook. */
#define _GNU_SOURCE
#include "runtime/direct_memory.h"
#include "runtime/syslib.h"

#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static sigjmp_buf recovered;
static volatile sig_atomic_t raise_next, raised, returned_from_raise, delivered;
static int live_blocks, frees;
static unsigned char output[8];
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

void *rt_timeout_calloc(size_t count, size_t size)
{
    void *pointer = calloc(count, size);
    if (pointer) live_blocks++;
    if (pointer && raise_next) {
        raise_next = 0;
        raised++;
        if (raise(SIGALRM)) abort();
        returned_from_raise = 1;
    }
    return pointer;
}

void rt_timeout_free(void *pointer)
{
    if (pointer) { live_blocks--; frees++; }
    free(pointer);
}

static void timeout_handler(int signal_number)
{
    (void)signal_number;
    delivered++;
    siglongjmp(recovered, 1);
}

static int same_mask(const sigset_t *a, const sigset_t *b)
{
    for (int number = 1; number < NSIG; number++)
        if (sigismember(a, number) != sigismember(b, number)) return 0;
    return 1;
}

static void *entry(const char *symbol)
{
    for (const rt_syslib_entry *e = rt_syslib_begin(); e && e != rt_syslib_end(); e++)
        if (!strcmp(e->library, "libkernel") && !strcmp(e->symbol, symbol)) return e->fn;
    return NULL;
}

static int child_regression(void)
{
    uint64_t (*size_entry)(void) = (uint64_t (*)(void))entry("sceKernelGetDirectMemorySize");
    int32_t (*allocate_entry)(uint64_t, uint64_t, uint64_t, uint64_t, int32_t, void *) =
        (int32_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, int32_t, void *))entry("sceKernelAllocateDirectMemory");
    REQUIRE(size_entry && allocate_entry);
    sigset_t original_mask, expected_mask, observed;
    REQUIRE(pthread_sigmask(SIG_SETMASK, NULL, &original_mask) == 0);
    expected_mask = original_mask;
    REQUIRE(sigaddset(&expected_mask, SIGUSR1) == 0);
    REQUIRE(sigdelset(&expected_mask, SIGALRM) == 0);
    REQUIRE(pthread_sigmask(SIG_SETMASK, &expected_mask, NULL) == 0);
    REQUIRE(pthread_sigmask(SIG_SETMASK, NULL, &expected_mask) == 0);
    struct sigaction action = {0}, previous_action;
    action.sa_handler = timeout_handler;
    REQUIRE(sigemptyset(&action.sa_mask) == 0);
    REQUIRE(sigaction(SIGALRM, &action, &previous_action) == 0);
    const uint64_t page = RT_DIRECT_PAGE_SIZE, capacity = 4 * page;
    REQUIRE(rt_direct_runtime_configure(capacity) == RT_DIRECT_OK);
    REQUIRE(live_blocks == 1); /* Pool object only, before an allocation ledger exists. */

    sigset_t blocked = expected_mask;
    REQUIRE(sigaddset(&blocked, SIGALRM) == 0);
    REQUIRE(pthread_sigmask(SIG_SETMASK, &blocked, NULL) == 0);
    REQUIRE(pthread_sigmask(SIG_SETMASK, NULL, &blocked) == 0);
    REQUIRE(size_entry() == capacity);
    REQUIRE(pthread_sigmask(SIG_SETMASK, NULL, &observed) == 0 && same_mask(&observed, &blocked));
    REQUIRE(pthread_sigmask(SIG_SETMASK, &expected_mask, NULL) == 0);
    memset(output, 0x7b, sizeof output);
    if (sigsetjmp(recovered, 1) == 0) {
        raise_next = 1;
        (void)allocate_entry(0, capacity, page, 0, 0, output);
        fprintf(stderr, "pending timeout did not transfer control to the initialized recovery point\n");
        return 1;
    }

    /* Old code strands the mutex after the jump. A fresh default-action alarm makes the
       subsequent ordinary query/shutdown fail in two seconds rather than hanging the suite. */
    struct sigaction watchdog = {0};
    watchdog.sa_handler = SIG_DFL;
    REQUIRE(sigemptyset(&watchdog.sa_mask) == 0 && sigaction(SIGALRM, &watchdog, NULL) == 0);
    alarm(2);
    REQUIRE(delivered == 1 && raised == 1);
    REQUIRE(pthread_sigmask(SIG_SETMASK, NULL, &observed) == 0 && same_mask(&observed, &expected_mask));
    for (size_t i = 0; i < sizeof output; i++) REQUIRE(output[i] == 0x7b);
    REQUIRE(size_entry() == capacity); /* Proves the mutex was released before handler delivery. */
    REQUIRE(returned_from_raise == 1); /* raise returned while deferred, before ledger publication. */
    REQUIRE(live_blocks == 2); /* The timed-out allocation is still cleanup-owned. */
    uint64_t second = UINT64_C(0xaabbccdd11223344);
    REQUIRE(allocate_entry(0, page, page, 0, 0, &second) == (int32_t)UINT32_C(0x8002000c));
    REQUIRE(second == UINT64_C(0xaabbccdd11223344));
    REQUIRE(rt_direct_runtime_shutdown() == RT_DIRECT_OK && live_blocks == 0 && frees == 2);
    REQUIRE(pthread_sigmask(SIG_SETMASK, NULL, &observed) == 0 && same_mask(&observed, &expected_mask));
    alarm(0);
    REQUIRE(sigaction(SIGALRM, &previous_action, NULL) == 0);
    REQUIRE(pthread_sigmask(SIG_SETMASK, &original_mask, NULL) == 0);
    return 0;
}

int main(void)
{
    const pid_t child = fork();
    REQUIRE(child >= 0);
    if (child == 0) _exit(child_regression());
    int result;
    REQUIRE(waitpid(child, &result, 0) == child);
    REQUIRE(WIFEXITED(result) && WEXITSTATUS(result) == 0);
    return 0;
}
