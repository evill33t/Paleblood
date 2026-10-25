/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "runtime/module.h"
#include "runtime/syslib.h"
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); abort(); } } while (0)

typedef int32_t (*load_fn)(const char *, uint64_t, const void *, uint32_t, const void *, void *);
static load_fn load;
static jmp_buf recovery;
static int fatal_expected;
static const char *fatal_message;
static unsigned char trace[128];
static size_t trace_size;
static rt_modules *catalogue;
static void *wanted_override;

_Noreturn void rt_fatal(const char *function, const char *message)
{
    CHECK(fatal_expected && !strcmp(function, "sceKernelLoadStartModuleForSysmodule"));
    CHECK(strstr(message, fatal_message));
    longjmp(recovery, 1);
}
static int32_t entry_a(uint64_t size, const void *arguments, void *override)
{
    CHECK(!size && !arguments && override == wanted_override);
    trace[trace_size++] = 'A';
    return 0;
}
static int32_t entry_b(uint64_t size, const void *arguments, void *override)
{
    CHECK(!size && !arguments && !override);
    trace[trace_size++] = 'B';
    return 7;
}
static int32_t entry_c(uint64_t size, const void *arguments, void *override)
{
    CHECK(!size && !arguments && !override);
    trace[trace_size++] = 'C';
    return 0;
}
static int32_t entry_d(uint64_t size, const void *arguments, void *override)
{
    CHECK(!size && !arguments && !override);
    trace[trace_size++] = 'D';
    return 0;
}
static int32_t negative(uint64_t size, const void *arguments, void *override)
{
    CHECK(!size && !arguments && !override);
    trace[trace_size++] = 'N';
    return -123;
}
static int32_t reentrant(uint64_t size, const void *arguments, void *override)
{
    CHECK(!size && !arguments && !override);
    int32_t handle = 0x1234, result = 0x5678;
    CHECK(rt_modules_load("/B", &result, &handle) == RT_MODULE_REENTRANT);
    CHECK(handle == 0x1234 && result == 0x5678);
    trace[trace_size++] = 'R';
    return 0;
}
static int32_t abandoned(uint64_t size, const void *arguments, void *override)
{
    CHECK(!size && !arguments && !override);
    trace[trace_size++] = 'X';
    longjmp(recovery, 1);
}
static void expect_stop(const char *path, const char *message)
{
    fatal_expected = 1;
    fatal_message = message;
    if (!setjmp(recovery)) { load(path, 0, NULL, 0, NULL, NULL); CHECK(0); }
    fatal_expected = 0;
}
static void *foreign_thread(void *unused)
{
    (void)unused;
    int32_t handle = 123;
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_WRONG_THREAD);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_WRONG_THREAD);
    CHECK(rt_modules_destroy(catalogue) == RT_MODULE_WRONG_THREAD);
    CHECK(rt_modules_state(catalogue, 10) == NULL);
    CHECK(rt_modules_load("/A", NULL, &handle) == RT_MODULE_BAD_SCOPE && handle == 123);
    return NULL;
}
static int32_t scalar_data;
RT_SYSLIB_DATA("owned", ownedScalar, scalar_data);

int main(void)
{
    unsigned found = 0;
    for (const rt_syslib_entry *s = rt_syslib_begin(); s != rt_syslib_end(); s++) {
        CHECK(strcmp(s->symbol, "sceKernelLoadStartModule") != 0); /* private entry is not a public alias */
        if (!strcmp(s->library, "libkernel") && !strcmp(s->symbol, "sceKernelLoadStartModuleForSysmodule")) {
            CHECK(s->capability == RT_SYSLIB_CAPABILITY_IMPLEMENTED);
            load = (load_fn)s->fn;
            found++;
        }
    }
    CHECK(found == 1 && load);
    found = 0;
    for (const rt_syslib_data *d = rt_syslib_data_begin(); d != rt_syslib_data_end(); d++) {
        if (!d->library) continue; /* linker sentinel is not a provider */
        CHECK(!strcmp(d->library, "owned") && !strcmp(d->symbol, "ownedScalar"));
        CHECK(d->address == &scalar_data && d->size == sizeof scalar_data);
        found++;
    }
    CHECK(found == 1);
    _Alignas(uint64_t) unsigned char canary[16];
    CHECK((uintptr_t)(canary + 3) % _Alignof(int32_t) != 0);
    memset(canary, 0xa5, sizeof canary);
    CHECK((uint32_t)load(NULL, UINT64_MAX, (void *)(uintptr_t)1, UINT32_MAX,
                         (void *)(uintptr_t)1, canary + 3) == UINT32_C(0x80020016));
    for (size_t i = 0; i < sizeof canary; i++) CHECK(canary[i] == 0xa5);
    expect_stop("/C", "no active");

    char path_a[] = "/A";
    wanted_override = (void *)(uintptr_t)0x12345678;
    rt_module_spec specs[] = {
        {path_a, 10, entry_a, wanted_override, 0}, {"/B", 20, entry_b, NULL, 1},
        {"/C", 30, entry_c, NULL, 3}, {"/D", 40, entry_d, NULL, 1}
    };
    CHECK(rt_modules_create(specs, 4, &catalogue) == RT_MODULE_OK);
    path_a[1] = 'Z'; /* catalogue owns its path copy */
    CHECK(*rt_modules_state(catalogue, 10) == RT_MODULE_PREPARED);
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_OK);
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_BAD_SCOPE);
    CHECK(rt_modules_destroy(catalogue) == RT_MODULE_BAD_SCOPE);
    int32_t result = -1;
    CHECK(load("/C", 0, NULL, 0, NULL, &result) == 30 && result == 0);
    CHECK(trace_size == 3 && !memcmp(trace, "ABC", 3));
    CHECK(load("/D", 0, NULL, 0, NULL, NULL) == 40);
    CHECK(trace_size == 4 && !memcmp(trace, "ABCD", 4));
    int32_t handle = -1;
    result = -2;
    CHECK(rt_modules_load("/B", &result, &handle) == RT_MODULE_ALREADY_STARTED);
    CHECK(result == -2 && handle == -1);
    expect_stop("/C", "repeated");
    expect_stop("/missing", "not a configured");
    pthread_t child;
    CHECK(pthread_create(&child, NULL, foreign_thread, NULL) == 0);
    CHECK(pthread_join(child, NULL) == 0);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_OK);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_BAD_SCOPE);
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_OK);
    CHECK(*rt_modules_state(catalogue, 20) == RT_MODULE_STARTED);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_OK);
    CHECK(rt_modules_destroy(catalogue) == RT_MODULE_OK);

    wanted_override = NULL;
    const rt_module_spec positive = {"/B", 1, entry_b, NULL, 0};
    CHECK(rt_modules_create(&positive, 1, &catalogue) == RT_MODULE_OK);
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_OK);
    memset(canary, 0xa5, sizeof canary);
    CHECK(load("/B", 0, NULL, 0, NULL, canary + 3) == 1);
    const unsigned char expected[4] = {7, 0, 0, 0};
    CHECK(!memcmp(canary + 3, expected, 4));
    for (size_t i = 0; i < sizeof canary; i++) if (i < 3 || i >= 7) CHECK(canary[i] == 0xa5);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_OK && rt_modules_destroy(catalogue) == RT_MODULE_OK);

    rt_module_spec failure = {"/N", 1, negative, NULL, 0};
    CHECK(rt_modules_create(&failure, 1, &catalogue) == RT_MODULE_OK);
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_OK);
    handle = 99; result = 88;
    CHECK(rt_modules_load("/N", &result, &handle) == RT_MODULE_INIT_FAILED);
    CHECK(result == -123 && handle == 99 && *rt_modules_state(catalogue, 1) == RT_MODULE_FAILED);
    result = 77;
    CHECK(rt_modules_load("/N", &result, &handle) == RT_MODULE_INIT_FAILED && result == 77);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_OK && rt_modules_destroy(catalogue) == RT_MODULE_OK);

    rt_module_spec nested[] = {{"/R", 1, reentrant, NULL, 0}, {"/B", 2, entry_b, NULL, 0}};
    CHECK(rt_modules_create(nested, 2, &catalogue) == RT_MODULE_OK);
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_OK);
    CHECK(load("/R", 0, NULL, 0, NULL, NULL) == 1);
    CHECK(*rt_modules_state(catalogue, 2) == RT_MODULE_PREPARED);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_OK && rt_modules_destroy(catalogue) == RT_MODULE_OK);

    const rt_module_spec interrupted = {"/X", 1, abandoned, NULL, 0};
    CHECK(rt_modules_create(&interrupted, 1, &catalogue) == RT_MODULE_OK);
    CHECK(rt_modules_enter(catalogue) == RT_MODULE_OK);
    if (!setjmp(recovery)) { (void)load("/X", 0, NULL, 0, NULL, NULL); CHECK(0); }
    CHECK(*rt_modules_state(catalogue, 1) == RT_MODULE_STARTING);
    CHECK(rt_modules_leave(catalogue) == RT_MODULE_OK);
    CHECK(*rt_modules_state(catalogue, 1) == RT_MODULE_FAILED);
    CHECK(rt_modules_destroy(catalogue) == RT_MODULE_OK);

    rt_module_spec invalid[] = {positive, positive};
    rt_modules *witness = (rt_modules *)(uintptr_t)0x1234;
    CHECK(rt_modules_create(invalid, 2, &witness) == RT_MODULE_BAD_LAYOUT
          && witness == (rt_modules *)(uintptr_t)0x1234);
    invalid[1] = (rt_module_spec){"/other", 2, entry_c, NULL, 4};
    CHECK(rt_modules_create(invalid, 2, &witness) == RT_MODULE_BAD_LAYOUT);
    invalid[1].dependencies = 1; invalid[0].dependencies = 2;
    CHECK(rt_modules_create(invalid, 2, &witness) == RT_MODULE_BAD_LAYOUT);
    invalid[0] = positive; invalid[0].entry = NULL;
    CHECK(rt_modules_create(invalid, 1, &witness) == RT_MODULE_BAD_LAYOUT);
    CHECK(rt_modules_create(NULL, 0, &witness) == RT_MODULE_BAD_ARGUMENT);
    CHECK(rt_modules_create(&positive, RT_MODULE_LIMIT + 1, &witness) == RT_MODULE_BAD_ARGUMENT);
    puts("owned module lifecycle passed");
    return 0;
}
