/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Default, unnamed rwlock lifecycle only. The guest owns an eight-byte pointer slot;
   the allocated host object is private and does not reproduce a console object layout. */
#include "runtime/process.h"
#include "runtime/syslib.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(void *) == 8, "guest rwlock handles require 64-bit pointers");

typedef struct {
    pthread_rwlock_t host;
} guest_rwlock;

static int rwlock_init(void *slot, const void *attr, const char *name)
{
    if (attr)
        rt_fatal("scePthreadRwlockInit", "non-NULL attributes are not implemented");
    if (name)
        rt_fatal("scePthreadRwlockInit", "named rwlocks are not implemented");

    guest_rwlock *lock = calloc(1, sizeof *lock);
    if (!lock) return (int)UINT32_C(0x8002000c);
    /* Default host attributes are a port policy, not console scheduling/fairness evidence. */
    if (pthread_rwlock_init(&lock->host, NULL) != 0) {
        free(lock); /* no initialized host lock or published guest handle exists */
        rt_fatal("scePthreadRwlockInit", "host initialization failed; error translation is not established");
    }
    /* No read/check of the old slot and no invented NULL-slot error. The borrowed slot
       must be writable on success; failed allocation returns before touching it. */
    const uint64_t handle = (uint64_t)(uintptr_t)lock;
    memcpy(slot, &handle, sizeof handle);
    return 0;
}

static int rwlock_destroy(void *slot)
{
    uint64_t handle;
    memcpy(&handle, slot, sizeof handle);
    if (!handle) return 0;
    if (handle == 1)
        rt_fatal("scePthreadRwlockDestroy", "destroyed-handle diagnostic behavior is not implemented");

    /* Only a valid, initialized, exclusive/quiescent handle from rwlock_init is supported.
       On a host failure retain its reachable handle/allocation and stop. This error policy
       adds a host cleanup check; the recovered default console path has no busy check. */
    guest_rwlock *lock = (guest_rwlock *)(uintptr_t)handle;
    if (pthread_rwlock_destroy(&lock->host) != 0)
        rt_fatal("scePthreadRwlockDestroy", "host destruction failed; handle retained, error translation is not established");
    const uint64_t destroyed = 1;
    memcpy(slot, &destroyed, sizeof destroyed);
    free(lock); /* the successful guest tombstone store precedes freeing owned storage */
    return 0;
}

RT_SYSLIB("libkernel", scePthreadRwlockInit, rwlock_init);
RT_SYSLIB("libkernel", scePthreadRwlockDestroy, rwlock_destroy);
