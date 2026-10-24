/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Call the supported reader/writer and lifecycle operations through the real registry. */
#include "runtime/syslib.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

typedef int (*init_fn)(void *, const void *, const char *);
typedef int (*slot_fn)(void *);

static void *entry(const char *symbol)
{
    for (const rt_syslib_entry *e = rt_syslib_begin(); e && e != rt_syslib_end(); e++) {
        if (!strcmp(e->library, "libkernel") && !strcmp(e->symbol, symbol)) {
            CHECK(e->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
            return e->fn;
        }
    }
    CHECK(0);
    return NULL;
}

static uint64_t read_slot(const unsigned char *p)
{
    uint64_t value;
    memcpy(&value, p, sizeof value);
    return value;
}

static void guards(const unsigned char *p, size_t n, size_t offset)
{
    for (size_t i = 0; i < n; i++)
        if (i < offset || i >= offset + 8) CHECK(p[i] == 0xa5);
}


enum { WRITER_ROUNDS = 64 };
typedef struct {
    void *slot;
    slot_fn wrlock, unlock;
    _Atomic unsigned ready, go, active, counter, errors;
} writer_pair;

static void update(writer_pair *pair)
{
    if (atomic_fetch_add(&pair->active, 1) != 0) atomic_fetch_add(&pair->errors, 1);
    const unsigned previous = atomic_load(&pair->counter);
    (void)sched_yield(); /* exercise overlap without a timing/fairness assertion */
    atomic_store(&pair->counter, previous + 1);
    if (atomic_fetch_sub(&pair->active, 1) != 1) atomic_fetch_add(&pair->errors, 1);
}

static void write_once(writer_pair *pair)
{
    if (pair->wrlock(pair->slot) != 0) { atomic_fetch_add(&pair->errors, 1); return; }
    update(pair);
    if (pair->unlock(pair->slot) != 0) atomic_fetch_add(&pair->errors, 1);
}

static void *worker(void *arg)
{
    writer_pair *pair = arg;
    atomic_store(&pair->ready, 1);
    while (!atomic_load(&pair->go)) (void)sched_yield();
    for (unsigned i = 0; i < WRITER_ROUNDS; i++) write_once(pair);
    return NULL;
}

static void writer_exclusion(init_fn init, slot_fn destroy, slot_fn wrlock, slot_fn unlock)
{
    _Alignas(uint64_t) unsigned char bytes[23], before[23];
    memset(bytes, 0xa5, sizeof bytes);
    CHECK((uintptr_t)(bytes + 3) % 8 != 0);
    CHECK(init(bytes + 3, NULL, NULL) == 0);
    memcpy(before, bytes, sizeof before);
    writer_pair pair = {.slot = bytes + 3, .wrlock = wrlock, .unlock = unlock};
    atomic_init(&pair.ready, 0); atomic_init(&pair.go, 0); atomic_init(&pair.active, 0);
    atomic_init(&pair.counter, 0); atomic_init(&pair.errors, 0);
    CHECK(wrlock(pair.slot) == 0);
    pthread_t thread;
    const int created = pthread_create(&thread, NULL, worker, &pair);
    if (created) { CHECK(created == 0); CHECK(unlock(pair.slot) == 0); CHECK(destroy(pair.slot) == 0); return; }
    while (!atomic_load(&pair.ready)) (void)sched_yield();
    atomic_store(&pair.go, 1);
    update(&pair); /* main already holds the lock when the worker starts */
    CHECK(unlock(pair.slot) == 0);
    for (unsigned i = 1; i < WRITER_ROUNDS; i++) write_once(&pair);
    const int joined = pthread_join(thread, NULL);
    if (joined) { CHECK(joined == 0); _Exit(2); } /* no stack/object teardown with a possible reader */
    CHECK(atomic_load(&pair.active) == 0 && atomic_load(&pair.errors) == 0);
    CHECK(atomic_load(&pair.counter) == 2 * WRITER_ROUNDS);
    CHECK(!memcmp(bytes, before, sizeof bytes)); /* all lock operations preserve the borrowed slot */
    CHECK(destroy(pair.slot) == 0 && read_slot(bytes + 3) == 1);
    guards(bytes, sizeof bytes, 3);
}

typedef struct {
    void *slot;
    slot_fn rdlock, wrlock, unlock;
    _Atomic unsigned admitted, release_reader, writer_start, writer_done, active_readers, errors;
} reader_group;

static void *holding_reader(void *arg)
{
    reader_group *group = arg;
    if (group->rdlock(group->slot) != 0) {
        atomic_fetch_add(&group->errors, 1);
        atomic_store(&group->admitted, 1);
        return NULL;
    }
    atomic_fetch_add(&group->active_readers, 1);
    atomic_store(&group->admitted, 1);
    while (!atomic_load(&group->release_reader)) (void)sched_yield();
    atomic_fetch_sub(&group->active_readers, 1);
    if (group->unlock(group->slot) != 0) atomic_fetch_add(&group->errors, 1);
    return NULL;
}

static void *waiting_writer(void *arg)
{
    reader_group *group = arg;
    atomic_store(&group->writer_start, 1);
    if (group->wrlock(group->slot) != 0) { atomic_fetch_add(&group->errors, 1); return NULL; }
    if (atomic_load(&group->active_readers) != 0) atomic_fetch_add(&group->errors, 1);
    atomic_store(&group->writer_done, 1);
    if (group->unlock(group->slot) != 0) atomic_fetch_add(&group->errors, 1);
    return NULL;
}

static void joined(pthread_t thread)
{
    const int result = pthread_join(thread, NULL);
    if (result) { CHECK(result == 0); _Exit(2); } /* retain possibly worker-owned stack/object */
}

static void reader_exclusion(init_fn init, slot_fn destroy, slot_fn rdlock, slot_fn wrlock, slot_fn unlock)
{
    _Alignas(uint64_t) unsigned char bytes[23], before[23];
    memset(bytes, 0xa5, sizeof bytes);
    CHECK((uintptr_t)(bytes + 3) % 8 != 0);
    CHECK(init(bytes + 3, NULL, NULL) == 0);
    memcpy(before, bytes, sizeof before);
    reader_group group = {.slot = bytes + 3, .rdlock = rdlock, .wrlock = wrlock, .unlock = unlock};
    atomic_init(&group.admitted, 0); atomic_init(&group.release_reader, 0);
    atomic_init(&group.writer_start, 0); atomic_init(&group.writer_done, 0);
    atomic_init(&group.active_readers, 1); atomic_init(&group.errors, 0);
    CHECK(rdlock(group.slot) == 0);
    pthread_t reader, writer;
    const int read_created = pthread_create(&reader, NULL, holding_reader, &group);
    if (read_created) {
        CHECK(read_created == 0); CHECK(unlock(group.slot) == 0); CHECK(destroy(group.slot) == 0); return;
    }
    while (!atomic_load(&group.admitted)) (void)sched_yield();
    CHECK(atomic_load(&group.active_readers) == 2); /* shared admission before main releases */
    const int write_created = pthread_create(&writer, NULL, waiting_writer, &group);
    if (!write_created) {
        while (!atomic_load(&group.writer_start)) (void)sched_yield();
        CHECK(atomic_load(&group.writer_done) == 0);
    } else CHECK(write_created == 0);
    atomic_fetch_sub(&group.active_readers, 1);
    CHECK(unlock(group.slot) == 0);
    CHECK(atomic_load(&group.writer_done) == 0); /* worker still has its reader hold */
    atomic_store(&group.release_reader, 1);
    joined(reader);
    if (!write_created) joined(writer);
    CHECK(atomic_load(&group.active_readers) == 0 && atomic_load(&group.errors) == 0);
    if (!write_created) CHECK(atomic_load(&group.writer_done) == 1);
    CHECK(!memcmp(bytes, before, sizeof bytes));
    CHECK(destroy(group.slot) == 0 && read_slot(bytes + 3) == 1);
    guards(bytes, sizeof bytes, 3);
}

static void reader_ledger(init_fn init, slot_fn destroy, slot_fn rdlock, slot_fn unlock)
{
    _Alignas(uint64_t) unsigned char a[23], b[27], saved_a[23], saved_b[27];
    memset(a, 0xa5, sizeof a); memset(b, 0xa5, sizeof b);
    CHECK((uintptr_t)(a + 3) % 8 != 0 && (uintptr_t)(b + 5) % 8 != 0);
    CHECK(init(a + 3, NULL, NULL) == 0 && init(b + 5, NULL, NULL) == 0);
    memcpy(saved_a, a, sizeof a); memcpy(saved_b, b, sizeof b);
    CHECK(rdlock(a + 3) == 0 && rdlock(b + 5) == 0);
    CHECK(unlock(a + 3) == 0); /* remove a non-head ledger node */
    CHECK(rdlock(a + 3) == 0); /* balanced re-acquisition is not recursion */
    CHECK(unlock(b + 5) == 0 && unlock(a + 3) == 0);
    CHECK(!memcmp(a, saved_a, sizeof a) && !memcmp(b, saved_b, sizeof b));
    CHECK(destroy(a + 3) == 0 && destroy(b + 5) == 0);
    guards(a, sizeof a, 3); guards(b, sizeof b, 5);
}

int main(void)
{
    const init_fn init = (init_fn)entry("scePthreadRwlockInit");
    const slot_fn destroy = (slot_fn)entry("scePthreadRwlockDestroy");
    const slot_fn rdlock = (slot_fn)entry("scePthreadRwlockRdlock");
    const slot_fn wrlock = (slot_fn)entry("scePthreadRwlockWrlock");
    const slot_fn unlock = (slot_fn)entry("scePthreadRwlockUnlock");
    if (failures) return 1;
    for (unsigned round = 0; round < 3; round++) {
        _Alignas(uint64_t) unsigned char a[23], b[27];
        memset(a, 0xa5, sizeof a);
        memset(b, 0xa5, sizeof b);
        /* Both slots are deliberately unaligned and initially nonzero poison. */
        CHECK((uintptr_t)(a + 3) % 8 != 0 && (uintptr_t)(b + 5) % 8 != 0);
        CHECK(init(a + 3, NULL, NULL) == 0);
        CHECK(init(b + 5, NULL, NULL) == 0);
        const uint64_t first = read_slot(a + 3), second = read_slot(b + 5);
        CHECK(first != 0 && first != 1 && second != 0 && second != 1 && first != second);
        guards(a, sizeof a, 3);
        guards(b, sizeof b, 5);
        CHECK(wrlock(a + 3) == 0 && wrlock(b + 5) == 0);
        CHECK(unlock(b + 5) == 0 && unlock(a + 3) == 0);
        CHECK(read_slot(a + 3) == first && read_slot(b + 5) == second);
        CHECK(destroy(a + 3) == 0 && read_slot(a + 3) == 1);
        CHECK(read_slot(b + 5) == second);
        CHECK(destroy(b + 5) == 0 && read_slot(b + 5) == 1);
        guards(a, sizeof a, 3);
        guards(b, sizeof b, 5);
    }
    _Alignas(uint64_t) unsigned char empty[19], before[19];
    memset(empty, 0xa5, sizeof empty);
    memset(empty + 7, 0, 8);
    memcpy(before, empty, sizeof before);
    CHECK((uintptr_t)(empty + 7) % 8 != 0);
    CHECK(destroy(empty + 7) == 0 && !memcmp(empty, before, sizeof empty));
    writer_exclusion(init, destroy, wrlock, unlock);
    reader_ledger(init, destroy, rdlock, unlock);
    reader_exclusion(init, destroy, rdlock, wrlock, unlock);
    if (!failures) puts("rwlock lifecycle: ok");
    return failures != 0;
}
