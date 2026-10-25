/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Private system-module loader entry, with the observed native three-argument
   initializer ABI over a pre-mapped owned catalogue. No public LoadStart alias. */
#include "runtime/module.h"
#include "runtime/process.h"
#include "runtime/syslib.h"
#include <stdint.h>

static int32_t kernel_load_start_for_sysmodule(const char *path, uint64_t argument_size,
                                              const void *arguments, uint32_t flags,
                                              const void *options, void *result)
{
    /* R8 is rejected before native load or any result write in the supplied helper. */
    if (options) return (int32_t)UINT32_C(0x80020016);
    if (argument_size || arguments || flags)
        rt_fatal("sceKernelLoadStartModuleForSysmodule", "only zero argument-size, arguments and caller flags are supported");
    int32_t handle = 0;
    const rt_module_status status = rt_modules_load(path, result, &handle);
    if (status != RT_MODULE_OK)
        rt_fatal("sceKernelLoadStartModuleForSysmodule", rt_module_status_message(status));
    return handle;
}
RT_SYSLIB("libkernel", sceKernelLoadStartModuleForSysmodule, kernel_load_start_for_sysmodule);
