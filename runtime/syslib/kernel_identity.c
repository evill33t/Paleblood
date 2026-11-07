/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "runtime/process.h"
#include "runtime/process_data.h"
#include "runtime/syslib.h"
#include <stdint.h>

/* The native wrapper forwards syscall20; its numeric PID comes from a kernel.
   Our opt-in profile instead owns one genuine host-process identity. Only the
   low32 result is consumed by the established caller. No success RDX or native
   kernel error result is claimed by this uint64_t return. */
static uint64_t guest_getpid(void)
{
    int32_t pid;
    const rt_process_data_status status = rt_process_data_pid(&pid);
    if (status != RT_PROCESS_DATA_OK)
        rt_fatal("getpid", rt_process_data_status_message(status));
    return (uint64_t)(uint32_t)pid;
}
RT_SYSLIB("libkernel", getpid, guest_getpid);
RT_SYSLIB("libScePosix", getpid, guest_getpid);
