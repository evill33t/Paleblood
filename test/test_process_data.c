/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DEFAULT_SOURCE
/* Compile the production source with only its host entropy boundary replaced.
   There is no runtime injection/reset switch in the shipped provider. */
#define getentropy test_entropy
#include "../runtime/syslib/kernel_process_data.c"
#undef getentropy
#include <pthread.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); abort(); } } while (0)
static int entropy_mode, entropy_calls, entered, release_entropy;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
int test_entropy(void *out, size_t size)
{
    CHECK(size == 64);
    entropy_calls++;
    if (entropy_mode == 0) { memset(out, 0xa5, size / 2); return -1; }
    memset(out, 0, size);
    if (entropy_mode == 1) return 0;
    CHECK(pthread_mutex_lock(&lock) == 0);
    entered = 1;
    CHECK(pthread_cond_broadcast(&changed) == 0);
    while (!release_entropy) CHECK(pthread_cond_wait(&changed, &lock) == 0);
    CHECK(pthread_mutex_unlock(&lock) == 0);
    for (size_t i = 0; i < size; i++) ((unsigned char *)out)[i] = (unsigned char)(i + 1);
    return 0;
}
static const rt_syslib_data *record(const char *name, size_t size)
{
    const rt_syslib_data *found = NULL;
    for (const rt_syslib_data *d = rt_syslib_data_begin(); d != rt_syslib_data_end(); d++) {
        if (!d->library) continue;
        if (strcmp(d->symbol, name)) continue;
        CHECK(!found && !strcmp(d->library, "libkernel"));
        CHECK(d->address && d->size == size && d->ready == rt_process_data_ready);
        found = d;
    }
    CHECK(found);
    return found;
}
static size_t argument_count = 2;
static char name[64] = "/owned/path/game.bin", second[] = "guest arg", environment[] = "OWNED=only";
static const char *args[] = {name, second, NULL};
static const char *envs[] = {environment, "SECOND=value", NULL};
static void *prepare(void *unused)
{
    (void)unused;
    CHECK(rt_process_data_prepare(argument_count, argument_count ? args : NULL, 2, envs) == RT_PROCESS_DATA_OK);
    return NULL;
}
int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "empty")) argument_count = 0;
    if (argc > 1 && !strcmp(argv[1], "trailing")) strcpy(name, "/owned/path/");
    if (argc > 1 && !strcmp(argv[1], "backslash")) strcpy(name, "path\\game.bin");
    const rt_syslib_data *g = record("__stack_chk_guard", 64), *e = record("environ", 8);
    const rt_syslib_data *p = record("__progname", 8), *s = record("_sigintr", 16);
    CHECK(!g->ready() && !rt_process_data_parameters());
    CHECK((uintptr_t)g->address % 16 == 0);
    CHECK(rt_process_data_prepare(65, NULL, 0, NULL) == RT_PROCESS_DATA_INVALID);
    CHECK(rt_process_data_prepare(0, NULL, 129, NULL) == RT_PROCESS_DATA_INVALID);
    CHECK(rt_process_data_prepare(1, NULL, 0, NULL) == RT_PROCESS_DATA_INVALID);
    CHECK(rt_process_data_prepare(0, args, 0, NULL) == RT_PROCESS_DATA_INVALID);
    CHECK(rt_process_data_prepare(1, args, 0, NULL) == RT_PROCESS_DATA_INVALID); /* wrong terminator */
    const char *missing[] = {NULL, NULL};
    CHECK(rt_process_data_prepare(1, missing, 0, NULL) == RT_PROCESS_DATA_INVALID);
    char *too_long = malloc(RT_PROCESS_STRING_BYTES + 1);
    CHECK(too_long);
    memset(too_long, 'a', RT_PROCESS_STRING_BYTES);
    too_long[RT_PROCESS_STRING_BYTES] = 0;
    const char *oversize[] = {too_long, NULL};
    CHECK(rt_process_data_prepare(1, oversize, 0, NULL) == RT_PROCESS_DATA_INVALID);
    free(too_long);
    CHECK(entropy_calls == 0);
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_ENTROPY);
    entropy_mode = 1;
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_ENTROPY);
    CHECK(entropy_calls == 2 && !g->ready() && !rt_process_data_parameters());
    for (size_t i = 0; i < 64; i++) CHECK(!((unsigned char *)g->address)[i]);
    CHECK(!*(void **)e->address && !*(void **)p->address);
    entropy_mode = 2;
    pthread_t worker;
    CHECK(pthread_create(&worker, NULL, prepare, NULL) == 0);
    CHECK(pthread_mutex_lock(&lock) == 0);
    while (!entered) CHECK(pthread_cond_wait(&changed, &lock) == 0);
    CHECK(!g->ready() && !rt_process_data_parameters());
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_BUSY);
    release_entropy = 1;
    CHECK(pthread_cond_broadcast(&changed) == 0);
    CHECK(pthread_mutex_unlock(&lock) == 0);
    CHECK(pthread_join(worker, NULL) == 0);
    CHECK(entropy_calls == 3 && g->ready() && e->ready() && p->ready() && s->ready());
    uint64_t *raw = rt_process_data_parameters();
    CHECK(raw && raw[0] == argument_count);
    char **copied = (char **)(raw + 1);
    CHECK(copied[argument_count] == NULL);
    char **environment_copy = *(char ***)e->address;
    CHECK(environment_copy == copied + argument_count + 1);
    CHECK(environment_copy[2] == NULL && environment_copy[0] != environment);
    CHECK(!strcmp(environment_copy[0], environment) && !strcmp(environment_copy[1], envs[1]));
    environment[0] = 'X';
    CHECK(environment_copy[0][0] == 'O');
    environment_copy[0][0] = 'Y';
    CHECK(environment[0] == 'X');
    if (argument_count) {
        CHECK(copied[0] != name && copied[1] != second);
        CHECK(!strcmp(copied[0], name) && !strcmp(copied[1], second));
        char *base = strrchr(copied[0], '/');
        CHECK(*(char **)p->address == (base ? base + 1 : copied[0]));
        if (argc > 1 && !strcmp(argv[1], "trailing")) CHECK(!**(char **)p->address);
        if (argc > 1 && !strcmp(argv[1], "backslash")) CHECK(*(char **)p->address == copied[0]);
        name[0] = 'Z';
        CHECK(copied[0][0] != 'Z');
        copied[1][0] = 'G'; CHECK(second[0] == 'g');
    } else CHECK(!*(char **)p->address);
    for (size_t i = 0; i < 64; i++) CHECK(((unsigned char *)g->address)[i] == (unsigned char)(i + 1));
    for (size_t i = 0; i < 4; i++) CHECK(!((uint32_t *)s->address)[i]);
    ((uint32_t *)s->address)[3] = UINT32_C(0x80000000);
    ((unsigned char *)g->address)[63] = 0xa5;
    CHECK(rt_process_data_prepare(0, NULL, 0, NULL) == RT_PROCESS_DATA_ALREADY_PREPARED);
    CHECK(entropy_calls == 3 && rt_process_data_parameters() == raw);
    CHECK(((uint32_t *)s->address)[3] == UINT32_C(0x80000000));
    CHECK(((unsigned char *)g->address)[63] == 0xa5);
    puts("owned process data: publication, copies, aliases, guard and lifetime passed");
    return 0;
}
