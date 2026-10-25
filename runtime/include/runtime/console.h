/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_CONSOLE_H
#define RUNTIME_CONSOLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned output-stream policy, not a console terminal or general filesystem.
   Prepare duplicates a writable host descriptor; it never owns the borrowed descriptor.
   One preparation attempt/lifetime per process. Descriptor operations and shutdown belong
   exclusively to that preparing host thread. ready() is an atomic observation; __error is
   usable by each host thread independently. The owner must not exit before shutdown. */
typedef enum {
    RT_CONSOLE_OK = 0,
    RT_CONSOLE_NOT_READY,
    RT_CONSOLE_ALREADY_PREPARED,
    RT_CONSOLE_WRONG_THREAD,
    RT_CONSOLE_UNSUPPORTED_PATH,
    RT_CONSOLE_UNSUPPORTED_FLAGS,
    RT_CONSOLE_INVALID_OUTPUT,
    RT_CONSOLE_HOST_SETUP,
    RT_CONSOLE_UNKNOWN_HOST_ERROR,
    RT_CONSOLE_CLOSE_UNCERTAIN,
    RT_CONSOLE_SIGNAL_MASK,
} rt_console_status;

/* The observed two-register error shape is retained. Success extra0 is owned policy,
   not a claim about native RDX. A non-OK backend status is an infrastructure/unsupported
   diagnostic: its result must not be published as a successful guest syscall. */
typedef struct { uint64_t value, extra; } rt_syscall_result;
typedef struct { rt_console_status status; rt_syscall_result result; } rt_console_call;

rt_console_status rt_console_prepare(int output_fd);
/* Requires quiescent owner after guest scope. Closes tracked duplicates only, once each.
   Idempotent after shutdown; it never re-closes an uncertain failed-close descriptor. */
rt_console_status rt_console_shutdown(void);
int rt_console_ready(void);
const char *rt_console_status_message(rt_console_status status);

/* Owned per-host-thread int32 cell, initially0, stable until that host thread exits.
   This is not the supplied kernel's conditional TCB/fallback implementation. Normal API
   returns preserve host errno; successful guest operations leave guest errno unchanged. */
int32_t *rt_guest_errno_address(void);

/* Exact path /dev/console and full raw flags1 only; mode is ignored for that noncreating
   profile. Guest descriptors are the lowest free int32>=3, independent of host numbers.
   Only valid readable path and write buffers for their declared extents are supported. */
rt_console_call rt_console_open(const char *path, uint64_t flags, uint64_t mode);
/* One host write, including zero count: no write-all, EINTR/EAGAIN retry, F_SETFL or SIGPIPE
   handler. SIGALRM is not blocked across write. Host output offset/status are shared by dup.
   Counts beyond host SSIZE_MAX are refused with owned EINVAL before buffer access. */
rt_console_call rt_console_write(uint64_t descriptor, const void *buffer, uint64_t count);
rt_console_call rt_console_close(uint64_t descriptor);

/* Short allocation/publication/close transitions block and restore SIGALRM. A synchronous
   invalid borrowed-pointer fault, cancellation or other nonlocal exit inside those transitions
   is outside this policy. SIGALRM may exit a blocked write: tracked resources remain reachable
   for owner shutdown. Other async signal handlers must not reenter this API. */

#ifdef __cplusplus
}
#endif
#endif
