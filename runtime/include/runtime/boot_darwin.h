/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_BOOT_DARWIN_H
#define RUNTIME_BOOT_DARWIN_H
#include "runtime/guest_tls.h"
#include "runtime/loader.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Main-thread-only startup ownership. Zero-initialize, prepare once, then clean up on the
   SAME thread after all guest/direct readers have stopped. No guest thread creation is supplied.
   The reader calls only our ten-byte GS-slot read on an owned stack with fault/timeout recovery;
   it returns 0 and publishes RAX only after a normal return, otherwise -1. */
typedef int (*rt_boot_darwin_reader)(void *user, const void *code, uint64_t *value);
typedef struct {
    rt_guest_tls_key *key;
    void *allocation, *tcb, *read_code;
    size_t read_code_size;
    uint32_t offset;
    int qualified;
} rt_boot_darwin_tls;

/* Allocate the executable's variant-II template/TCB; verify this thread's public TSD value
   against an owned GS-relative read for two live pointers, NULL and the real TCB. Only then is
   offset suitable for the bounded loader pattern rewrite. This is not a public Darwin layout
   guarantee, arbitrary FS/GS instruction support, or qualification of another host thread.
   The image is borrowed readable storage of image_size bytes. No target code is called here. */
int rt_boot_darwin_tls_prepare(rt_boot_darwin_tls *state, const rt_elf_tls *tls,
                               const void *image, size_t image_size,
                               rt_boot_darwin_reader reader, void *user,
                               char *error, size_t error_size);
/* Clear the public key before freeing its TCB; failed cleanup retains the resources still
   owned. Idempotent after success. No automatic thread-exit/destructor guessing. */
int rt_boot_darwin_tls_cleanup(rt_boot_darwin_tls *state);
#ifdef __cplusplus
}
#endif
#endif
