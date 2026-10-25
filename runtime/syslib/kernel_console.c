/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "runtime/console.h"
#include "runtime/process.h"
#include "runtime/syslib.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

_Static_assert(sizeof(int32_t) == 4, "guest errno is a four-byte object");
_Static_assert(sizeof(rt_syscall_result) == 16, "syscall results contain two full-width words");
_Static_assert(sizeof(void *) == 8, "owned syscall pointers require a 64-bit host");

/* This table owns host duplicates, never the borrowed output. Its only mutator is owner.
   The release-published state makes the once-written pthread_t safe to inspect elsewhere. */
struct descriptor { int32_t guest; int host; struct descriptor *next; };
static struct descriptor *descriptors;
static int anchor = -1;
static pthread_t owner;
enum { NEVER_PREPARED, PREPARING, READY, FAILED, POISONED, SHUT_DOWN };
static atomic_int state;
static _Thread_local int32_t guest_errno;

int32_t *rt_guest_errno_address(void)
{
    const int saved = errno;
    int32_t *result = &guest_errno;
    errno = saved;
    return result;
}
int rt_console_ready(void)
{
    const int saved = errno;
    const int ready = atomic_load_explicit(&state, memory_order_acquire) == READY;
    errno = saved;
    return ready;
}

static rt_console_status owner_status(void)
{
    const int current = atomic_load_explicit(&state, memory_order_acquire);
    if (current == NEVER_PREPARED || current == PREPARING) return RT_CONSOLE_NOT_READY;
    if (!pthread_equal(pthread_self(), owner)) return RT_CONSOLE_WRONG_THREAD;
    if (current == POISONED) return RT_CONSOLE_CLOSE_UNCERTAIN;
    return current == READY ? RT_CONSOLE_OK : RT_CONSOLE_NOT_READY;
}
static int block_alarm(sigset_t *previous)
{
    sigset_t blocked;
    if (sigemptyset(&blocked) || sigaddset(&blocked, SIGALRM)) return -1;
    return pthread_sigmask(SIG_BLOCK, &blocked, previous);
}
static rt_console_status restore_alarm(const sigset_t *previous, rt_console_status status, int saved)
{
    /* A pending alarm may nonlocally exit during unblocking, before our caller resumes. */
    errno = saved;
    return pthread_sigmask(SIG_SETMASK, previous, NULL) ? RT_CONSOLE_SIGNAL_MASK : status;
}
static rt_console_call diagnostic(rt_console_status status, int saved)
{
    errno = saved;
    return (rt_console_call){status, {UINT64_MAX, UINT64_MAX}};
}
static rt_console_call success(uint64_t value, int saved)
{
    errno = saved;
    return (rt_console_call){RT_CONSOLE_OK, {value, 0}};
}
static rt_console_call guest_failure(int32_t error, int saved)
{
    guest_errno = error;
    errno = saved;
    return (rt_console_call){RT_CONSOLE_OK, {UINT64_MAX, UINT64_MAX}};
}
/* Explicit FreeBSD errno compatibility values, not recovered PS4 service results.
   Unknown host failures are diagnostics instead of arbitrary numeric translation. */
static rt_console_call host_failure(int host_error, int saved)
{
    int32_t error;
    switch (host_error) {
    case EBADF: error = 9; break;
    case EINTR: error = 4; break;
    case EIO: error = 5; break;
    case ENOMEM: error = 12; break;
    case EFAULT: error = 14; break;
    case EINVAL: error = 22; break;
    case ENFILE: error = 23; break;
    case EMFILE: error = 24; break;
    case EFBIG: error = 27; break;
    case ENOSPC: error = 28; break;
    case EPIPE: error = 32; break;
    case EAGAIN: error = 35; break;
    default: return diagnostic(RT_CONSOLE_UNKNOWN_HOST_ERROR, saved);
    }
    return guest_failure(error, saved);
}

rt_console_status rt_console_prepare(int output_fd)
{
    const int saved = errno;
    int expected = NEVER_PREPARED;
    if (!atomic_compare_exchange_strong_explicit(&state, &expected, PREPARING,
                                                 memory_order_acq_rel, memory_order_acquire)) {
        const rt_console_status result = expected != PREPARING && !pthread_equal(pthread_self(), owner)
                                         ? RT_CONSOLE_WRONG_THREAD : RT_CONSOLE_ALREADY_PREPARED;
        errno = saved;
        return result;
    }
    owner = pthread_self();
    sigset_t previous;
    rt_console_status result = RT_CONSOLE_SIGNAL_MASK;
    if (block_alarm(&previous)) {
        atomic_store_explicit(&state, FAILED, memory_order_release);
        errno = saved;
        return result;
    }
    const int flags = fcntl(output_fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) == O_RDONLY) result = RT_CONSOLE_INVALID_OUTPUT;
    else {
        anchor = fcntl(output_fd, F_DUPFD_CLOEXEC, 0);
        result = anchor < 0 ? RT_CONSOLE_HOST_SETUP : RT_CONSOLE_OK;
    }
    atomic_store_explicit(&state, result == RT_CONSOLE_OK ? READY : FAILED, memory_order_release);
    result = restore_alarm(&previous, result, saved);
    errno = saved;
    return result;
}

rt_console_call rt_console_open(const char *path, uint64_t flags, uint64_t mode)
{
    const int saved = errno;
    const rt_console_status available = owner_status();
    if (available != RT_CONSOLE_OK) return diagnostic(available, saved);
    if (flags != 1) return diagnostic(RT_CONSOLE_UNSUPPORTED_FLAGS, saved);
    if (!path || strcmp(path, "/dev/console")) return diagnostic(RT_CONSOLE_UNSUPPORTED_PATH, saved);
    (void)mode; /* No creation in the admitted flags1 profile. */
    sigset_t previous;
    if (block_alarm(&previous)) return diagnostic(RT_CONSOLE_SIGNAL_MASK, saved);
    struct descriptor **slot = &descriptors;
    int32_t guest = 3;
    while (*slot && (*slot)->guest == guest) {
        if (guest == INT32_MAX) {
            const rt_console_status status = restore_alarm(&previous, RT_CONSOLE_OK, saved);
            return status == RT_CONSOLE_OK ? guest_failure(24, saved) : diagnostic(status, saved);
        }
        guest++;
        slot = &(*slot)->next;
    }
    struct descriptor *record = malloc(sizeof *record);
    int host_error = 0;
    if (!record) host_error = ENOMEM;
    else {
        record->host = fcntl(anchor, F_DUPFD_CLOEXEC, 0);
        if (record->host < 0) { host_error = errno; free(record); }
        else {
            record->guest = guest;
            record->next = *slot;
            *slot = record; /* Reachable before unblocking a pending timeout. */
        }
    }
    const rt_console_status status = restore_alarm(&previous, RT_CONSOLE_OK, saved);
    if (status != RT_CONSOLE_OK) return diagnostic(status, saved);
    return host_error ? host_failure(host_error, saved) : success((uint64_t)guest, saved);
}

static struct descriptor **find_descriptor(uint64_t guest)
{
    if (guest < 3 || guest > INT32_MAX) return NULL;
    struct descriptor **slot = &descriptors;
    while (*slot && (uint64_t)(*slot)->guest < guest) slot = &(*slot)->next;
    return *slot && (uint64_t)(*slot)->guest == guest ? slot : NULL;
}
rt_console_call rt_console_write(uint64_t descriptor, const void *buffer, uint64_t count)
{
    const int saved = errno;
    const rt_console_status available = owner_status();
    if (available != RT_CONSOLE_OK) return diagnostic(available, saved);
    struct descriptor **slot = find_descriptor(descriptor);
    if (!slot) return guest_failure(9, saved);
    if (count > (uint64_t)SSIZE_MAX) return guest_failure(22, saved);
    /* No mask or lock covers the potentially blocking call. A SIGALRM nonlocal exit leaves
       the existing registry/anchor reachable for quiescent owner cleanup. */
    const ssize_t count_written = write((*slot)->host, buffer, (size_t)count);
    const int host_error = errno;
    return count_written < 0 ? host_failure(host_error, saved) : success((uint64_t)count_written, saved);
}
rt_console_call rt_console_close(uint64_t descriptor)
{
    const int saved = errno;
    const rt_console_status available = owner_status();
    if (available != RT_CONSOLE_OK) return diagnostic(available, saved);
    struct descriptor **slot = find_descriptor(descriptor);
    if (!slot) return guest_failure(9, saved);
    sigset_t previous;
    if (block_alarm(&previous)) return diagnostic(RT_CONSOLE_SIGNAL_MASK, saved);
    struct descriptor *record = *slot;
    *slot = record->next;
    const int failed = close(record->host);
    free(record);
    if (failed) atomic_store_explicit(&state, POISONED, memory_order_release);
    const rt_console_status status = restore_alarm(&previous, failed ? RT_CONSOLE_CLOSE_UNCERTAIN : RT_CONSOLE_OK, saved);
    /* Failed close has unspecified descriptor state: retired, no retry, and guest execution
       must stop by name. It is not recast as a definite guest close error. */
    return status == RT_CONSOLE_OK ? success(0, saved) : diagnostic(status, saved);
}

rt_console_status rt_console_shutdown(void)
{
    const int saved = errno;
    const int current = atomic_load_explicit(&state, memory_order_acquire);
    if (current == NEVER_PREPARED) { errno = saved; return RT_CONSOLE_OK; }
    if (current == PREPARING) { errno = saved; return RT_CONSOLE_NOT_READY; }
    if (!pthread_equal(pthread_self(), owner)) { errno = saved; return RT_CONSOLE_WRONG_THREAD; }
    if (current == SHUT_DOWN) { errno = saved; return RT_CONSOLE_OK; }
    sigset_t previous;
    if (block_alarm(&previous)) { errno = saved; return RT_CONSOLE_SIGNAL_MASK; }
    rt_console_status result = current == POISONED ? RT_CONSOLE_CLOSE_UNCERTAIN : RT_CONSOLE_OK;
    while (descriptors) {
        struct descriptor *record = descriptors;
        descriptors = record->next;
        if (close(record->host)) result = RT_CONSOLE_CLOSE_UNCERTAIN;
        free(record);
    }
    if (anchor >= 0) {
        const int owned = anchor;
        anchor = -1;
        if (close(owned)) result = RT_CONSOLE_CLOSE_UNCERTAIN;
    }
    atomic_store_explicit(&state, SHUT_DOWN, memory_order_release);
    result = restore_alarm(&previous, result, saved);
    errno = saved;
    return result;
}
const char *rt_console_status_message(rt_console_status status)
{
    switch (status) {
    case RT_CONSOLE_OK: return "owned console operation completed";
    case RT_CONSOLE_NOT_READY: return "prepare the owned output stream with --owned-console before guest execution; it cannot be reopened after shutdown";
    case RT_CONSOLE_ALREADY_PREPARED: return "owned console preparation may be attempted only once";
    case RT_CONSOLE_WRONG_THREAD: return "console descriptors belong to a different host thread";
    case RT_CONSOLE_UNSUPPORTED_PATH: return "only the exact owned /dev/console output stream is supported";
    case RT_CONSOLE_UNSUPPORTED_FLAGS: return "owned console admits only full raw open flags1";
    case RT_CONSOLE_INVALID_OUTPUT: return "a valid writable borrowed host output descriptor is required";
    case RT_CONSOLE_HOST_SETUP: return "host duplicate-anchor preparation failed";
    case RT_CONSOLE_UNKNOWN_HOST_ERROR: return "host operation failed without an owned BSD errno mapping";
    case RT_CONSOLE_CLOSE_UNCERTAIN: return "host close state is uncertain; descriptor retired without retry";
    case RT_CONSOLE_SIGNAL_MASK: return "cannot block or restore the owned SIGALRM transition mask";
    }
    return "unknown owned console status";
}
static rt_syscall_result checked(const char *name, rt_console_call call)
{
    if (call.status != RT_CONSOLE_OK) rt_fatal(name, rt_console_status_message(call.status));
    return call.result;
}
static rt_syscall_result kernel_open(const char *path, uint64_t flags, uint64_t mode)
{ return checked("_open", rt_console_open(path, flags, mode)); }
static rt_syscall_result kernel_write(uint64_t fd, const void *buffer, uint64_t count)
{ return checked("_write", rt_console_write(fd, buffer, count)); }
static rt_syscall_result kernel_close(uint64_t fd)
{ return checked("_close", rt_console_close(fd)); }
RT_SYSLIB("libkernel", _open, kernel_open);
RT_SYSLIB("libkernel", _write, kernel_write);
RT_SYSLIB("libkernel", _close, kernel_close);
RT_SYSLIB("libkernel", __error, rt_guest_errno_address);
