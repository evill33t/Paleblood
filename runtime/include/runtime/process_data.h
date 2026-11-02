/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_PROCESS_DATA_H
#define RUNTIME_PROCESS_DATA_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* An opt-in owned process, not console defaults. Explicit counts exclude each
   vector's required NULL terminator; a zero count also accepts a NULL vector.
   Strings and vectors are copied into writable process-lifetime storage.
   Host environ is never implicitly inherited. Call before entering any guest.
   After one success there is no reset: guest code may retain every address. */
#define RT_PROCESS_MAX_ARGS 64u
#define RT_PROCESS_MAX_ENV 128u
#define RT_PROCESS_STRING_BYTES (1024u * 1024u)
typedef enum {
    RT_PROCESS_DATA_OK, RT_PROCESS_DATA_INVALID, RT_PROCESS_DATA_NO_MEMORY,
    RT_PROCESS_DATA_ENTROPY, RT_PROCESS_DATA_BUSY, RT_PROCESS_DATA_ALREADY_PREPARED
} rt_process_data_status;
rt_process_data_status rt_process_data_prepare(size_t argc, const char *const *argv,
                                               size_t envc, const char *const *env);
/* Acquire publication before using the raw argc/argv/env block or registry data.
   The parameter block is NULL before successful preparation. */
int rt_process_data_ready(void);
void *rt_process_data_parameters(void);
const char *rt_process_data_status_message(rt_process_data_status status);
#ifdef __cplusplus
}
#endif
#endif
