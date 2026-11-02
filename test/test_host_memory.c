/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../runtime/syslib/kernel_memory.c"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    int64_t offset = -1;
    CHECK(get_direct_memory_size() == RT_DIRECT_MEMORY_SIZE);
    CHECK(allocate_direct_memory(0, RT_DIRECT_MEMORY_SIZE, DIRECT_PAGE, DIRECT_PAGE, MEMORY_TYPE_WB_ONION, &offset) == 0);
    CHECK(offset == 0 && (fcntl(pool_fd, F_GETFD) & FD_CLOEXEC));
    void *first = NULL, *second = NULL;
    CHECK(map_direct_memory(&first, DIRECT_PAGE, 0x33, 0, offset, DIRECT_PAGE) == 0);
    CHECK(map_direct_memory(&second, DIRECT_PAGE, 3, 0, offset, DIRECT_PAGE) == 0);
    CHECK(first != second && !((uintptr_t)first % DIRECT_PAGE));
    memset(first, 0x4d, DIRECT_PAGE); CHECK(((unsigned char *)second)[DIRECT_PAGE - 1] == 0x4d);
    guest_mapping recorded; CHECK(mapping_holding((uintptr_t)first, &recorded));
    CHECK(recorded.direct == 1 && recorded.protection == 0x33 && recorded.offset == 0);
    /* A shared guard exposes accidental MAP_FIXED replacement across the child. */
    unsigned char *guard = mmap(NULL, DIRECT_PAGE * 2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    CHECK(guard != MAP_FAILED);
    unsigned char *aligned = (unsigned char *)align_up((uintptr_t)guard, DIRECT_PAGE);
    memset(aligned, 0x7a, DIRECT_PAGE);
    pid_t child = fork(); CHECK(child >= 0);
    if (!child) { void *hint = aligned; map_direct_memory(&hint, DIRECT_PAGE, 3, 0, offset, DIRECT_PAGE); _exit(0); }
    int status; CHECK(waitpid(child, &status, 0) == child && WIFSIGNALED(status));
    for (size_t i = 0; i < DIRECT_PAGE; i++) CHECK(aligned[i] == 0x7a);
    CHECK(munmap(guard, DIRECT_PAGE * 2) == 0);
    puts("host direct memory: sparse backing, shared aliases, GPU metadata and occupied hint preserved");
    return 0;
}
