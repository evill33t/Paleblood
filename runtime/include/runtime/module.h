/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_MODULE_H
#define RUNTIME_MODULE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* An owned, immutable catalogue of already mapped modules. The caller retains the
   executable mappings and start override targets until after catalogue destruction. */
#define RT_MODULE_LIMIT 64
#define RT_MODULE_PREPARED 0
#define RT_MODULE_STARTING 1
#define RT_MODULE_STARTED 2
#define RT_MODULE_FAILED 3

typedef int32_t (*rt_module_entry)(uint64_t argument_size, const void *arguments, void *start_override);
typedef struct {
    const char *path;             /* exact guest-visible path, copied at preparation */
    int32_t handle;               /* positive, unique handle in this owned namespace */
    rt_module_entry entry;        /* admitted mapped entry; offset zero is valid */
    void *start_override;         /* admitted module_start export, or NULL */
    uint64_t dependencies;       /* catalogue indices; bit zero names record zero */
} rt_module_spec;

typedef struct rt_modules rt_modules;
typedef enum {
    RT_MODULE_OK = 0,
    RT_MODULE_BAD_ARGUMENT,
    RT_MODULE_BAD_LAYOUT,
    RT_MODULE_NO_MEMORY,
    RT_MODULE_WRONG_THREAD,
    RT_MODULE_BAD_SCOPE,
    RT_MODULE_UNKNOWN_PATH,
    RT_MODULE_ALREADY_STARTED,
    RT_MODULE_REENTRANT,
    RT_MODULE_INIT_FAILED
} rt_module_status;

/* Validate the complete graph before publication, copying paths and records.
   No initializer runs. Failure leaves *out unchanged. */
rt_module_status rt_modules_create(const rt_module_spec *specs, size_t count, rt_modules **out);
/* Activate only around one outer guest recovery scope on the creating host thread.
   Leaving a scope marks any nonlocally abandoned initializer as failed. */
rt_module_status rt_modules_enter(rt_modules *modules);
rt_module_status rt_modules_leave(rt_modules *modules);
/* Start dependencies once, then call the actual target entry on the current stack.
   This profile admits argument_size=0, arguments=NULL, flags=0. Actual signed DWORD
   initializer results are published to byte-addressed target_result if non-NULL, before checking
   negativity. A negative result is an owned failure, with no successful handle.
   Direct repeated loads and reentrant public loads remain unsupported. */
rt_module_status rt_modules_load(const char *path, void *target_result, int32_t *handle);
/* Stable byte for a call trampoline's read-only readiness gate; NULL if absent.
   The owner may inspect states while inactive. Never write through this address. */
const unsigned char *rt_modules_state(const rt_modules *modules, int32_t handle);
/* Owner only, outside a guest scope. Does not unmap modules or undo constructors. */
rt_module_status rt_modules_destroy(rt_modules *modules);
const char *rt_module_status_message(rt_module_status status);

#ifdef __cplusplus
}
#endif
#endif
