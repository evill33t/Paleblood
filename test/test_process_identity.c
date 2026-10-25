/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); abort(); } } while (0)
static pid_t test_host_pid(void);
static int test_entropy(void *, size_t);
static void *test_allocate(size_t);
static void test_release(void *);
/* Host boundary substitutions exist only in this test translation unit. */
#define getpid test_host_pid
#define getentropy test_entropy
#define malloc test_allocate
#define free test_release
/* glibc normalizes _DEFAULT_SOURCE after headers; let the included source declare its mode. */
#undef _DEFAULT_SOURCE
#include "../runtime/syslib/kernel_process_data.c"
#undef free
#undef malloc
#undef getentropy
#undef getpid
#include "runtime/process.h"

static atomic_int bad_pid;
static atomic_int pid_calls;
static int allocation_failure, allocations, entropy_calls;
static int entropy_mode = 2, entered, release_entropy;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static sigjmp_buf fatal_jump;
static int fatal_armed;
static const char *fatal_name, *fatal_message;
static volatile sig_atomic_t active_child;

static pid_t test_host_pid(void)
{
    atomic_fetch_add(&pid_calls, 1);
    errno = EOVERFLOW; /* Identity boundaries must restore the caller's errno. */
    const int bad = atomic_load(&bad_pid);
    return bad == 1 ? (pid_t)0 : bad == 2 ? (pid_t)-1 : getpid();
}
static void *test_allocate(size_t bytes)
{
    if (allocation_failure) { errno = ENOMEM; return NULL; }
    void *p = malloc(bytes);
    if (p) allocations++;
    return p;
}
static void test_release(void *p)
{
    if (p) { CHECK(allocations > 0); allocations--; }
    free(p);
}
static int test_entropy(void *out, size_t bytes)
{
    CHECK(bytes == 64);
    entropy_calls++;
    errno = EIO;
    memset(out, 0, bytes);
    if (entropy_mode == 0) return -1;
    if (entropy_mode == 1) return 0;
    if (entropy_mode == 3) {
        CHECK(pthread_mutex_lock(&mutex) == 0);
        entered = 1;
        CHECK(pthread_cond_broadcast(&changed) == 0);
        while (!release_entropy) CHECK(pthread_cond_wait(&changed, &mutex) == 0);
        CHECK(pthread_mutex_unlock(&mutex) == 0);
    }
    memset(out, 0xa5, bytes);
    return 0;
}
_Noreturn void rt_fatal(const char *function, const char *message)
{
    fatal_name = function;
    fatal_message = message;
    if (fatal_armed) siglongjmp(fatal_jump, 1);
    fprintf(stderr, "unexpected fatal %s: %s\n", function, message);
    abort();
}
typedef uint64_t (*pid_provider)(void);
static pid_provider providers[2];
static void registry(void)
{
    size_t count = 0;
    for (const rt_syslib_entry *e = rt_syslib_begin(); e != rt_syslib_end(); e++) {
        if (!e->library) continue;
        CHECK(!strcmp(e->symbol, "getpid"));
        CHECK(e->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
        const size_t index = !strcmp(e->library, "libkernel") ? 0 : 1;
        CHECK(index == 0 || !strcmp(e->library, "libScePosix"));
        CHECK(!providers[index] && e->fn);
        providers[index] = (pid_provider)e->fn;
        count++;
    }
    CHECK(count == 2 && providers[0] && providers[1] && providers[0] == providers[1]);
}
static void named_failure(const char *text)
{
    for (size_t i = 0; i < 2; i++) {
        fatal_name = fatal_message = NULL;
        fatal_armed = 1;
        if (sigsetjmp(fatal_jump, 1) == 0) { (void)providers[i](); CHECK(0); }
        fatal_armed = 0;
        CHECK(fatal_name && !strcmp(fatal_name, "getpid"));
        CHECK(fatal_message && strstr(fatal_message, text));
    }
}
static void unpublished(rt_process_data_status expected)
{
    struct { uint32_t before; int32_t pid; uint32_t after; } out = {0x12345678, -719, 0xabcdef01};
    errno = EDOM;
    CHECK(rt_process_data_pid(&out.pid) == expected);
    CHECK(out.before == 0x12345678 && out.pid == -719 && out.after == 0xabcdef01);
    CHECK(errno == EDOM && !rt_process_data_ready() && !rt_process_data_parameters());
    CHECK(errno == EDOM);
    size_t records = 0;
    for (const rt_syslib_data *d = rt_syslib_data_begin(); d != rt_syslib_data_end(); d++) {
        if (!d->library) continue;
        CHECK(d->ready && !d->ready());
        records++;
    }
    CHECK(records == 4 && errno == EDOM);
}
static int32_t ready_pid(void)
{
    struct { uint32_t before; int32_t pid; uint32_t after; } out = {0x12345678, -719, 0xabcdef01};
    errno = ERANGE;
    CHECK(rt_process_data_pid(&out.pid) == RT_PROCESS_DATA_OK);
    CHECK(out.before == 0x12345678 && out.after == 0xabcdef01 && out.pid > 0);
    CHECK(errno == ERANGE && rt_process_data_ready() && rt_process_data_parameters());
    CHECK(errno == ERANGE);
    for (size_t i = 0; i < 2; i++) {
        CHECK(providers[i]() == (uint64_t)(uint32_t)out.pid);
        CHECK(errno == ERANGE);
    }
    return out.pid;
}
static int32_t worker_pid;
static void *prepare_worker(void *unused)
{
    (void)unused;
    const char *args[] = {"/owned/game", NULL};
    CHECK(rt_process_data_prepare(1, args, 0, NULL) == RT_PROCESS_DATA_OK);
    worker_pid = ready_pid();
    return NULL;
}
static void regular(void)
{
    unpublished(RT_PROCESS_DATA_NOT_READY);
    named_failure("--owned-process-data");
    errno = E2BIG;
    CHECK(rt_process_data_pid(NULL) == RT_PROCESS_DATA_INVALID && errno == E2BIG);
    entropy_mode = 3;
    pthread_t worker;
    CHECK(pthread_create(&worker, NULL, prepare_worker, NULL) == 0);
    CHECK(pthread_mutex_lock(&mutex) == 0);
    while (!entered) CHECK(pthread_cond_wait(&changed, &mutex) == 0);
    unpublished(RT_PROCESS_DATA_BUSY);
    CHECK(!parameters && !guest_program_name && !guest_environment && process_identity == 0);
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_BUSY);
    release_entropy = 1;
    CHECK(pthread_cond_broadcast(&changed) == 0);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    if (pthread_join(worker, NULL) != 0) _Exit(2);
    CHECK(entropy_calls == 1 && allocations == 1);
    CHECK(ready_pid() == (int32_t)getpid() && worker_pid == ready_pid());
    void *saved = rt_process_data_parameters();
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_ALREADY_PREPARED);
    CHECK(rt_process_data_parameters() == saved && entropy_calls == 1 && allocations == 1);
    puts("process identity regular: PASS");
}
static void failure(void)
{
    CHECK(RT_PROCESS_DATA_OK == 0 && RT_PROCESS_DATA_INVALID == 1
          && RT_PROCESS_DATA_NO_MEMORY == 2 && RT_PROCESS_DATA_ENTROPY == 3
          && RT_PROCESS_DATA_BUSY == 4 && RT_PROCESS_DATA_ALREADY_PREPARED == 5);
    for (int bad = 1; bad <= 2; bad++) {
        atomic_store(&bad_pid, bad);
        CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_HOST_PID);
        unpublished(RT_PROCESS_DATA_NOT_READY);
        CHECK(allocations == 0 && entropy_calls == 0 && process_identity == 0);
    }
    atomic_store(&bad_pid, 0);
    allocation_failure = 1;
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_NO_MEMORY);
    CHECK(allocations == 0 && entropy_calls == 0);
    allocation_failure = 0;
    for (entropy_mode = 0; entropy_mode < 2; entropy_mode++) {
        CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_ENTROPY);
        unpublished(RT_PROCESS_DATA_NOT_READY);
        CHECK(allocations == 0 && !parameters && process_identity == 0);
        for (size_t i = 0; i < sizeof guest_guard; i++) CHECK(!guest_guard[i]);
    }
    entropy_mode = 2;
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_OK);
    CHECK(ready_pid() == (int32_t)getpid() && entropy_calls == 3 && allocations == 1);
    void *saved = parameters;
    atomic_store(&bad_pid, 1);
    unpublished(RT_PROCESS_DATA_HOST_PID);
    CHECK(parameters == saved); /* Rejection cannot destroy process-lifetime storage. */
    atomic_store(&bad_pid, 0);
    CHECK(ready_pid() == (int32_t)getpid());
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_ALREADY_PREPARED);
    CHECK(allocations == 1 && entropy_calls == 3 && parameters == saved);
    puts("process identity failure: PASS");
}
static void deadline_signal(int signal_number)
{
    (void)signal_number;
    if (active_child > 0) (void)kill((pid_t)active_child, SIGKILL);
    _Exit(91);
}
static double now(void)
{
    struct timespec ts;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}
static void wait_child(pid_t pid)
{
    int status = 0;
    const double deadline = now() + 4.0;
    for (;;) {
        const pid_t got = waitpid(pid, &status, WNOHANG);
        if (got == pid) break;
        CHECK(got == 0 || (got < 0 && errno == EINTR));
        if (now() >= deadline) {
            (void)kill(pid, SIGKILL);
            const double cleanup = now() + 1.0;
            while (now() < cleanup) {
                if (waitpid(pid, &status, WNOHANG) == pid) break;
                const struct timespec pause = {0, 10000000};
                (void)nanosleep(&pause, NULL);
            }
            fprintf(stderr, "owned child did not finish within bound\n");
            _Exit(3);
        }
        const struct timespec pause = {0, 10000000};
        (void)nanosleep(&pause, NULL);
    }
    active_child = 0;
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
static void send_record(int fd, const void *data, size_t bytes)
{
    const unsigned char *p = data;
    while (bytes) {
        const ssize_t n = write(fd, p, bytes);
        if (n < 0 && errno == EINTR) continue;
        CHECK(n > 0);
        p += (size_t)n; bytes -= (size_t)n;
    }
}
static void receive_record(int fd, void *data, size_t bytes)
{
    unsigned char *p = data;
    while (bytes) {
        const ssize_t n = read(fd, p, bytes);
        if (n < 0 && errno == EINTR) continue;
        CHECK(n > 0);
        p += (size_t)n; bytes -= (size_t)n;
    }
    unsigned char extra;
    CHECK(read(fd, &extra, 1) == 0);
}
static void fork_mode(void)
{
    unpublished(RT_PROCESS_DATA_NOT_READY);
    struct sigaction action = {0}, old_action;
    action.sa_handler = deadline_signal;
    CHECK(sigemptyset(&action.sa_mask) == 0 && sigaction(SIGALRM, &action, &old_action) == 0);
    alarm(10);
    int channel[2]; CHECK(pipe(channel) == 0);
    pid_t child = fork(); CHECK(child >= 0);
    if (child == 0) {
        CHECK(close(channel[0]) == 0);
        unpublished(RT_PROCESS_DATA_NOT_READY);
        CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_OK);
        const int32_t pid = ready_pid();
        send_record(channel[1], &pid, sizeof pid);
        CHECK(close(channel[1]) == 0); _Exit(0);
    }
    active_child = (sig_atomic_t)child;
    CHECK(close(channel[1]) == 0); wait_child(child);
    int32_t reported = 0; receive_record(channel[0], &reported, sizeof reported);
    CHECK(close(channel[0]) == 0 && reported == (int32_t)child); /* Parent fork result is independent oracle. */
    unpublished(RT_PROCESS_DATA_NOT_READY);
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_OK);
    const int32_t parent_pid = ready_pid();
    CHECK(pipe(channel) == 0); child = fork(); CHECK(child >= 0);
    if (child == 0) {
        CHECK(close(channel[0]) == 0);
        unpublished(RT_PROCESS_DATA_INHERITED);
        const int before_allocations = allocations, before_entropy = entropy_calls;
        CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_INHERITED);
        CHECK(allocations == before_allocations && entropy_calls == before_entropy);
        named_failure("host-fork");
        const int32_t preserved_parent_identity = process_identity;
        send_record(channel[1], &preserved_parent_identity, sizeof preserved_parent_identity);
        CHECK(close(channel[1]) == 0); _Exit(0);
    }
    active_child = (sig_atomic_t)child;
    CHECK(close(channel[1]) == 0); wait_child(child);
    reported = 0; receive_record(channel[0], &reported, sizeof reported);
    CHECK(close(channel[0]) == 0 && reported == parent_pid && (int32_t)child != parent_pid);
    CHECK(ready_pid() == parent_pid && allocations == 1);
    alarm(0); CHECK(sigaction(SIGALRM, &old_action, NULL) == 0);
    puts("process identity fork: PASS");
}
int main(int argc, char **argv)
{
    CHECK(argc == 2);
    registry();
    if (!strcmp(argv[1], "regular")) regular();
    else if (!strcmp(argv[1], "failure")) failure();
    else if (!strcmp(argv[1], "fork")) fork_mode();
    else CHECK(0);
    return 0;
}
