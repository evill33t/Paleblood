/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_PROCESS_H
#define RUNTIME_PROCESS_H

/* The running guest process, as the system libraries see it. Set by whoever loads the
   executable (runtime/boot.c) before the guest runs. */

#include <stdint.h>
#include "runtime/cpu.h"
#include "runtime/logical_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A module the title ships that the runtime answers for without mapping it (the game's own data
   lists them; tools/boot.py passes each to runtime/boot.c as --stub-module ID:NAME). A request to
   load it through the system module library is reported as done (runtime/syslib/sysmodule.c). */
typedef struct {
    uint16_t id;         /* its system module id, the one sceSysmoduleLoadModule is given */
    const char *name;    /* its library name, for reports */
    int loaded;          /* the guest has loaded it and not unloaded it since */
} rt_stub_module;

#define RT_MAX_STUB_MODULES 32

typedef struct {
    void *image;         /* where the executable is mapped */
    uint64_t image_size;
    void *proc_param;    /* its process parameter block (PT_SCE_PROCPARAM), or NULL */
    void **heap_api;     /* the C library's heap functions, as it registered them, or NULL */
    rt_stub_module stub_modules[RT_MAX_STUB_MODULES];
    uint32_t stub_module_count;
} rt_process_info;

extern rt_process_info rt_process;

/* Registers the calling host thread as the guest's main thread. Before guest execution this
   fixes an own-runtime CPU mapping from all actually allowed host CPUs, and reports it. Optional
   BB_RUNTIME_CPU_MAP is an ordered decimal permutation of that full set. Unrepresentable or
   unsupported configurations stop by name; this policy is not a measured console CPU mask.
   Darwin startup registers only the calling thread's owned identity/stack. It supplies no CPU
   mapping by default; scePthreadAttrGet stops by name unless a separate explicit logical-slot
   policy has been registered. Native affinity remains unsupported on Darwin. */
void rt_thread_register_main(void *stack, uint64_t stack_size);

/* Embedding/testing form with a caller-owned provider context (lifetime: whole guest run).
   Registers only once and only the current main thread. Returns an internal CPU status;
   failures publish no thread/topology. No host scheduling state is changed. */
rt_cpu_status rt_thread_register_main_cpu(void *stack, uint64_t stack_size, rt_cpu_query query,
                                         void *context, const char *mapping);

/* Explicit own-port alternative: immutable unpinned logical slots, not allowed host CPU IDs.
   Registers the calling main thread once; count must be 1..64. No hardware query or scheduler
   operation occurs. Failure leaves identity/policy unpublished. The value is owned for the main
   thread lifetime; AttrGet captures its mask, while fresh attributes remain unrestricted. */
rt_logical_cpu_result rt_thread_register_main_logical(void *stack, uint64_t stack_size, uint64_t count);

/* The detach state (0 joinable, 1 detached) recorded on a guest thread attribute variable, for the
   system call that creates a thread from it; caller names that call. A state the guest did not set
   is one the system supplies (a fresh attribute's default, or the state scePthreadAttrGet copied
   from a running thread), and it is not known, so asking for it stops the process (rt_fatal). So
   does a null or empty variable, for which thread creation would use the system's defaults. */
int rt_thread_attr_detach_state(void *const *attr, const char *caller);

/* Stops the process with a message naming the system function: behaviour the runtime does not
   know is never guessed. */
_Noreturn void rt_fatal(const char *function, const char *message);

#ifdef __cplusplus
}
#endif

#endif
