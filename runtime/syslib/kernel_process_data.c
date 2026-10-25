/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DEFAULT_SOURCE
#include "runtime/process_data.h"
#include "runtime/syslib.h"
#include <stdatomic.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

_Static_assert(sizeof(void *) == 8, "owned process parameters require 64-bit pointers");
struct process_parameters { uint64_t argc; char *items[]; };
static struct process_parameters *parameters;
static char **guest_environment;
static char *guest_program_name;
static _Alignas(16) unsigned char guest_guard[64];
/* A BSD-derived four-word preference set, initially empty by owned policy.
   This is guest storage; it neither reads nor changes the host signal mask. */
static uint32_t guest_interrupt_preferences[4];
enum { UNREADY, READY, PREPARING };
static atomic_int state;
static int32_t process_identity; /* Immutable after the READY release. */

static int host_identity(int32_t *out)
{
    const int saved = errno;
    const pid_t pid = getpid();
    errno = saved;
    if (pid <= 0 || (uintmax_t)pid > INT32_MAX) return 0;
    *out = (int32_t)pid;
    return 1;
}
static rt_process_data_status identity_status(int32_t *out)
{
    const int current = atomic_load_explicit(&state, memory_order_acquire);
    if (current != READY)
        return current == PREPARING ? RT_PROCESS_DATA_BUSY : RT_PROCESS_DATA_NOT_READY;
    int32_t actual;
    if (!host_identity(&actual)) return RT_PROCESS_DATA_HOST_PID;
    if (actual != process_identity) return RT_PROCESS_DATA_INHERITED;
    if (out) *out = process_identity;
    return RT_PROCESS_DATA_OK;
}
rt_process_data_status rt_process_data_pid(int32_t *out)
{
    return out ? identity_status(out) : RT_PROCESS_DATA_INVALID;
}

int rt_process_data_ready(void) { return identity_status(NULL) == RT_PROCESS_DATA_OK; }
void *rt_process_data_parameters(void) { return rt_process_data_ready() ? parameters : NULL; }

static int string_sizes(size_t count, const char *const *strings, size_t *bytes)
{
    if (!strings) return !count;
    if (strings[count]) return 0;
    for (size_t i = 0; i < count; i++) {
        if (!strings[i] || *bytes == RT_PROCESS_STRING_BYTES) return 0;
        const size_t remaining = RT_PROCESS_STRING_BYTES - *bytes;
        const size_t length = strnlen(strings[i], remaining);
        if (length == remaining) return 0;
        *bytes += length + 1;
    }
    return 1;
}
rt_process_data_status rt_process_data_prepare(size_t argc, const char *const *argv,
                                               size_t envc, const char *const *env)
{
    int expected = UNREADY;
    if (!atomic_compare_exchange_strong_explicit(&state, &expected, PREPARING,
                                                 memory_order_acq_rel, memory_order_acquire)) {
        if (expected != READY) return RT_PROCESS_DATA_BUSY;
        const rt_process_data_status identity = identity_status(NULL);
        return identity == RT_PROCESS_DATA_OK ? RT_PROCESS_DATA_ALREADY_PREPARED : identity;
    }
    rt_process_data_status result = RT_PROCESS_DATA_INVALID;
    size_t bytes = 0;
    struct process_parameters *prepared = NULL;
    int32_t prepared_identity;
    if (argc > RT_PROCESS_MAX_ARGS || envc > RT_PROCESS_MAX_ENV
        || !string_sizes(argc, argv, &bytes) || !string_sizes(envc, env, &bytes)) goto failed;
    if (!host_identity(&prepared_identity)) { result = RT_PROCESS_DATA_HOST_PID; goto failed; }
    const size_t vector_bytes = (argc + envc + 2) * sizeof(char *);
    prepared = malloc(sizeof *prepared + vector_bytes + bytes);
    if (!prepared) { result = RT_PROCESS_DATA_NO_MEMORY; goto failed; }
    prepared->argc = argc;
    char *copy = (char *)prepared->items + vector_bytes;
    size_t index = 0;
    for (size_t group = 0; group < 2; group++) {
        const size_t count = group ? envc : argc;
        const char *const *strings = group ? env : argv;
        for (size_t i = 0; i < count; i++) {
            const size_t length = strlen(strings[i]) + 1;
            prepared->items[index++] = copy;
            memcpy(copy, strings[i], length);
            copy += length;
        }
        prepared->items[index++] = NULL;
    }
    /* Native guard storage is 64 bytes, with a first-QWORD readiness sentinel.
       Host getentropy is an explicit port policy, not the console's service.
       Failure (including a zero first word) never admits fixed/zero substitutes. */
    unsigned char guard[sizeof guest_guard];
    uint64_t first;
    if (getentropy(guard, sizeof guard)) { result = RT_PROCESS_DATA_ENTROPY; goto failed; }
    memcpy(&first, guard, sizeof first);
    if (!first) { result = RT_PROCESS_DATA_ENTROPY; goto failed; }
    char *program = argc ? prepared->items[0] : NULL;
    if (program) {
        char *slash = strrchr(program, '/');
        if (slash) program = slash + 1;
    }
    parameters = prepared;
    guest_environment = &prepared->items[argc + 1];
    guest_program_name = program;
    memcpy(guest_guard, guard, sizeof guard);
    memset(guest_interrupt_preferences, 0, sizeof guest_interrupt_preferences);
    process_identity = prepared_identity;
    atomic_store_explicit(&state, READY, memory_order_release);
    return RT_PROCESS_DATA_OK;
failed:
    free(prepared);
    atomic_store_explicit(&state, UNREADY, memory_order_release);
    return result;
}
const char *rt_process_data_status_message(rt_process_data_status status)
{
    switch (status) {
    case RT_PROCESS_DATA_OK: return "ready";
    case RT_PROCESS_DATA_INVALID: return "invalid owned argv/environment or string budget";
    case RT_PROCESS_DATA_NO_MEMORY: return "cannot allocate owned process parameters";
    case RT_PROCESS_DATA_ENTROPY: return "host entropy failed or produced a zero guard sentinel";
    case RT_PROCESS_DATA_BUSY: return "owned process preparation is already in progress";
    case RT_PROCESS_DATA_ALREADY_PREPARED: return "owned process data is already prepared";
    case RT_PROCESS_DATA_HOST_PID: return "host process ID is not a positive int32 value";
    case RT_PROCESS_DATA_NOT_READY: return "enable --owned-process-data before guest execution";
    case RT_PROCESS_DATA_INHERITED: return "host-fork inherited owned process data is unsupported; no reset/rebind";
    }
    return "unknown owned process data status";
}
RT_SYSLIB_DATA_WITH_READY("libkernel", environ, guest_environment, rt_process_data_ready);
RT_SYSLIB_DATA_WITH_READY("libkernel", __progname, guest_program_name, rt_process_data_ready);
RT_SYSLIB_DATA_WITH_READY("libkernel", __stack_chk_guard, guest_guard, rt_process_data_ready);
RT_SYSLIB_DATA_WITH_READY("libkernel", _sigintr, guest_interrupt_preferences, rt_process_data_ready);
