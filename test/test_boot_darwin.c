/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Own startup state/recovery test; include the production unit to check private cleanup directly. */
#define main owned_pbboot_main
#include "../runtime/boot.c"
#undef main
#include <libkern/OSCacheControl.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)
static unsigned failed_reads;
static int wrong_reader(void *user, const void *code, uint64_t *out)
{
    (void)user; (void)code; *out = 0; failed_reads++; return 0;
}

int main(void)
{
    stack_t before_alt, after_alt;
    struct sigaction before[6], after;
    CHECK(sigaltstack(NULL, &before_alt) == 0);
    for (size_t i = 0; i < 6; i++) CHECK(sigaction(darwin_signals[i], NULL, &before[i]) == 0);
    CHECK(darwin_install_signals() == 0);
    if (failures) return 1;
    void *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (stack == MAP_FAILED) return 1;
    struct darwin_reader reader = {(unsigned char *)stack + STACK_SIZE - 64, 1};
    const unsigned char template[] = {0x31,0x42,0x53,0x64};
    rt_elf_tls tls = {.vaddr = 0, .filesz = sizeof template, .memsz = 24, .align = 16};
    char error[256];
    rt_boot_darwin_tls rejected = {0};
    rt_elf_tls bad = tls; bad.align = 24;
    CHECK(rt_boot_darwin_tls_prepare(&rejected, &bad, template, sizeof template,
                                     wrong_reader, NULL, error, sizeof error) == -1);
    CHECK(!rejected.key && !rejected.allocation && !rejected.read_code && !failed_reads);
    bad.align = 3;
    CHECK(rt_boot_darwin_tls_prepare(&rejected, &bad, template, sizeof template,
                                     wrong_reader, NULL, error, sizeof error) == -1);
    CHECK(!rejected.key && !rejected.allocation && !rejected.read_code && !failed_reads);
    CHECK(rt_boot_darwin_tls_prepare(&rejected, &tls, template, sizeof template,
                                     wrong_reader, NULL, error, sizeof error) == -1);
    CHECK(failed_reads == 1 && !rejected.key && !rejected.allocation && !rejected.read_code);
    CHECK(rt_boot_darwin_tls_prepare(&darwin_tls, &tls, template, sizeof template,
                                     darwin_read_owned_slot, &reader, error, sizeof error) == 0);
    if (!darwin_tls.qualified) { fprintf(stderr, "%s\n", error); return 1; }
    CHECK(memcmp(darwin_tls.allocation, template, sizeof template) == 0);
    for (size_t i = sizeof template; i < 32; i++) CHECK(((unsigned char *)darwin_tls.allocation)[i] == 0);
    CHECK(*(uintptr_t *)darwin_tls.tcb == (uintptr_t)darwin_tls.tcb);
    void *public_value = NULL;
    CHECK(rt_guest_tls_read(darwin_tls.key, &public_value).status == RT_GUEST_TLS_OK);
    CHECK(public_value == darwin_tls.tcb);
    const long page = sysconf(_SC_PAGESIZE);
    if (page < 2) return 1;
    unsigned char *code = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANON, -1, 0);
    if (code == MAP_FAILED) return 1;
    code[0] = 0x0f; code[1] = 0x0b; /* owned UD2 */
    CHECK(mprotect(code, (size_t)page, PROT_READ | PROT_EXEC) == 0);
    sys_icache_invalidate(code, 2);
    run_guest(code, NULL, NULL, NULL, reader.stack_top, 1);
    CHECK(outcome == FAULT && fault_signal == SIGILL && fault_rip == (uintptr_t)code);
    CHECK(!recovery_active);
    CHECK(mprotect(code, (size_t)page, PROT_READ | PROT_WRITE) == 0);
    code[0] = 0xeb; code[1] = 0xfe; /* owned bounded-by-alarm spin */
    CHECK(mprotect(code, (size_t)page, PROT_READ | PROT_EXEC) == 0);
    sys_icache_invalidate(code, 2);
    run_guest(code, NULL, NULL, NULL, reader.stack_top, 1);
    CHECK(outcome == TIMEOUT && !recovery_active);
    CHECK(darwin_cleanup() == 0);
    CHECK(!darwin_tls.key && !darwin_tls.allocation && !darwin_tls.read_code && !darwin_tls.qualified);
    CHECK(darwin_cleanup() == 0);
    CHECK(sigaltstack(NULL, &after_alt) == 0);
    if (before_alt.ss_flags & SS_DISABLE) CHECK(after_alt.ss_flags & SS_DISABLE);
    else CHECK(after_alt.ss_sp == before_alt.ss_sp && after_alt.ss_size == before_alt.ss_size
               && !(after_alt.ss_flags & SS_DISABLE));
    for (size_t i = 0; i < 6; i++) {
        CHECK(sigaction(darwin_signals[i], NULL, &after) == 0);
        CHECK(after.sa_sigaction == before[i].sa_sigaction && after.sa_flags == before[i].sa_flags);
        for (int n = 1; n < NSIG; n++) CHECK(sigismember(&after.sa_mask, n) == sigismember(&before[i].sa_mask, n));
    }
    CHECK(munmap(code, (size_t)page) == 0);
    CHECK(munmap(stack, STACK_SIZE) == 0);
    printf("owned Darwin startup/recovery/cleanup failures=%d\n", failures);
    return failures != 0;
}
