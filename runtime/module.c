/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "runtime/module.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    rt_module_spec spec;
    unsigned char state;
} module;
struct rt_modules {
    pthread_t owner;
    size_t count;
    module *records;
    unsigned loading;
};
static _Thread_local rt_modules *current;
static int owner(const rt_modules *m) { return pthread_equal(m->owner, pthread_self()); }

static void release(rt_modules *m)
{
    for (size_t i = 0; i < m->count; i++) free((void *)m->records[i].spec.path);
    free(m->records);
    free(m);
}
static int acyclic(const rt_module_spec *specs, size_t count, size_t index, unsigned char *colors)
{
    if (colors[index] == 1) return 0;
    if (colors[index] == 2) return 1;
    colors[index] = 1;
    for (size_t i = 0; i < count; i++)
        if ((specs[index].dependencies & (UINT64_C(1) << i)) && !acyclic(specs, count, i, colors)) return 0;
    colors[index] = 2;
    return 1;
}
rt_module_status rt_modules_create(const rt_module_spec *specs, size_t count, rt_modules **out)
{
    if (!out || !count || !specs || count > RT_MODULE_LIMIT) return RT_MODULE_BAD_ARGUMENT;
    const uint64_t allowed = count == RT_MODULE_LIMIT ? UINT64_MAX : (UINT64_C(1) << count) - 1;
    for (size_t i = 0; i < count; i++) {
        if (!specs[i].path || !*specs[i].path || !specs[i].entry || specs[i].handle <= 0
            || (specs[i].dependencies & ~allowed)) return RT_MODULE_BAD_LAYOUT;
        for (size_t j = 0; j < i; j++)
            if (specs[i].handle == specs[j].handle || !strcmp(specs[i].path, specs[j].path))
                return RT_MODULE_BAD_LAYOUT;
    }
    unsigned char colors[RT_MODULE_LIMIT] = {0};
    for (size_t i = 0; i < count; i++)
        if (!acyclic(specs, count, i, colors)) return RT_MODULE_BAD_LAYOUT;
    rt_modules *m = calloc(1, sizeof *m);
    if (!m) return RT_MODULE_NO_MEMORY;
    m->records = calloc(count, sizeof *m->records);
    if (!m->records) { free(m); return RT_MODULE_NO_MEMORY; }
    m->owner = pthread_self();
    m->count = count;
    for (size_t i = 0; i < count; i++) {
        m->records[i].spec = specs[i];
        m->records[i].spec.path = strdup(specs[i].path);
        if (!m->records[i].spec.path) { release(m); return RT_MODULE_NO_MEMORY; }
    }
    *out = m;
    return RT_MODULE_OK;
}
rt_module_status rt_modules_enter(rt_modules *m)
{
    if (!m) return RT_MODULE_BAD_ARGUMENT;
    if (!owner(m)) return RT_MODULE_WRONG_THREAD;
    if (current) return RT_MODULE_BAD_SCOPE;
    current = m;
    return RT_MODULE_OK;
}
rt_module_status rt_modules_leave(rt_modules *m)
{
    if (!m) return RT_MODULE_BAD_ARGUMENT;
    if (!owner(m)) return RT_MODULE_WRONG_THREAD;
    if (current != m) return RT_MODULE_BAD_SCOPE;
    for (size_t i = 0; i < m->count; i++)
        if (m->records[i].state == RT_MODULE_STARTING) m->records[i].state = RT_MODULE_FAILED;
    m->loading = 0;
    current = NULL;
    return RT_MODULE_OK;
}
static rt_module_status start(rt_modules *m, size_t index, void *result)
{
    module *record = &m->records[index];
    if (record->state == RT_MODULE_STARTED) return RT_MODULE_OK;
    if (record->state == RT_MODULE_FAILED) return RT_MODULE_INIT_FAILED;
    if (record->state != RT_MODULE_PREPARED) return RT_MODULE_REENTRANT;
    record->state = RT_MODULE_STARTING;
    for (size_t i = 0; i < m->count; i++) {
        if (!(record->spec.dependencies & (UINT64_C(1) << i))) continue;
        const rt_module_status status = start(m, i, NULL);
        if (status != RT_MODULE_OK) { record->state = RT_MODULE_FAILED; return status; }
    }
    const int32_t value = record->spec.entry(0, NULL, record->spec.start_override);
    if (result) memcpy(result, &value, sizeof value);
    record->state = value < 0 ? RT_MODULE_FAILED : RT_MODULE_STARTED;
    return value < 0 ? RT_MODULE_INIT_FAILED : RT_MODULE_OK;
}
rt_module_status rt_modules_load(const char *path, void *result, int32_t *handle)
{
    if (!path || !handle) return RT_MODULE_BAD_ARGUMENT;
    if (!current) return RT_MODULE_BAD_SCOPE;
    if (current->loading) return RT_MODULE_REENTRANT;
    size_t index = 0;
    while (index < current->count && strcmp(current->records[index].spec.path, path)) index++;
    if (index == current->count) return RT_MODULE_UNKNOWN_PATH;
    if (current->records[index].state == RT_MODULE_STARTED) return RT_MODULE_ALREADY_STARTED;
    if (current->records[index].state != RT_MODULE_PREPARED) return RT_MODULE_INIT_FAILED;
    current->loading = 1;
    const rt_module_status status = start(current, index, result);
    current->loading = 0;
    if (status == RT_MODULE_OK) *handle = current->records[index].spec.handle;
    return status;
}
const unsigned char *rt_modules_state(const rt_modules *m, int32_t handle)
{
    if (!m || !owner(m)) return NULL;
    for (size_t i = 0; i < m->count; i++)
        if (m->records[i].spec.handle == handle) return &m->records[i].state;
    return NULL;
}
rt_module_status rt_modules_destroy(rt_modules *m)
{
    if (!m) return RT_MODULE_BAD_ARGUMENT;
    if (!owner(m)) return RT_MODULE_WRONG_THREAD;
    if (current == m) return RT_MODULE_BAD_SCOPE;
    release(m);
    return RT_MODULE_OK;
}
const char *rt_module_status_message(rt_module_status status)
{
    switch (status) {
    case RT_MODULE_OK: return "ready";
    case RT_MODULE_BAD_ARGUMENT: return "invalid owned module catalogue argument";
    case RT_MODULE_BAD_LAYOUT: return "duplicate, cyclic or inconsistent module catalogue";
    case RT_MODULE_NO_MEMORY: return "could not prepare owned module catalogue";
    case RT_MODULE_WRONG_THREAD: return "module catalogue belongs to another host thread";
    case RT_MODULE_BAD_SCOPE: return "no active owned guest module scope";
    case RT_MODULE_UNKNOWN_PATH: return "path is not a configured system module";
    case RT_MODULE_ALREADY_STARTED: return "direct repeated module loads are not supported";
    case RT_MODULE_REENTRANT: return "reentrant or partially initialized module startup is unsupported";
    case RT_MODULE_INIT_FAILED: return "module initializer failed or was abandoned; native rollback is unsupported";
    }
    return "unknown owned module status";
}
