/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Realtime service for a writable guest timeval and NULL timezone. Host clock selection
   is port policy; native timeval padding and microsecond widths are not the guest wire. */
#include "runtime/process.h"
#include "runtime/syslib.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

_Static_assert(sizeof(time_t) == sizeof(int64_t) && (time_t)-1 < 0,
               "the realtime adapter requires signed 64-bit host seconds");
_Static_assert(sizeof(suseconds_t) <= sizeof(int64_t) && (suseconds_t)-1 < 0,
               "host microseconds must fit a signed guest field");

typedef struct {
    int64_t seconds;
    int64_t microseconds;
} guest_timeval;
_Static_assert(sizeof(guest_timeval) == 16, "guest timeval is two signed 64-bit fields");
_Static_assert(offsetof(guest_timeval, microseconds) == 8, "guest microseconds follow seconds");

static int posix_gettimeofday(void *output, const void *timezone)
{
    if (timezone)
        rt_fatal("gettimeofday", "non-NULL timezone behavior is not implemented");
    if (!output)
        rt_fatal("gettimeofday", "NULL output behavior is not implemented");

    struct timeval native;
    const int saved_errno = errno;
    const int result = gettimeofday(&native, NULL);
    errno = saved_errno; /* Guest error state is not the host thread's errno. */
    if (result != 0)
        rt_fatal("gettimeofday", "host realtime query failed; guest error translation is not established");
    const int64_t microseconds = (int64_t)native.tv_usec;
    if (microseconds < 0 || microseconds >= INT64_C(1000000))
        rt_fatal("gettimeofday", "host realtime query returned invalid microseconds");
    const guest_timeval value = {(int64_t)native.tv_sec, microseconds};
    memcpy(output, &value, sizeof value); /* Success-only publication, including unaligned output. */
    return 0;
}

RT_SYSLIB("libScePosix", gettimeofday, posix_gettimeofday);
RT_SYSLIB("libkernel", gettimeofday, posix_gettimeofday);
