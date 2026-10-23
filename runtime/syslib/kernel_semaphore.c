/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Process-private creation/deletion records only. This does not implement semaphore
   wait/signal, console name namespaces, count validation, or scheduling. */
#include "runtime/process.h"
#include "runtime/syslib.h"

#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct semaphore_record {
    int32_t id;
    struct semaphore_record *next;
} semaphore_record;

static pthread_mutex_t semaphore_table_lock = PTHREAD_MUTEX_INITIALIZER;
static semaphore_record *semaphores;
static uint64_t next_semaphore_id = 1;
typedef struct { sigset_t previous_mask; } semaphore_guard;

static void restore_timeout(const char *operation, const semaphore_guard *guard)
{
    /* The boot handler can siglongjmp immediately when a pending SIGALRM is unblocked. */
    if (pthread_sigmask(SIG_SETMASK, &guard->previous_mask, NULL))
        rt_fatal(operation, "could not restore the previous signal mask");
}

static semaphore_guard lock_table(const char *operation)
{
    semaphore_guard guard;
    sigset_t timeout_signal;
    if (sigemptyset(&timeout_signal) || sigaddset(&timeout_signal, SIGALRM)
        || pthread_sigmask(SIG_BLOCK, &timeout_signal, &guard.previous_mask))
        rt_fatal(operation, "could not defer the boot timeout signal");
    if (pthread_mutex_lock(&semaphore_table_lock)) {
        restore_timeout(operation, &guard);
        rt_fatal(operation, "semaphore table mutex lock failed");
    }
    return guard;
}

static void unlock_table(const char *operation)
{
    if (pthread_mutex_unlock(&semaphore_table_lock))
        rt_fatal(operation, "semaphore table mutex unlock failed");
}

static int32_t sema_create(void *slot, const char *name, uint32_t attributes,
                           int32_t initial_count, int32_t maximum_count, const void *options)
{
    const char *operation = "sceKernelCreateSema";
    /* Observed wrapper precedence: no other argument is accessed on this branch. */
    if (options) return (int32_t)UINT32_C(0x80020016);
    if (attributes != 2 || initial_count != 0 || maximum_count != 1)
        rt_fatal(operation, "only the process-private attribute-2 binary creation policy (0 of 1) is implemented");
    if (!name || name[0])
        rt_fatal(operation, "only a non-NULL empty name is implemented; named semaphore policy is unknown");

    const semaphore_guard guard = lock_table(operation);
    if (next_semaphore_id > INT32_MAX) {
        unlock_table(operation);
        restore_timeout(operation, &guard);
        rt_fatal(operation, "process-private positive semaphore IDs are exhausted; IDs are never reused");
    }
    semaphore_record *record = calloc(1, sizeof *record);
    if (!record) {
        unlock_table(operation);
        restore_timeout(operation, &guard);
        rt_fatal(operation, "semaphore record allocation failed; kernel error translation is not established");
    }
    record->id = (int32_t)next_semaphore_id++;
    record->next = semaphores;
    semaphores = record;
    const uint64_t handle = (uint64_t)record->id;
    unlock_table(operation);
    /* The borrowed slot must be writable on success. There is no old-slot read, NULL
       check, or rollback for an invalid slot; a fault may leave a process-owned record.
       Publish outside the mutex, with SIGALRM still deferred until publication completes. */
    memcpy(slot, &handle, sizeof handle);
    restore_timeout(operation, &guard);
    return 0;
}

static int32_t sema_delete(uint64_t handle)
{
    const char *operation = "sceKernelDeleteSema";
    const semaphore_guard guard = lock_table(operation);
    semaphore_record **link = &semaphores;
    while (*link && (uint64_t)(*link)->id != handle) link = &(*link)->next;
    if (!*link) {
        unlock_table(operation);
        restore_timeout(operation, &guard);
        rt_fatal(operation, "unknown or deleted full-width semaphore ID; kernel diagnostic/error behavior is unknown");
    }
    semaphore_record *record = *link;
    *link = record->next;
    unlock_table(operation);
    /* No waiters/readers or wait/signal operation exist in this unit. The removed
       record is exclusively owned; no borrowed guest slot is dereferenced or changed. */
    free(record);
    restore_timeout(operation, &guard);
    return 0;
}

RT_SYSLIB("libkernel", sceKernelCreateSema, sema_create);
RT_SYSLIB("libkernel", sceKernelDeleteSema, sema_delete);
