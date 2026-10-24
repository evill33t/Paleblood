/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <errno.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static int test_host_gettimeofday(struct timeval *, void *);
static _Noreturn void test_fatal(const char *, const char *);
#define gettimeofday test_host_gettimeofday
#define rt_fatal test_fatal
#include "../runtime/syslib/kernel_time.c"
#undef gettimeofday
#undef rt_fatal

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #x); abort(); } } while (0)
static jmp_buf fatal_target;
static int expect_fatal, calls, host_result;
static int fatal_errno;
static int64_t seconds, microseconds;
_Alignas(uint64_t) static unsigned char rejected_output[18];

static int test_host_gettimeofday(struct timeval *out, void *zone)
{
    CHECK(zone == NULL);
    ++calls;
    memset(out, 0x5a, sizeof *out); /* Poison host padding, especially Darwin's upper usec bytes. */
    out->tv_sec = (time_t)seconds;
    out->tv_usec = (suseconds_t)microseconds;
    errno = EIO;
    return host_result;
}

static _Noreturn void test_fatal(const char *caller, const char *reason)
{
    fatal_errno = errno; /* The libc recovery path need not preserve this later. */
    CHECK(expect_fatal && strcmp(caller, "gettimeofday") == 0 && reason && *reason);
    longjmp(fatal_target, 1);
}

static void success(int64_t sec, int64_t usec)
{
    _Alignas(uint64_t) unsigned char bytes[18];
    CHECK((uintptr_t)(bytes + 1) % sizeof(uint64_t) != 0);
    memset(bytes, 0xa5, sizeof bytes);
    seconds = sec; microseconds = usec; host_result = 0;
    const int old_calls = calls;
    errno = ERANGE;
    const int result = posix_gettimeofday(bytes + 1, NULL);
    CHECK(result == 0 && calls == old_calls + 1 && errno == ERANGE);
    int64_t fields[2];
    memcpy(fields, bytes + 1, sizeof fields);
    CHECK(fields[0] == sec && fields[1] == usec);
    CHECK(bytes[0] == 0xa5 && bytes[17] == 0xa5);
}

static void rejected(int null_output, int has_timezone, int result, int64_t usec,
                     int expected_host_calls)
{
    unsigned char original[sizeof rejected_output];
    CHECK((uintptr_t)(rejected_output + 1) % sizeof(uint64_t) != 0);
    memset(rejected_output, 0xa5, sizeof rejected_output);
    memcpy(original, rejected_output, sizeof original);
    const int old_calls = calls;
    host_result = result; seconds = INT64_C(0x100000001); microseconds = usec;
    expect_fatal = 1; fatal_errno = 0;
    if (setjmp(fatal_target) == 0) {
        errno = ERANGE; /* Establish the provider's input after saving the jump context. */
        (void)posix_gettimeofday(null_output ? NULL : rejected_output + 1,
                                has_timezone ? (const void *)(uintptr_t)1 : NULL);
        CHECK(0);
    }
    expect_fatal = 0;
    CHECK(calls == old_calls + expected_host_calls);
    CHECK(memcmp(rejected_output, original, sizeof original) == 0);
    CHECK(fatal_errno == ERANGE);
}

int main(void)
{
    success(INT64_C(0x100000001), 0);
    success(-INT64_C(0x100000001), 999999);
    success(INT64_MIN, 1);
    success(INT64_MAX, 999999);
    rejected(1, 0, 0, 0, 0);
    rejected(0, 1, 0, 0, 0);
    rejected(1, 1, 0, 0, 0);
    rejected(0, 0, -1, 123456, 1);
    rejected(0, 0, 1, 123456, 1);
    rejected(0, 0, 0, -1, 1);
    rejected(0, 0, 0, 1000000, 1);
    return 0;
}
