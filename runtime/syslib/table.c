/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "runtime/syslib.h"

/* Bounds describe this linked image, not entries in other loaded libraries. */
#if defined(__APPLE__) && defined(__MACH__)
extern const rt_syslib_entry __start_rt_syslib[]
    __asm("section$start$__DATA$rt_syslib") __attribute__((weak_import));
extern const rt_syslib_entry __stop_rt_syslib[]
    __asm("section$end$__DATA$rt_syslib") __attribute__((weak_import));
#else
/* An empty ELF table still links: the weak bounds then stay null. */
extern const rt_syslib_entry __start_rt_syslib[] __attribute__((weak));
extern const rt_syslib_entry __stop_rt_syslib[] __attribute__((weak));
#endif

const rt_syslib_entry *rt_syslib_begin(void) { return __start_rt_syslib; }
const rt_syslib_entry *rt_syslib_end(void) { return __stop_rt_syslib; }

/* A null sentinel gives both ELF and Mach-O a real section when there are no
   admitted data providers. Iterators skip it; it never supplies an object. */
__attribute__((used, section(RT_SYSLIB_DATA_SECTION), aligned(sizeof(void *))))
static const rt_syslib_data empty_data_record = {NULL, NULL, NULL, 0};
#if defined(__APPLE__) && defined(__MACH__)
extern const rt_syslib_data data_start[] __asm("section$start$__DATA$rt_sysdata");
extern const rt_syslib_data data_stop[] __asm("section$end$__DATA$rt_sysdata");
#else
extern const rt_syslib_data data_start[] __asm("__start_rt_sysdata");
extern const rt_syslib_data data_stop[] __asm("__stop_rt_sysdata");
#endif
const rt_syslib_data *rt_syslib_data_begin(void) { return data_start; }
const rt_syslib_data *rt_syslib_data_end(void) { return data_stop; }
