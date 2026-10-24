/* SPDX-License-Identifier: GPL-2.0-or-later */
/* System call group: kernel, threads and memory. Threads and thread attributes (libkernel,
   libScePosix).

   Facts used: a thread or attribute variable holds a pointer to an object the library allocates
   (FreeBSD libthr, which the PS4's thread library is); the SCE names return 0 or 0x80020000 plus a
   FreeBSD error number, and their POSIX names (libScePosix) return that error number itself. An
   attribute records only what has been set or copied into it; reading a value the runtime does not
   know (one the system would supply) is not implemented, so the boot stops there by name instead of
   guessing. */
#include "runtime/process.h"
#include "runtime/syslib.h"

#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BSD_ENOMEM = 12, BSD_EINVAL = 22 };
#define SCE_KERNEL_ERROR(e) ((int)(0x80020000u + (unsigned)(e)))

/* PTHREAD_CREATE_JOINABLE and PTHREAD_CREATE_DETACHED in FreeBSD's <pthread.h>. The OpenOrbis
   homebrew SDK header (include/orbis/_types/pthread.h) defines ORBIS_PTHREAD_CREATE_JOINABLE and
   ORBIS_PTHREAD_CREATE_DETACHED with the same values; it follows FreeBSD's text, so it corroborates
   them rather than measuring them. */
enum { DETACH_JOINABLE = 0, DETACH_DETACHED = 1 };

typedef struct {
    void *stack;
    uint64_t stack_size;
} guest_thread;

typedef struct {
    int has_stack;
    void *stack;
    uint64_t stack_size;
    int has_detach_state;
    int detach_state;
    int has_affinity;
    uint64_t affinity;
} guest_attr;

static int sce(int bsd) { return bsd ? SCE_KERNEL_ERROR(bsd) : 0; }

static guest_thread main_thread;
static _Thread_local guest_thread *current;
static rt_cpu_topology main_topology;
static rt_logical_cpu_slots main_logical_slots;
/* These domains have different meanings: logical slots never stand for native CPU IDs. */
enum main_cpu_domain { MAIN_CPU_NONE, MAIN_CPU_NATIVE_ALLOWED, MAIN_CPU_LOGICAL_UNPINNED };
static enum main_cpu_domain main_cpu_policy;
static int main_registered;

rt_cpu_status rt_thread_register_main_cpu(void *stack, uint64_t stack_size, rt_cpu_query query,
                                         void *context, const char *mapping)
{
    if (main_registered) return RT_CPU_BAD_CONFIGURATION;
    rt_cpu_topology topology;
    const rt_cpu_status status = rt_cpu_topology_init(&topology, query, context, mapping);
    if (status != RT_CPU_OK) return status;
    main_topology = topology;
    main_cpu_policy = MAIN_CPU_NATIVE_ALLOWED;
    main_thread = (guest_thread){.stack = stack, .stack_size = stack_size};
    current = &main_thread;
    main_registered = 1;
    return RT_CPU_OK;
}

rt_logical_cpu_result rt_thread_register_main_logical(void *stack, uint64_t stack_size, uint64_t count)
{
    if (main_registered) return (rt_logical_cpu_result){RT_LOGICAL_CPU_INVALID_STATE, 0};
    rt_logical_cpu_slots slots;
    const rt_logical_cpu_result status = rt_logical_cpu_slots_init(count, &slots);
    if (status.status != RT_LOGICAL_CPU_OK) return status;
    main_logical_slots = slots;
    main_cpu_policy = MAIN_CPU_LOGICAL_UNPINNED;
    main_thread = (guest_thread){.stack = stack, .stack_size = stack_size};
    current = &main_thread;
    main_registered = 1;
    return status;
}

void rt_thread_register_main(void *stack, uint64_t stack_size)
{
#if defined(__APPLE__) && defined(__MACH__)
    if (main_registered) rt_fatal("main-thread identity", "already registered");
    main_thread = (guest_thread){.stack = stack, .stack_size = stack_size};
    current = &main_thread;
    main_registered = 1;
    fputs("runtime: Darwin main-thread identity registered; native CPU affinity is unsupported\n", stderr);
#else
    const rt_cpu_status status = rt_thread_register_main_cpu(stack, stack_size, rt_cpu_query_native,
                                                            NULL, getenv("BB_RUNTIME_CPU_MAP"));
    if (status != RT_CPU_OK) rt_fatal("main-thread CPU topology", rt_cpu_status_message(status));
    fputs("runtime: own CPU topology (guest bit=host CPU):", stderr);
    for (size_t i = 0; i < main_topology.count; i++)
        fprintf(stderr, " %zu=%u", i, main_topology.host[i]);
    fputs("; port policy, not a console affinity measurement\n", stderr);
#endif
}

static guest_thread *thread_self(void)
{
    if (!current) rt_fatal("scePthreadSelf", "no guest thread registered on this host thread");
    return current;
}

/* Two thread variables name the same thread when they hold the same object pointer. Non-zero when
   equal, zero otherwise, with no error return (FreeBSD pthread_equal(3)). */
static int thread_equal(guest_thread *a, guest_thread *b)
{
    return a == b;
}

/* sched_yield: the calling thread gives up the processor and the call returns 0 (POSIX and FreeBSD
   sched_yield(2), which define no error for it). A guest thread runs on a host thread, so the
   host's yield is the same request. A host failure has no FreeBSD error number we could report, so
   it stops by name instead. */
static int kernel_sched_yield(void)
{
    if (sched_yield() != 0) rt_fatal("sched_yield", "the host did not yield the processor");
    return 0;
}

/* scePthreadYield asks for the same yield, but its PS4-visible signature is not established:
   FreeBSD's pthread_yield(3) returns nothing, Linux's returns an int, and no PS4-specific public
   source or guest call site we can cite says which one the guest sees. Returning would leave the
   value a caller might read undefined, so the call stops by name. It is registered with
   RT_SYSLIB_UNRESOLVED: a boot that reaches it names the call, and coverage leaves the import
   unchecked, since a diagnostic is not an implementation. */
static void thread_yield(void)
{
    rt_fatal("scePthreadYield", "the PS4 return behaviour is not established by public evidence");
}

static int attr_init(guest_attr **attr)
{
    if (!attr) return SCE_KERNEL_ERROR(BSD_EINVAL);
    guest_attr *a = calloc(1, sizeof *a);
    if (!a) return SCE_KERNEL_ERROR(BSD_ENOMEM);
    *attr = a;
    return 0;
}

static int attr_destroy(guest_attr **attr)
{
    if (!attr || !*attr) return SCE_KERNEL_ERROR(BSD_EINVAL);
    free(*attr);
    *attr = NULL;
    return 0;
}

/* Replace the supported stack fields and capture the selected owned main-thread policy.
   A previously set detach state is dropped: the copied thread's detach semantics remain
   unresolved. Backend failure occurs before changing the destination attribute. */
static int attr_get(guest_thread *thread, guest_attr **attr)
{
    if (!thread || !attr || !*attr) return SCE_KERNEL_ERROR(BSD_EINVAL);
    if (!main_registered || thread != &main_thread || current != &main_thread)
        rt_fatal("scePthreadAttrGet", "affinity queries currently support only the calling registered main thread");
    uint64_t mask;
    if (main_cpu_policy == MAIN_CPU_LOGICAL_UNPINNED) {
        const rt_logical_cpu_result status = rt_logical_cpu_slots_read(&main_logical_slots, &mask);
        if (status.status != RT_LOGICAL_CPU_OK)
            rt_fatal("scePthreadAttrGet", "the owned logical CPU-slot policy is invalid");
    } else if (main_cpu_policy == MAIN_CPU_NATIVE_ALLOWED) {
        const rt_cpu_status status = rt_cpu_topology_read(&main_topology, &mask);
        if (status != RT_CPU_OK) rt_fatal("scePthreadAttrGet", rt_cpu_status_message(status));
    } else {
        rt_fatal("scePthreadAttrGet", "Darwin native CPU affinity backend is unsupported");
    }
    **attr = (guest_attr){.has_stack = 1, .stack = thread->stack, .stack_size = thread->stack_size,
                         .has_affinity = 1, .affinity = mask};
    return 0;
}

/* Supplied libkernel establishes two pointer arguments and exactly one eight-byte output store
   on success. Its null-mask attribute sentinel is unrestricted, unlike a captured thread mask.
   This port chooses an internal16-byte CPU set and at most64 mapped guest CPUs: fresh attributes
   therefore publish low64 all-ones; AttrGet publishes a snapshot from the selected
   native-allowed or unpinned logical policy.
   No output/null/alignment check is invented: memcpy preserves an unaligned/alias eight-byte
   publication, and an inaccessible output faults. Invalid attributes leave the output alone. */
static int attr_get_affinity(guest_attr **attr, void *output)
{
    if (!attr || !*attr) return SCE_KERNEL_ERROR(BSD_EINVAL);
    const uint64_t mask = (*attr)->has_affinity ? (*attr)->affinity : UINT64_MAX;
    memcpy(output, &mask, sizeof mask);
    return 0;
}

/* Whether a thread created with the attribute starts detached or joinable (POSIX
   pthread_attr_setdetachstate). Any other value is EINVAL (POSIX; FreeBSD pthread_attr(3)), and so
   is a variable holding no attribute object (libthr's _pthread_attr_setdetachstate, thr_attr.c). */
static int attr_set_detach_state(guest_attr **attr, int state)
{
    if (!attr || !*attr) return BSD_EINVAL;
    if (state != DETACH_JOINABLE && state != DETACH_DETACHED) return BSD_EINVAL;
    (*attr)->has_detach_state = 1;
    (*attr)->detach_state = state;
    return 0;
}

static int sce_attr_set_detach_state(guest_attr **attr, int state)
{
    return sce(attr_set_detach_state(attr, state));
}

/* Read the state through this, never from the field: a fresh attribute's detach_state is zero,
   which is the joinable value, so reading the field would assume joinable silently. The two
   messages tell a boot log which unknown the guest reached. */
int rt_thread_attr_detach_state(void *const *attr, const char *caller)
{
    const guest_attr *a = attr ? *attr : NULL;
    if (!a)
        rt_fatal(caller, "no attribute object: the PS4 default detach state is not established");
    if (!a->has_detach_state)
        rt_fatal(caller, "no detach state set: the one the system supplies is not established");
    return a->detach_state;
}

RT_SYSLIB("libkernel", scePthreadSelf, thread_self);
RT_SYSLIB("libkernel", scePthreadEqual, thread_equal);
RT_SYSLIB("libkernel", sched_yield, kernel_sched_yield);
RT_SYSLIB_UNRESOLVED("libkernel", scePthreadYield, thread_yield);
RT_SYSLIB("libkernel", scePthreadAttrInit, attr_init);
RT_SYSLIB("libkernel", scePthreadAttrDestroy, attr_destroy);
RT_SYSLIB("libkernel", scePthreadAttrGet, attr_get);
RT_SYSLIB("libkernel", scePthreadAttrGetaffinity, attr_get_affinity);
RT_SYSLIB("libkernel", scePthreadAttrSetdetachstate, sce_attr_set_detach_state);

/* libScePosix: errno values. pthread_attr_init and pthread_attr_destroy are not registered yet, so
   eboot's own POSIX path stops at those first. */
RT_SYSLIB("libScePosix", pthread_attr_setdetachstate, attr_set_detach_state);
