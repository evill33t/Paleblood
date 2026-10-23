/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Real registry, concurrent allocation and borrowed-slot checks; active in Release. */
#include "runtime/syslib.h"
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t (*create_fn)(void *, const char *, uint32_t, int32_t, int32_t, const void *);
typedef int32_t (*delete_fn)(uint64_t);
static create_fn create_semaphore;
static delete_fn delete_semaphore;
static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

enum { WORKERS = 4, PER_WORKER = 32 };
typedef struct {
    _Alignas(uint64_t) unsigned char slots[PER_WORKER][24];
    uint64_t ids[PER_WORKER];
    int failed;
} worker_state;

static void *lookup(const char *symbol)
{
    for (const rt_syslib_entry *e = rt_syslib_begin(); e && e != rt_syslib_end(); e++)
        if (!strcmp(e->library, "libkernel") && !strcmp(e->symbol, symbol)) {
            CHECK(e->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
            return e->fn;
        }
    CHECK(0);
    return NULL;
}

static uint64_t value(const void *p)
{
    uint64_t result;
    memcpy(&result, p, sizeof result);
    return result;
}

static void *worker(void *opaque)
{
    worker_state *state = opaque;
    memset(state->slots, 0xa5, sizeof state->slots);
    for (unsigned i = 0; i < PER_WORKER; i++) {
        unsigned char *slot = state->slots[i] + 3;
        if ((uintptr_t)slot % 8 == 0 || create_semaphore(slot, "", 2, 0, 1, NULL)) state->failed = 1;
        state->ids[i] = value(slot);
        if (!state->ids[i] || state->ids[i] > INT32_MAX) state->failed = 1;
        for (size_t j = 0; j < sizeof state->slots[i]; j++)
            if ((j < 3 || j >= 11) && state->slots[i][j] != 0xa5) state->failed = 1;
        if (i && state->ids[i] <= state->ids[i - 1]) state->failed = 1;
    }
    return NULL;
}

int main(void)
{
    create_semaphore = (create_fn)lookup("sceKernelCreateSema");
    delete_semaphore = (delete_fn)lookup("sceKernelDeleteSema");
    if (failures) return 1;
    /* Options take precedence over every otherwise-invalid argument. */
    CHECK(create_semaphore((void *)(uintptr_t)1, (const char *)(uintptr_t)1,
                           UINT32_MAX, INT32_MIN, INT32_MIN, (const void *)(uintptr_t)1)
          == (int32_t)UINT32_C(0x80020016));
    worker_state states[WORKERS] = {0};
    pthread_t threads[WORKERS];
    unsigned started = 0;
    for (; started < WORKERS; started++) {
        const int result = pthread_create(&threads[started], NULL, worker, &states[started]);
        CHECK(result == 0);
        if (result) break;
    }
    for (unsigned i = 0; i < started; i++) {
        const int result = pthread_join(threads[i], NULL);
        CHECK(result == 0);
        if (result) _Exit(2); /* a live worker may still own the stack-backed state */
    }
    uint64_t greatest = 0;
    for (unsigned w = 0; w < started; w++) {
        CHECK(!states[w].failed);
        for (unsigned i = 0; i < PER_WORKER; i++) {
            const uint64_t id = states[w].ids[i];
            if (id > greatest) greatest = id;
            for (unsigned x = 0; x <= w; x++)
                for (unsigned j = 0; j < (x == w ? i : PER_WORKER); j++) CHECK(id != states[x].ids[j]);
            unsigned char saved[24];
            memcpy(saved, states[w].slots[i], sizeof saved);
            CHECK(delete_semaphore(id) == 0);
            CHECK(!memcmp(saved, states[w].slots[i], sizeof saved)); /* by-value, no slot mutation */
        }
    }
    _Alignas(uint64_t) unsigned char last[19], before[19];
    memset(last, 0x5a, sizeof last);
    memcpy(before, last, sizeof before);
    CHECK((uintptr_t)(last + 5) % 8 != 0);
    CHECK(create_semaphore(last + 5, "", 2, 0, 1, NULL) == 0);
    CHECK(value(last + 5) > greatest); /* freed IDs are not reused */
    CHECK(delete_semaphore(value(last + 5)) == 0);
    for (size_t i = 0; i < sizeof last; i++) if (i < 5 || i >= 13) CHECK(last[i] == before[i]);
    if (!failures) puts("semaphore creation/deletion registry: ok");
    return failures != 0;
}
