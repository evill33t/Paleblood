/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exact boot translation unit with a test-only count-query replacement. It never calls sysctl. */
#define rt_logical_cpu_darwin_count owned_darwin_count
#define main owned_boot_main
#include "../runtime/boot.c"
#undef main
#undef rt_logical_cpu_darwin_count

rt_logical_cpu_result owned_darwin_count(uint32_t *out)
{
    fputs("owned test: Darwin count query called\n", stderr);
    const char *failure = getenv("PB_TEST_COUNT_FAILURE");
    if (failure) return (rt_logical_cpu_result){RT_LOGICAL_CPU_HOST_ERROR, EIO};
    *out = 3;
    return (rt_logical_cpu_result){RT_LOGICAL_CPU_OK, 0};
}

int main(int argc, char **argv) { return owned_boot_main(argc, argv); }
