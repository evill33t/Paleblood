/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Default, unnamed rwlock lifecycle and nonrecursive own-reader/writer operations. The guest owns
   an eight-byte pointer slot;
   the allocated host object is private and does not reproduce a console object layout. */
#include "runtime/process.h"
#include "runtime/syslib.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(void *) == 8, "guest rwlock handles require 64-bit pointers");

typedef struct {
    pthread_rwlock_t host;
    _Atomic(uintptr_t) writer;
    atomic_size_t readers;
} guest_rwlock;

/* Host-thread identity only: no guest TLS layout, pthread_t encoding or GS/FS changes.
   A thread must unlock before exit; calls are not reentrant on the same host thread. */
static _Thread_local unsigned char writer_identity;

typedef struct reader_hold {
    guest_rwlock *lock;
    struct reader_hold *next;
} reader_hold;

/* Host-thread ledger only, one node per balanced nonrecursive read acquisition.
   It is not the guest TCB's aggregate read count or the console object layout. */
static _Thread_local reader_hold *reader_holds;

static reader_hold **reader_record(guest_rwlock *lock)
{
    reader_hold **link = &reader_holds;
    while (*link && (*link)->lock != lock) link = &(*link)->next;
    return link;
}

static guest_rwlock *initialized_lock(void *slot, const char *caller)
{
    uint64_t handle;
    memcpy(&handle, slot, sizeof handle);
    if (!handle) rt_fatal(caller, "lazy initialization is not implemented");
    if (handle == 1) rt_fatal(caller, "destroyed-handle diagnostic behavior is not implemented");
    return (guest_rwlock *)(uintptr_t)handle;
}

static int rwlock_rdlock(void *slot)
{
    guest_rwlock *lock = initialized_lock(slot, "scePthreadRwlockRdlock");
    const uintptr_t self = (uintptr_t)&writer_identity;
    if (atomic_load(&lock->writer) == self)
        rt_fatal("scePthreadRwlockRdlock", "own write-to-read conversion is not implemented");
    if (*reader_record(lock))
        rt_fatal("scePthreadRwlockRdlock", "recursive read locking is not implemented");
    reader_hold *record = calloc(1, sizeof *record);
    if (!record)
        rt_fatal("scePthreadRwlockRdlock", "reader record allocation failed; error translation is not established");
    record->lock = lock;
    /* No signal mask or metadata mutex spans this blocking host operation. A process-
       terminal nonlocal exit can abandon the candidate/host lock; continuation is unsupported. */
    if (pthread_rwlock_rdlock(&lock->host) != 0) {
        free(record);
        rt_fatal("scePthreadRwlockRdlock", "host read locking failed; error translation is not established");
    }
    record->next = reader_holds;
    reader_holds = record;
    atomic_fetch_add(&lock->readers, 1);
    return 0;
}

static int rwlock_wrlock(void *slot)
{
    guest_rwlock *lock = initialized_lock(slot, "scePthreadRwlockWrlock");
    const uintptr_t self = (uintptr_t)&writer_identity;
    if (atomic_load(&lock->writer) == self)
        rt_fatal("scePthreadRwlockWrlock", "recursive write locking is not implemented");
    if (*reader_record(lock))
        rt_fatal("scePthreadRwlockWrlock", "own read-to-write conversion is not implemented");
    /* Host blocking/contention behavior is port policy; no console fairness is claimed. */
    if (pthread_rwlock_wrlock(&lock->host) != 0)
        rt_fatal("scePthreadRwlockWrlock", "host write locking failed; error translation is not established");
    atomic_store(&lock->writer, self);
    return 0;
}

static int rwlock_unlock(void *slot)
{
    guest_rwlock *lock = initialized_lock(slot, "scePthreadRwlockUnlock");
    const uintptr_t self = (uintptr_t)&writer_identity;
    if (atomic_load(&lock->writer) != self) {
        reader_hold **link = reader_record(lock);
        if (!*link)
            rt_fatal("scePthreadRwlockUnlock", "only the recorded writer or this thread's reader may unlock");
        reader_hold *record = *link;
        if (pthread_rwlock_unlock(&lock->host) != 0)
            rt_fatal("scePthreadRwlockUnlock", "host reader unlocking failed; ownership retained, error translation is not established");
        *link = record->next;
        atomic_fetch_sub(&lock->readers, 1);
        free(record);
        /* Never clear writer here: a waiting writer may already own the host lock. */
        return 0;
    }
    if (pthread_rwlock_unlock(&lock->host) != 0)
        rt_fatal("scePthreadRwlockUnlock", "host unlocking failed; error translation is not established");
    /* A next writer may have acquired and published its token before this thread resumes.
       Clear only our old token, never that next writer's record. */
    uintptr_t expected = self;
    (void)atomic_compare_exchange_strong(&lock->writer, &expected, 0);
    return 0;
}

static int rwlock_init(void *slot, const void *attr, const char *name)
{
    if (attr)
        rt_fatal("scePthreadRwlockInit", "non-NULL attributes are not implemented");
    if (name)
        rt_fatal("scePthreadRwlockInit", "named rwlocks are not implemented");

    guest_rwlock *lock = calloc(1, sizeof *lock);
    if (!lock) return (int)UINT32_C(0x8002000c);
    atomic_init(&lock->writer, 0);
    atomic_init(&lock->readers, 0);
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
    if (atomic_load(&lock->writer) != 0 || atomic_load(&lock->readers) != 0)
        rt_fatal("scePthreadRwlockDestroy", "destruction requires a quiescent object with no writer or readers");
    if (pthread_rwlock_destroy(&lock->host) != 0)
        rt_fatal("scePthreadRwlockDestroy", "host destruction failed; handle retained, error translation is not established");
    const uint64_t destroyed = 1;
    memcpy(slot, &destroyed, sizeof destroyed);
    free(lock); /* the successful guest tombstone store precedes freeing owned storage */
    return 0;
}

RT_SYSLIB("libkernel", scePthreadRwlockInit, rwlock_init);
RT_SYSLIB("libkernel", scePthreadRwlockDestroy, rwlock_destroy);

RT_SYSLIB("libkernel", scePthreadRwlockRdlock, rwlock_rdlock);
RT_SYSLIB("libkernel", scePthreadRwlockWrlock, rwlock_wrlock);
RT_SYSLIB("libkernel", scePthreadRwlockUnlock, rwlock_unlock);
