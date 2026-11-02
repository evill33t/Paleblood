/* SPDX-License-Identifier: GPL-2.0-or-later */
/* System-module entry points over the explicitly declared own-runtime stub namespace.
   The supplied firmware forwards the complete EDI DWORD. Do not truncate a wider
   request into an unrelated declared stub. Only the independently bound early
   invalid-ID statuses are returned; unknown native loading/state behavior still
   stops by name. See docs/own-runtime-sysmodule-wire.md for evidence and limits. */
#include "runtime/process.h"
#include "runtime/syslib.h"

#include <stdint.h>
#include <stdio.h>

/* Stops with the call, the module id and, for a stub, its name. */
static _Noreturn void sysmodule_fatal(const char *function, uint32_t id, const rt_stub_module *stub,
                                      const char *what)
{
    char message[256];
    snprintf(message, sizeof message, "module 0x%04x%s%s%s: %s", (unsigned)id, stub ? " (" : "",
             stub ? stub->name : "", stub ? ")" : "", what);
    rt_fatal(function, message);
}

/* Host stub declarations use 16-bit IDs. Wider native requests remain unsupported;
   rejecting them before lookup preserves every declared record instead of aliasing. */
static uint16_t owned_module_id(const char *function, uint32_t argument)
{
    if (argument > UINT16_MAX)
        sysmodule_fatal(function, argument, NULL, "full-width module ID is outside the supported owned namespace");
    return (uint16_t)argument;
}

static rt_stub_module *stub_module(uint16_t id)
{
    for (uint32_t i = 0; i < rt_process.stub_module_count; i++)
        if (rt_process.stub_modules[i].id == id) return &rt_process.stub_modules[i];
    return NULL;
}

static int32_t sysmodule_load_module(uint32_t argument)
{
    if ((argument & UINT32_C(0x7fffffff)) == 0) return (int32_t)UINT32_C(0x805a1000);
    const uint16_t id = owned_module_id("sceSysmoduleLoadModule", argument);
    rt_stub_module *stub = stub_module(id);
    if (!stub) sysmodule_fatal("sceSysmoduleLoadModule", id, NULL, "not a stub module, and the runtime cannot load system modules yet");
    if (stub->loaded)
        sysmodule_fatal("sceSysmoduleLoadModule", id, stub, "already loaded; whether the system counts a second load or refuses it is not established");
    stub->loaded = 1;
    return 0;
}
RT_SYSLIB("libSceSysmodule", sceSysmoduleLoadModule, sysmodule_load_module);

static int32_t sysmodule_is_loaded(uint32_t argument)
{
    if (argument == 0) return (int32_t)UINT32_C(0x805a1000);
    const uint16_t id = owned_module_id("sceSysmoduleIsLoaded", argument);
    const rt_stub_module *stub = stub_module(id);
    if (!stub) sysmodule_fatal("sceSysmoduleIsLoaded", id, NULL, "not a stub module, and the runtime cannot load system modules yet");
    if (!stub->loaded) sysmodule_fatal("sceSysmoduleIsLoaded", id, stub, "not loaded; the error value for a module not loaded is not established");
    return 0;
}
RT_SYSLIB("libSceSysmodule", sceSysmoduleIsLoaded, sysmodule_is_loaded);

static int32_t sysmodule_unload_module(uint32_t argument)
{
    if (argument == 0) return (int32_t)UINT32_C(0x805a1000);
    const uint16_t id = owned_module_id("sceSysmoduleUnloadModule", argument);
    rt_stub_module *stub = stub_module(id);
    if (!stub) sysmodule_fatal("sceSysmoduleUnloadModule", id, NULL, "not a stub module, and the runtime cannot load system modules yet");
    if (!stub->loaded)
        sysmodule_fatal("sceSysmoduleUnloadModule", id, stub, "not loaded; the error value for a module not loaded is not established");
    stub->loaded = 0;
    return 0;
}
RT_SYSLIB("libSceSysmodule", sceSysmoduleUnloadModule, sysmodule_unload_module);
