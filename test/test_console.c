/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "runtime/console.h"
#include "runtime/syslib.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); abort(); } } while (0)

/* Only host boundary aliases: default paths call the actual host operations. No production
   reset, injection flag or alternative syscall implementation is shipped. */
static int owned_fcntl(int, int, ...);
static ssize_t owned_write(int, const void *, size_t);
static int owned_close(int);
static void *owned_malloc(size_t);
static void owned_free(void *);
#define fcntl owned_fcntl
#define write owned_write
#define close owned_close
#define malloc owned_malloc
#define free owned_free
#include "../runtime/syslib/kernel_console.c"
#undef fcntl
#undef write
#undef close
#undef malloc
#undef free

static int duplicate_error, write_error, close_error, allocation_error;
static unsigned duplicates, writes, closes, allocations, releases, live_allocations;
static int owned_fds[128], failed_close_fd = -1;
static unsigned owned_count, failed_close_calls;
static int owned_fcntl(int fd, int command, ...)
{
    if (command == F_GETFL) return fcntl(fd, command);
    CHECK(command == F_DUPFD_CLOEXEC);
    va_list args;
    va_start(args, command);
    const int lower = va_arg(args, int);
    va_end(args);
    duplicates++;
    if (duplicate_error) { errno = duplicate_error; return -1; }
    const int result = fcntl(fd, command, lower);
    if (result >= 0) {
        CHECK(owned_count < sizeof owned_fds / sizeof owned_fds[0]);
        owned_fds[owned_count++] = result;
        CHECK(fcntl(result, F_GETFD) & FD_CLOEXEC);
    }
    return result;
}
static ssize_t owned_write(int fd, const void *data, size_t count)
{
    writes++;
    if (write_error) { errno = write_error; return -1; }
    return write(fd, data, count);
}
static int owned_close(int fd)
{
    closes++;
    if (fd == failed_close_fd) failed_close_calls++;
    if (close_error) {
        failed_close_fd = fd;
        failed_close_calls = 1;
        errno = close_error;
        return -1; /* An injected ambiguous close deliberately leaves the real FD open. */
    }
    return close(fd);
}
static void *owned_malloc(size_t size)
{
    allocations++;
    if (allocation_error) { errno = ENOMEM; return NULL; }
    void *memory = malloc(size);
    if (memory) live_allocations++;
    return memory;
}
static void owned_free(void *memory)
{
    if (memory) { CHECK(live_allocations); live_allocations--; releases++; }
    free(memory);
}

static sigjmp_buf fatal_jump;
static int fatal_armed;
static char fatal_function[64];
_Noreturn void rt_fatal(const char *function, const char *message)
{
    CHECK(message && *message);
    if (!fatal_armed) { fprintf(stderr, "%s: %s\n", function, message); abort(); }
    CHECK(strlen(function) < sizeof fatal_function);
    strcpy(fatal_function, function);
    siglongjmp(fatal_jump, 1);
}
static const rt_syslib_entry *entry(const char *symbol)
{
    const rt_syslib_entry *found = NULL;
    size_t count = 0;
    for (const rt_syslib_entry *p = rt_syslib_begin(); p != rt_syslib_end(); p++) {
        CHECK(p->library && !strcmp(p->library, "libkernel"));
        CHECK(p->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
        CHECK(!strcmp(p->symbol, "_open") || !strcmp(p->symbol, "_write")
              || !strcmp(p->symbol, "_close") || !strcmp(p->symbol, "__error"));
        count++;
        if (!strcmp(p->symbol, symbol)) { CHECK(!found); found = p; }
    }
    CHECK(count == 4 && found);
    return found;
}
static rt_syscall_result guest_open(const char *path, uint64_t flags, uint64_t mode)
{ return ((rt_syscall_result (*)(const char *, uint64_t, uint64_t))entry("_open")->fn)(path, flags, mode); }
static rt_syscall_result guest_write(uint64_t fd, const void *data, uint64_t count)
{ return ((rt_syscall_result (*)(uint64_t, const void *, uint64_t))entry("_write")->fn)(fd, data, count); }
static rt_syscall_result guest_close(uint64_t fd)
{ return ((rt_syscall_result (*)(uint64_t))entry("_close")->fn)(fd); }
static int32_t *guest_error(void)
{ return ((int32_t *(*)(void))entry("__error")->fn)(); }
static void good(rt_syscall_result value, uint64_t expected)
{ CHECK(value.value == expected && value.extra == 0); }
static void bad(rt_syscall_result value, int32_t expected)
{ CHECK(value.value == UINT64_MAX && value.extra == UINT64_MAX && *guest_error() == expected); }
static int temporary_file(void)
{
    char name[] = "/tmp/paleblood-owned-console-XXXXXX";
    int fd = mkstemp(name);
    CHECK(fd >= 0 && unlink(name) == 0);
    return fd;
}
static void all_owned_closed(void)
{
    for (unsigned i = 0; i < owned_count; i++) {
        if (owned_fds[i] == failed_close_fd) continue;
        errno = 0;
        CHECK(fcntl(owned_fds[i], F_GETFD) == -1 && errno == EBADF);
    }
    CHECK(live_allocations == 0);
}
static uintptr_t worker_cell_address;
static void *wrong_owner(void *unused)
{
    (void)unused;
    errno = EDOM;
    int32_t *cell = guest_error();
    CHECK(*cell == 0 && guest_error() == cell && errno == EDOM);
    *cell = 702;
    worker_cell_address = (uintptr_t)cell;
    CHECK(rt_console_open((const char *)(uintptr_t)1, UINT64_MAX, 0).status == RT_CONSOLE_WRONG_THREAD);
    CHECK(rt_console_write(3, (const void *)(uintptr_t)1, UINT64_MAX).status == RT_CONSOLE_WRONG_THREAD);
    CHECK(rt_console_close(3).status == RT_CONSOLE_WRONG_THREAD);
    CHECK(rt_console_prepare(1) == RT_CONSOLE_WRONG_THREAD);
    CHECK(rt_console_shutdown() == RT_CONSOLE_WRONG_THREAD);
    CHECK(*cell == 702 && errno == EDOM);
    return NULL;
}
static void regular(void)
{
    CHECK(!rt_console_ready());
    errno = ENOTTY;
    int32_t *cell = guest_error();
    CHECK(cell == rt_guest_errno_address() && *cell == 0 && errno == ENOTTY);
    CHECK(rt_console_open("/dev/console", 1, 0).status == RT_CONSOLE_NOT_READY);
    const int output = temporary_file(), reader = dup(output);
    CHECK(reader >= 0);
    const int flags = fcntl(output, F_GETFL), descriptor_flags = fcntl(output, F_GETFD);
    const int unrelated_host_fd = fcntl(output, F_DUPFD_CLOEXEC, 64);
    CHECK(unrelated_host_fd >= 64);
    errno = ENOTTY;
    CHECK(rt_console_prepare(output) == RT_CONSOLE_OK && errno == ENOTTY && rt_console_ready());
    CHECK(rt_console_prepare(output) == RT_CONSOLE_ALREADY_PREPARED);
    good(guest_open("/dev/console", 1, 0), 3);
    good(guest_open("/dev/console", 1, UINT64_MAX), 4);
    CHECK(fcntl(output, F_GETFL) == flags && fcntl(output, F_GETFD) == descriptor_flags
          && lseek(output, 0, SEEK_CUR) == 0);
    errno = ENOTTY;
    bad(guest_close(1), 9);
    bad(guest_close((uint64_t)unrelated_host_fd), 9);
    CHECK(fcntl(unrelated_host_fd, F_GETFD) >= 0 && close(unrelated_host_fd) == 0);
    bad(guest_close(UINT64_C(0x100000003)), 9);
    bad(guest_write(1, (const void *)(uintptr_t)1, UINT64_MAX), 9);
    CHECK(errno == ENOTTY && fcntl(STDOUT_FILENO, F_GETFD) >= 0);
    good(guest_write(3, "A", 1), 1);
    _Alignas(uint64_t) unsigned char source_bytes[16];
    memset(source_bytes, 0xa5, sizeof source_bytes);
    source_bytes[3] = 'B'; source_bytes[4] = 'C';
    CHECK((uintptr_t)(source_bytes + 3) % 8 != 0);
    good(guest_write(4, source_bytes + 3, 2), 2);
    CHECK(source_bytes[3] == 'B' && source_bytes[4] == 'C');
    for (size_t i = 0; i < sizeof source_bytes; i++) if (i != 3 && i != 4) CHECK(source_bytes[i] == 0xa5);
    CHECK(*cell == 9 && lseek(output, 0, SEEK_CUR) == 3);
    CHECK(close(output) == 0); /* The independently owned anchor remains live. */
    good(guest_close(3), 0);
    good(guest_open("/dev/console", 1, 123), 3); /* Lowest free guest descriptor. */
    good(guest_write(3, "D", 1), 1);
    const unsigned before = writes;
    good(guest_write(4, (const void *)(uintptr_t)1, 0), 0);
    CHECK(writes == before + 1);
    bad(guest_write(4, (const void *)(uintptr_t)1, UINT64_MAX), 22);
    CHECK(writes == before + 1);
    *cell = 701;
    const unsigned before_dup = duplicates, before_write = writes, before_close = closes;
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, wrong_owner, NULL) == 0);
    if (pthread_join(thread, NULL)) _Exit(2);
    CHECK(*cell == 701 && worker_cell_address != (uintptr_t)cell
          && duplicates == before_dup && writes == before_write && closes == before_close);
    _Alignas(uint64_t) unsigned char bytes[24];
    memset(bytes, 0xa5, sizeof bytes);
    CHECK(pread(reader, bytes + 3, 4, 0) == 4 && !memcmp(bytes + 3, "ABCD", 4));
    for (size_t i = 0; i < sizeof bytes; i++) if (i < 3 || i >= 7) CHECK(bytes[i] == 0xa5);
    errno = ENOTTY;
    CHECK(rt_console_shutdown() == RT_CONSOLE_OK && errno == ENOTTY && !rt_console_ready());
    CHECK(rt_console_shutdown() == RT_CONSOLE_OK);
    CHECK(rt_console_prepare(reader) == RT_CONSOLE_ALREADY_PREPARED);
    CHECK(fcntl(reader, F_GETFD) >= 0 && close(reader) == 0);
    all_owned_closed();
    puts("regular: exact registry, independent descriptors, offsets, flags, TLS errno and cleanup passed");
    puts("console regular: PASS");
}
static void errors(void)
{
    const int output = temporary_file();
    CHECK(rt_console_prepare(output) == RT_CONSOLE_OK);
    const unsigned initial_dup = duplicates;
    CHECK(rt_console_open(NULL, 1, 0).status == RT_CONSOLE_UNSUPPORTED_PATH);
    CHECK(rt_console_open("/tmp/anything", 1, 0).status == RT_CONSOLE_UNSUPPORTED_PATH);
    CHECK(rt_console_open("/dev/console", UINT64_C(0x100000001), 0).status == RT_CONSOLE_UNSUPPORTED_FLAGS);
    CHECK(duplicates == initial_dup);
    fatal_armed = 1;
    if (!sigsetjmp(fatal_jump, 1)) { (void)guest_open("/dev/console", 2, 0); CHECK(0); }
    CHECK(!strcmp(fatal_function, "_open") && duplicates == initial_dup);
    fatal_armed = 0;
    errno = ENOTTY;
    allocation_error = 1;
    bad(guest_open("/dev/console", 1, 0), 12);
    CHECK(errno == ENOTTY && duplicates == initial_dup && live_allocations == 0);
    allocation_error = 0;
    duplicate_error = EMFILE;
    bad(guest_open("/dev/console", 1, 0), 24);
    CHECK(live_allocations == 0 && errno == ENOTTY);
    duplicate_error = 0;
    good(guest_open("/dev/console", 1, 0), 3);
    const int native_errors[] = {EBADF,EINTR,EIO,ENOMEM,EFAULT,EINVAL,ENFILE,EMFILE,EFBIG,ENOSPC,EPIPE,EAGAIN};
    const int32_t bsd_errors[] = {9,4,5,12,14,22,23,24,27,28,32,35};
    for (size_t i = 0; i < sizeof native_errors / sizeof native_errors[0]; i++) {
        write_error = native_errors[i];
        const unsigned before = writes;
        errno = ENOTTY;
        bad(guest_write(3, "x", 1), bsd_errors[i]);
        CHECK(errno == ENOTTY && writes == before + 1);
    }
    write_error = ENOEXEC; /* Deliberately not in the compatibility table. */
    *guest_error() = 303;
    CHECK(rt_console_write(3, "x", 1).status == RT_CONSOLE_UNKNOWN_HOST_ERROR);
    CHECK(*guest_error() == 303);
    fatal_armed = 1;
    if (!sigsetjmp(fatal_jump, 1)) { (void)guest_write(3, "x", 1); CHECK(0); }
    CHECK(!strcmp(fatal_function, "_write"));
    fatal_armed = 0;
    write_error = 0;
    good(guest_open("/dev/console", 1, 0), 4);
    close_error = EINTR;
    errno = ENOTTY;
    CHECK(rt_console_close(3).status == RT_CONSOLE_CLOSE_UNCERTAIN && errno == ENOTTY);
    close_error = 0;
    CHECK(!rt_console_ready() && fcntl(failed_close_fd, F_GETFD) >= 0);
    CHECK(rt_console_close(3).status == RT_CONSOLE_CLOSE_UNCERTAIN && failed_close_calls == 1);
    CHECK(rt_console_shutdown() == RT_CONSOLE_CLOSE_UNCERTAIN && failed_close_calls == 1);
    CHECK(rt_console_shutdown() == RT_CONSOLE_OK && failed_close_calls == 1);
    all_owned_closed();
    CHECK(close(failed_close_fd) == 0); /* Test owns the explicitly injected uncertainty. */
    CHECK(close(output) == 0);
    puts("errors: named unsupported scope, four-byte errno, paired failure, resource ownership and no close retry passed");
    puts("console errors: PASS");
}
static size_t fill_pipe(int fd)
{
    /* A large initial write also exercises hosts that grow a pipe for larger requests. */
    char bytes[65536];
    memset(bytes, 'F', sizeof bytes);
    size_t total = 0;
    for (;;) {
        const ssize_t n = write(fd, bytes, sizeof bytes);
        if (n < 0) { CHECK(errno == EAGAIN); break; }
        CHECK(n > 0);
        total += (size_t)n;
        CHECK(total <= 1024 * 1024);
    }
    CHECK(total >= 4096);
    return total;
}
static void pipe_output(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);
    const int flags = fcntl(fds[1], F_GETFL);
    CHECK(fcntl(fds[1], F_SETFL, flags | O_NONBLOCK) == 0);
    const size_t capacity = fill_pipe(fds[1]);
    /* Filling a pipe can update host-maintained status bits. Snapshot the actual
       supplied state at the backend boundary, rather than assuming a round trip. */
    const int supplied_flags = fcntl(fds[1], F_GETFL);
    CHECK(supplied_flags >= 0 && (supplied_flags & O_NONBLOCK));
    CHECK(rt_console_prepare(fds[1]) == RT_CONSOLE_OK);
    good(guest_open("/dev/console", 1, 0), 3);
    CHECK(fcntl(fds[1], F_GETFL) == supplied_flags);
    errno = ENOTTY;
    bad(guest_write(3, "x", 1), 35);
    CHECK(errno == ENOTTY);
    char drained[4096];
    CHECK(read(fds[0], drained, sizeof drained) == (ssize_t)sizeof drained);
    char *large = malloc(capacity + sizeof drained);
    CHECK(large);
    memset(large, 'P', capacity + sizeof drained);
    const unsigned before = writes;
    const rt_syscall_result partial = guest_write(3, large, capacity + sizeof drained);
    CHECK(partial.extra == 0 && partial.value > 0 && partial.value < capacity + sizeof drained && writes == before + 1);
    free(large);
    CHECK(close(fds[1]) == 0 && close(fds[0]) == 0);
    struct sigaction previous, ignored;
    memset(&ignored, 0, sizeof ignored);
    ignored.sa_handler = SIG_IGN;
    CHECK(sigemptyset(&ignored.sa_mask) == 0 && sigaction(SIGPIPE, &ignored, &previous) == 0);
    errno = ENOTTY;
    bad(guest_write(3, "x", 1), 32);
    CHECK(errno == ENOTTY && sigaction(SIGPIPE, &previous, NULL) == 0);
    CHECK(rt_console_shutdown() == RT_CONSOLE_OK);
    all_owned_closed();
    puts("pipe: inherited nonblocking flags, one real partial write and explicit inherited SIGPIPE policy passed");
    puts("console pipe: PASS");
}
static sigjmp_buf timeout_jump;
static volatile sig_atomic_t timeout_armed, timeout_seen;
static void timeout_handler(int number)
{
    (void)number;
    timeout_seen = 1;
    if (timeout_armed) siglongjmp(timeout_jump, 1);
    _Exit(92);
}
static int timeout_fds[2];
static void timeout_cleanup(void)
{
    CHECK(pipe(timeout_fds) == 0);
    const int flags = fcntl(timeout_fds[1], F_GETFL);
    CHECK(fcntl(timeout_fds[1], F_SETFL, flags | O_NONBLOCK) == 0);
    (void)fill_pipe(timeout_fds[1]);
    CHECK(fcntl(timeout_fds[1], F_SETFL, flags) == 0);
    CHECK(rt_console_prepare(timeout_fds[1]) == RT_CONSOLE_OK);
    good(guest_open("/dev/console", 1, 0), 3);
    good(guest_open("/dev/console", 1, 0), 4);
    CHECK(close(timeout_fds[1]) == 0);
    struct sigaction action, previous;
    memset(&action, 0, sizeof action);
    action.sa_handler = timeout_handler;
    CHECK(sigemptyset(&action.sa_mask) == 0 && sigaction(SIGALRM, &action, &previous) == 0);
    sigset_t unblocked, old_mask, current;
    CHECK(sigemptyset(&unblocked) == 0 && sigaddset(&unblocked, SIGALRM) == 0);
    CHECK(pthread_sigmask(SIG_UNBLOCK, &unblocked, &old_mask) == 0);
    timeout_armed = 1;
    if (!sigsetjmp(timeout_jump, 1)) {
        alarm(1);
        (void)guest_write(3, "blocked", 7); /* Full blocking pipe, no draining reader. */
        CHECK(0);
    }
    timeout_armed = 0;
    alarm(0);
    CHECK(timeout_seen && writes == 1);
    CHECK(pthread_sigmask(SIG_SETMASK, NULL, &current) == 0 && sigismember(&current, SIGALRM) == 0);
    CHECK(rt_console_ready() && rt_console_shutdown() == RT_CONSOLE_OK);
    all_owned_closed();
    CHECK(close(timeout_fds[0]) == 0);
    CHECK(pthread_sigmask(SIG_SETMASK, &old_mask, NULL) == 0);
    CHECK(sigaction(SIGALRM, &previous, NULL) == 0);
    puts("timeout: actual blocked host write escaped by SIGALRM; unblocked mask and all tracked resources recovered");
    puts("console timeout: PASS");
}
static void prepare_failure(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);
    errno = ENOTTY;
    CHECK(rt_console_prepare(fds[0]) == RT_CONSOLE_INVALID_OUTPUT && errno == ENOTTY);
    CHECK(!rt_console_ready() && duplicates == 0 && live_allocations == 0);
    CHECK(rt_console_prepare(fds[1]) == RT_CONSOLE_ALREADY_PREPARED);
    CHECK(rt_console_shutdown() == RT_CONSOLE_OK && rt_console_shutdown() == RT_CONSOLE_OK);
    CHECK(fcntl(fds[0], F_GETFD) >= 0 && fcntl(fds[1], F_GETFD) >= 0);
    CHECK(close(fds[0]) == 0 && close(fds[1]) == 0);
    puts("prepare-failure: borrowed read-only endpoint refused without duplicate/ownership or reset");
    puts("console prepare-failure: PASS");
}
int main(int argc, char **argv)
{
    CHECK(argc == 2);
    if (!strcmp(argv[1], "regular")) regular();
    else if (!strcmp(argv[1], "errors")) errors();
    else if (!strcmp(argv[1], "pipe")) pipe_output();
    else if (!strcmp(argv[1], "timeout")) timeout_cleanup();
    else if (!strcmp(argv[1], "prepare-failure")) prepare_failure();
    else CHECK(0);
    return 0;
}
