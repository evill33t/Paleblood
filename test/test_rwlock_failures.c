/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Test-only aliases exercise allocation and host failures without changing production APIs. */
#include "runtime/process.h"
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *test_calloc(size_t, size_t);
static void test_free(void *);
static int test_host_init(pthread_rwlock_t *, const pthread_rwlockattr_t *);
static int test_host_destroy(pthread_rwlock_t *);
static int test_host_wrlock(pthread_rwlock_t *);
static int test_host_rdlock(pthread_rwlock_t *);
static int test_host_unlock(pthread_rwlock_t *);
static _Noreturn void test_fatal(const char *, const char *);
#define calloc test_calloc
#define free test_free
#define pthread_rwlock_init test_host_init
#define pthread_rwlock_destroy test_host_destroy
#define pthread_rwlock_wrlock test_host_wrlock
#define pthread_rwlock_rdlock test_host_rdlock
#define pthread_rwlock_unlock test_host_unlock
#define rt_fatal test_fatal
#include "../runtime/syslib/kernel_rwlock.c"
#undef calloc
#undef free
#undef pthread_rwlock_init
#undef pthread_rwlock_destroy
#undef pthread_rwlock_wrlock
#undef pthread_rwlock_rdlock
#undef pthread_rwlock_unlock
#undef rt_fatal

static int failures, allocations, frees, init_calls, destroy_calls, wrlock_calls, unlock_calls;
static int fail_allocate, init_error, destroy_error, wrlock_error, unlock_error;
static uintptr_t next_writer;
static int record_allocations, record_frees, fail_record_allocate, rdlock_calls, rdlock_error, next_reader;
static void *owned_record;
static void *owned;
static unsigned char *observed_slot;
static uint64_t expected_slot_at_free;
static jmp_buf fatal_context;
static const char *fatal_name, *fatal_message;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define FATAL(call, name) do { fatal_name = NULL; fatal_message = NULL; if (!setjmp(fatal_context)) { (void)(call); CHECK(0); } CHECK(fatal_name && !strcmp(fatal_name, name)); CHECK(fatal_message && *fatal_message); } while (0)

static uint64_t value(const void *p)
{
    uint64_t n;
    memcpy(&n, p, sizeof n);
    return n;
}

static void *test_calloc(size_t count, size_t size)
{
    if (size == sizeof(reader_hold)) {
        record_allocations++;
        CHECK(count == 1 && owned && !owned_record);
        if (fail_record_allocate) return NULL;
        owned_record = calloc(count, size);
        CHECK(owned_record != NULL);
        return owned_record;
    }
    allocations++;
    CHECK(count == 1 && size == sizeof(guest_rwlock) && !owned);
    if (fail_allocate) return NULL;
    owned = calloc(count, size);
    CHECK(owned != NULL);
    return owned;
}

static void test_free(void *p)
{
    if (p == owned_record && owned_record) {
        CHECK(!reader_holds && value(observed_slot) == (uint64_t)(uintptr_t)owned);
        record_frees++;
        free(p);
        owned_record = NULL;
        return;
    }
    CHECK(p == owned && owned != NULL);
    if (observed_slot) CHECK(value(observed_slot) == expected_slot_at_free);
    frees++;
    free(p);
    owned = NULL;
}

static int test_host_init(pthread_rwlock_t *p, const pthread_rwlockattr_t *attr)
{
    CHECK(p == &((guest_rwlock *)owned)->host && attr == NULL);
    CHECK(atomic_load(&((guest_rwlock *)owned)->writer) == 0);
    CHECK(atomic_load(&((guest_rwlock *)owned)->readers) == 0);
    init_calls++;
    return init_error;
}

static int test_host_destroy(pthread_rwlock_t *p)
{
    CHECK(p == &((guest_rwlock *)owned)->host);
    CHECK(atomic_load(&((guest_rwlock *)owned)->readers) == 0);
    destroy_calls++;
    /* Host cleanup runs while the caller can still reach the live allocation. */
    CHECK(observed_slot && value(observed_slot) == (uint64_t)(uintptr_t)owned);
    return destroy_error;
}

static int test_host_wrlock(pthread_rwlock_t *p)
{
    CHECK(p == &((guest_rwlock *)owned)->host);
    CHECK(atomic_load(&((guest_rwlock *)owned)->writer) == 0);
    wrlock_calls++;
    return wrlock_error;
}

static int test_host_rdlock(pthread_rwlock_t *p)
{
    CHECK(p == &((guest_rwlock *)owned)->host && owned_record);
    CHECK(atomic_load(&((guest_rwlock *)owned)->writer) != (uintptr_t)&writer_identity);
    CHECK(!*reader_record(owned));
    rdlock_calls++;
    return rdlock_error;
}

static int test_host_unlock(pthread_rwlock_t *p)
{
    CHECK(p == &((guest_rwlock *)owned)->host);
    CHECK(atomic_load(&((guest_rwlock *)owned)->writer) == (uintptr_t)&writer_identity
          || *reader_record(owned));
    CHECK(value(observed_slot) == (uint64_t)(uintptr_t)owned);
    unlock_calls++;
    if (next_writer) atomic_store(&((guest_rwlock *)owned)->writer, next_writer);
    if (next_reader) atomic_fetch_add(&((guest_rwlock *)owned)->readers, 1);
    return unlock_error;
}

static _Noreturn void test_fatal(const char *name, const char *message)
{
    fatal_name = name;
    fatal_message = message;
    longjmp(fatal_context, 1);
}

static void reader_failures(guest_rwlock *lock)
{
    const int before_unlock = unlock_calls;
    fail_record_allocate = 1;
    FATAL(rwlock_rdlock(observed_slot), "scePthreadRwlockRdlock");
    CHECK(record_allocations == 1 && !rdlock_calls && !owned_record && !reader_holds);
    CHECK(atomic_load(&lock->readers) == 0 && !record_frees);
    fail_record_allocate = 0;
    rdlock_error = 12345;
    FATAL(rwlock_rdlock(observed_slot), "scePthreadRwlockRdlock");
    CHECK(rdlock_calls == 1 && record_allocations == 2 && record_frees == 1);
    CHECK(!owned_record && !reader_holds && atomic_load(&lock->readers) == 0);
    rdlock_error = 0;
    CHECK(rwlock_rdlock(observed_slot) == 0 && atomic_load(&lock->readers) == 1);
    reader_hold *record = reader_holds;
    CHECK(record && record == owned_record && record->lock == lock && !record->next);
    FATAL(rwlock_rdlock(observed_slot), "scePthreadRwlockRdlock");
    _Alignas(uint64_t) unsigned char alias_slot[8];
    const uint64_t handle = (uint64_t)(uintptr_t)lock;
    memcpy(alias_slot, &handle, sizeof handle);
    FATAL(rwlock_rdlock(alias_slot), "scePthreadRwlockRdlock"); /* key is object, not slot */
    FATAL(rwlock_wrlock(observed_slot), "scePthreadRwlockWrlock");
    FATAL(rwlock_destroy(observed_slot), "scePthreadRwlockDestroy");
    CHECK(rdlock_calls == 2 && record_allocations == 3 && wrlock_calls == 3 && !destroy_calls);

    reader_holds = NULL; /* inert other-thread ledger view: same total, no own node */
    FATAL(rwlock_unlock(observed_slot), "scePthreadRwlockUnlock");
    CHECK(unlock_calls == before_unlock && atomic_load(&lock->readers) == 1 && owned_record == record);
    reader_holds = record;
    unlock_error = 12345;
    FATAL(rwlock_unlock(observed_slot), "scePthreadRwlockUnlock");
    CHECK(reader_holds == record && owned_record == record && atomic_load(&lock->readers) == 1);
    CHECK(unlock_calls == before_unlock + 1 && record_frees == 1);
    unlock_error = 0;
    CHECK(rwlock_unlock(observed_slot) == 0 && !reader_holds && !owned_record);
    CHECK(atomic_load(&lock->readers) == 0 && record_frees == 2);

    CHECK(rwlock_rdlock(observed_slot) == 0);
    next_writer = 1; /* a waiting writer publishes before reader cleanup resumes */
    CHECK(rwlock_unlock(observed_slot) == 0 && atomic_load(&lock->writer) == next_writer);
    CHECK(atomic_load(&lock->readers) == 0 && !reader_holds && !owned_record);
    next_writer = 0;
    atomic_store(&lock->writer, 0); /* injected other writer has completed */

    CHECK(rwlock_rdlock(observed_slot) == 0);
    next_reader = 1; /* separate interleaving: another thread completes a read */
    CHECK(rwlock_unlock(observed_slot) == 0 && atomic_load(&lock->readers) == 1);
    CHECK(!reader_holds && !owned_record && atomic_load(&lock->writer) == 0);
    next_reader = 0;
    atomic_store(&lock->readers, 0); /* injected other reader has completed */
    CHECK(rdlock_calls == 4 && record_allocations == 5 && record_frees == 4);
    CHECK(unlock_calls == before_unlock + 4);
}

int main(void)
{
    _Alignas(uint64_t) unsigned char bytes[23], saved[23];
    memset(bytes, 0xa5, sizeof bytes);
    memcpy(saved, bytes, sizeof saved);
    observed_slot = bytes + 3;
    CHECK((uintptr_t)observed_slot % 8 != 0);
    fail_allocate = 1;
    CHECK(rwlock_init(observed_slot, NULL, NULL) == (int)UINT32_C(0x8002000c));
    CHECK(!memcmp(bytes, saved, sizeof bytes) && !owned && !init_calls && !frees);
    CHECK(rwlock_init(NULL, NULL, NULL) == (int)UINT32_C(0x8002000c));
    CHECK(allocations == 2 && !init_calls && !frees);

    /* Unsupported pointers are rejected by identity without dereferencing them. */
    FATAL(rwlock_init(observed_slot, (const void *)(uintptr_t)1, NULL), "scePthreadRwlockInit");
    FATAL(rwlock_init(observed_slot, NULL, (const char *)(uintptr_t)1), "scePthreadRwlockInit");
    CHECK(allocations == 2 && !memcmp(bytes, saved, sizeof bytes));

    fail_allocate = 0;
    init_error = 12345; /* intentionally not translated into a guest errno */
    expected_slot_at_free = value(observed_slot);
    FATAL(rwlock_init(observed_slot, NULL, NULL), "scePthreadRwlockInit");
    CHECK(!owned && frees == 1 && init_calls == 1 && !memcmp(bytes, saved, sizeof bytes));

    init_error = 0;
    CHECK(rwlock_init(observed_slot, NULL, NULL) == 0);
    const uint64_t handle = (uint64_t)(uintptr_t)owned;
    CHECK(handle && handle != 1 && value(observed_slot) == handle);
    for (size_t i = 0; i < sizeof bytes; i++) if (i < 3 || i >= 11) CHECK(bytes[i] == 0xa5);
    memcpy(saved, bytes, sizeof saved);
    guest_rwlock *lock = owned;
    const uintptr_t self = (uintptr_t)&writer_identity;
    wrlock_error = 12345;
    FATAL(rwlock_wrlock(observed_slot), "scePthreadRwlockWrlock");
    CHECK(atomic_load(&lock->writer) == 0 && wrlock_calls == 1 && !unlock_calls);
    CHECK(!memcmp(bytes, saved, sizeof bytes) && frees == 1);
    wrlock_error = 0;
    CHECK(rwlock_wrlock(observed_slot) == 0 && atomic_load(&lock->writer) == self);
    FATAL(rwlock_rdlock(observed_slot), "scePthreadRwlockRdlock");
    CHECK(!rdlock_calls && !record_allocations && !reader_holds);
    FATAL(rwlock_wrlock(observed_slot), "scePthreadRwlockWrlock");
    FATAL(rwlock_destroy(observed_slot), "scePthreadRwlockDestroy");
    CHECK(wrlock_calls == 2 && !destroy_calls && frees == 1);
    atomic_store(&lock->writer, 1); /* injected different-owner record, not a guest handle */
    FATAL(rwlock_unlock(observed_slot), "scePthreadRwlockUnlock");
    CHECK(!unlock_calls && atomic_load(&lock->writer) == 1);
    atomic_store(&lock->writer, self);
    unlock_error = 12345;
    FATAL(rwlock_unlock(observed_slot), "scePthreadRwlockUnlock");
    CHECK(unlock_calls == 1 && atomic_load(&lock->writer) == self);
    CHECK(!memcmp(bytes, saved, sizeof bytes) && frees == 1);
    unlock_error = 0;
    CHECK(rwlock_unlock(observed_slot) == 0 && atomic_load(&lock->writer) == 0);
    FATAL(rwlock_unlock(observed_slot), "scePthreadRwlockUnlock");
    CHECK(unlock_calls == 2);

    /* Inert scheduling regression: a successful host unlock can let the next writer
       publish before the old thread resumes. Its record must survive the old CAS. */
    CHECK(rwlock_wrlock(observed_slot) == 0);
    next_writer = 1;
    CHECK(rwlock_unlock(observed_slot) == 0 && atomic_load(&lock->writer) == next_writer);
    next_writer = 0;
    atomic_store(&lock->writer, 0); /* end of the injected interleaving fixture */
    CHECK(!memcmp(bytes, saved, sizeof bytes) && wrlock_calls == 3 && unlock_calls == 3);
    reader_failures(lock);
    CHECK(!memcmp(bytes, saved, sizeof bytes) && !reader_holds && !owned_record);

    destroy_error = 12345;
    FATAL(rwlock_destroy(observed_slot), "scePthreadRwlockDestroy");
    CHECK(owned && value(observed_slot) == handle && frees == 1 && destroy_calls == 1);
    CHECK(!memcmp(bytes, saved, sizeof bytes));

    destroy_error = 0;
    expected_slot_at_free = 1;
    CHECK(rwlock_destroy(observed_slot) == 0);
    CHECK(!owned && value(observed_slot) == 1 && frees == 2 && destroy_calls == 2);
    memcpy(saved, bytes, sizeof saved);
    FATAL(rwlock_destroy(observed_slot), "scePthreadRwlockDestroy");
    CHECK(!memcmp(bytes, saved, sizeof bytes) && !owned && frees == 2 && destroy_calls == 2);
    FATAL(rwlock_rdlock(observed_slot), "scePthreadRwlockRdlock");
    FATAL(rwlock_wrlock(observed_slot), "scePthreadRwlockWrlock");
    FATAL(rwlock_unlock(observed_slot), "scePthreadRwlockUnlock");

    const uint64_t zero = 0;
    memcpy(observed_slot, &zero, sizeof zero);
    memcpy(saved, bytes, sizeof saved);
    FATAL(rwlock_rdlock(observed_slot), "scePthreadRwlockRdlock");
    FATAL(rwlock_wrlock(observed_slot), "scePthreadRwlockWrlock");
    FATAL(rwlock_unlock(observed_slot), "scePthreadRwlockUnlock");
    CHECK(rwlock_destroy(observed_slot) == 0 && !memcmp(bytes, saved, sizeof bytes));
    CHECK(!owned && frees == 2 && destroy_calls == 2);
    CHECK(allocations == 4 && wrlock_calls == 3 && unlock_calls == 7);
    CHECK(record_allocations == 5 && record_frees == 4 && rdlock_calls == 4 && !owned_record && !reader_holds);
    if (!failures) puts("rwlock failure ownership: ok");
    return failures != 0;
}
