/* SPDX-License-Identifier: GPL-2.0-or-later */
#if !defined(__APPLE__) || !defined(__MACH__) || !defined(__x86_64__)
#error Darwin guest startup requires an x86-64 Mach-O process.
#endif
#include "runtime/boot_darwin.h"
#include <libkern/OSCacheControl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int rt_boot_darwin_tls_cleanup(rt_boot_darwin_tls *s)
{
    if (!s) return -1;
    s->qualified = 0;
    if (s->key) {
        if (rt_guest_tls_clear(s->key).status != RT_GUEST_TLS_OK) return -1;
        if (rt_guest_tls_destroy(&s->key).status != RT_GUEST_TLS_OK) return -1;
    }
    /* With no live key, no registered/direct reader may still own this TCB. */
    free(s->allocation);
    s->allocation = s->tcb = NULL;
    /* A successful mapping at address zero still owns its full range. Retain the size on
       unmap failure so cleanup can be retried and prepare cannot overwrite ownership. */
    if (s->read_code_size) {
        if (munmap(s->read_code, s->read_code_size)) return -1;
        s->read_code = NULL;
    }
    s->read_code_size = 0;
    s->offset = 0;
    return 0;
}

static int failure(rt_boot_darwin_tls *s, char *error, size_t size, const char *message)
{
    const int cleanup = rt_boot_darwin_tls_cleanup(s);
    if (size) snprintf(error, size, "%s%s", message, cleanup ? "; startup cleanup incomplete" : "");
    return -1;
}

static int agrees(rt_boot_darwin_tls *s, void *expected, rt_boot_darwin_reader reader, void *user)
{
    const rt_guest_tls_result changed = expected ? rt_guest_tls_bind(s->key, expected)
                                                : rt_guest_tls_clear(s->key);
    if (changed.status != RT_GUEST_TLS_OK) return 0;
    void *public_value = (void *)(uintptr_t)1;
    uint64_t direct_value = 0;
    return rt_guest_tls_read(s->key, &public_value).status == RT_GUEST_TLS_OK
        && public_value == expected && reader(user, s->read_code, &direct_value) == 0
        && direct_value == (uint64_t)(uintptr_t)expected;
}

int rt_boot_darwin_tls_prepare(rt_boot_darwin_tls *s, const rt_elf_tls *tls,
                               const void *image, size_t image_size,
                               rt_boot_darwin_reader reader, void *user,
                               char *error, size_t error_size)
{
    if (!s || !tls || !image || !reader || (error_size && !error)) return -1;
    if (s->key || s->allocation || s->tcb || s->read_code || s->read_code_size || s->qualified) {
        if (error_size) snprintf(error, error_size, "Darwin startup TLS already owns resources");
        return -1;
    }
    const uint64_t align = tls->align > 16 ? tls->align : 16;
    if ((tls->align && (tls->align & (tls->align - 1)))
        || (align & (align - 1)) || align > SIZE_MAX || tls->memsz > SIZE_MAX
        || tls->filesz > tls->memsz || tls->vaddr > image_size
        || tls->filesz > image_size - tls->vaddr || tls->memsz > SIZE_MAX - (align - 1))
        return failure(s, error, error_size, "unrepresentable executable TLS template");
    const size_t tls_size = ((size_t)tls->memsz + (size_t)align - 1) & ~((size_t)align - 1);
    if (tls_size > SIZE_MAX - 4096)
        return failure(s, error, error_size, "executable TLS/TCB size overflow");
    if (posix_memalign(&s->allocation, (size_t)align, tls_size + 4096))
        return failure(s, error, error_size, "cannot allocate executable TLS/TCB");
    memset(s->allocation, 0, tls_size + 4096);
    s->tcb = (unsigned char *)s->allocation + tls_size;
    if (tls->filesz) memcpy(s->allocation, (const unsigned char *)image + tls->vaddr, (size_t)tls->filesz);
    const uintptr_t self = (uintptr_t)s->tcb;
    memcpy(s->tcb, &self, sizeof self);
    if (rt_guest_tls_create(&s->key).status != RT_GUEST_TLS_OK)
        return failure(s, error, error_size, "cannot create owned public pthread key");
    rt_guest_tls_candidate candidate;
    if (rt_guest_tls_get_candidate(s->key, &candidate).status != RT_GUEST_TLS_OK)
        return failure(s, error, error_size, "owned pthread key is outside the candidate GS-slot profile");
    s->offset = candidate.candidate_offset;
    unsigned char bytes[] = {0x65,0x48,0x8b,0x04,0x25,0,0,0,0,0xc3};
    for (unsigned i = 0; i < 4; i++) bytes[5 + i] = (unsigned char)(s->offset >> (8 * i));
    const long page = sysconf(_SC_PAGESIZE);
    if (page < (long)sizeof bytes)
        return failure(s, error, error_size, "cannot determine host code-page size");
    void *code = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (code == MAP_FAILED)
        return failure(s, error, error_size, "cannot allocate owned TLS qualification code");
    s->read_code = code;
    s->read_code_size = (size_t)page;
    if (!code)
        return failure(s, error, error_size, "host returned unsupported zero code address");
    memcpy(code, bytes, sizeof bytes);
    if (mprotect(code, (size_t)page, PROT_READ | PROT_EXEC))
        return failure(s, error, error_size, "cannot make owned TLS qualification code executable");
    sys_icache_invalidate(code, sizeof bytes);
    uint64_t witnesses[2][2] = {{UINT64_C(0x1122334455667788), 1},
                                {UINT64_C(0x8877665544332211), 2}};
    if (!agrees(s, witnesses[0], reader, user) || !agrees(s, witnesses[1], reader, user)
        || !agrees(s, NULL, reader, user) || !agrees(s, s->tcb, reader, user))
        return failure(s, error, error_size, "public/direct owned TSD qualification failed on this thread");
    if (memcmp(code, bytes, sizeof bytes))
        return failure(s, error, error_size, "owned TLS qualification code changed");
    if (munmap(code, s->read_code_size))
        return failure(s, error, error_size, "cannot release owned TLS qualification code");
    s->read_code = NULL;
    s->read_code_size = 0;
    s->qualified = 1;
    return 0;
}
