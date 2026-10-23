/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Four observed libkernel wires over an explicitly configured Paleblood backing pool.
   Unsupported policy is diagnosed by operation name; target kernel validation/error precedence
   has not been recovered. Never translate an arbitrary host errno into a console status. */
#include "runtime/direct_memory.h"
#include "runtime/process.h"
#include "runtime/syslib.h"

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>

static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;
static rt_direct_pool *runtime_pool;

typedef struct { sigset_t previous_mask; } pool_guard;

static pool_guard lock_pool(void)
{
    pool_guard guard;
    sigset_t timeout_signal;
    if (sigemptyset(&timeout_signal) || sigaddset(&timeout_signal, SIGALRM)
        || pthread_sigmask(SIG_BLOCK, &timeout_signal, &guard.previous_mask))
        rt_fatal("direct memory", "could not defer the boot timeout signal");
    if (pthread_mutex_lock(&pool_lock)) {
        if (pthread_sigmask(SIG_SETMASK, &guard.previous_mask, NULL))
            rt_fatal("direct memory", "pool lock failed and its previous signal mask could not be restored");
        rt_fatal("direct memory", "pool mutex lock failed");
    }
    return guard;
}

static void unlock_pool(const pool_guard *guard)
{
    const int unlocked = pthread_mutex_unlock(&pool_lock);
    /* The boot handler may siglongjmp as soon as a pending SIGALRM is unblocked. */
    const int restored = pthread_sigmask(SIG_SETMASK, &guard->previous_mask, NULL);
    if (unlocked || restored) rt_fatal("direct memory", "pool unlock or signal-mask restoration failed");
}

static void require_pool(const char *operation, const pool_guard *guard)
{
    if (!runtime_pool) {
        unlock_pool(guard);
        rt_fatal(operation, "direct-memory pool is not configured; use --direct-memory-size BYTES");
    }
}

rt_direct_result rt_direct_runtime_configure(uint64_t capacity)
{
    pool_guard guard = lock_pool();
    const rt_direct_result result = runtime_pool ? RT_DIRECT_ALREADY_CONFIGURED
                                                : rt_direct_pool_create(capacity, &runtime_pool);
    unlock_pool(&guard);
    return result;
}

rt_direct_result rt_direct_runtime_shutdown(void)
{
    pool_guard guard = lock_pool();
    const rt_direct_result result = rt_direct_pool_destroy(&runtime_pool);
    unlock_pool(&guard);
    return result;
}

static int32_t status(const char *operation, rt_direct_result result)
{
    switch (result) {
    case RT_DIRECT_OK: return 0;
    case RT_DIRECT_INVALID: return (int32_t)UINT32_C(0x80020016); /* own-policy EINVAL */
    case RT_DIRECT_NO_MEMORY: return (int32_t)UINT32_C(0x8002000c); /* own-policy ENOMEM */
    case RT_DIRECT_UNSUPPORTED_TYPE:
        rt_fatal(operation, "unsupported direct-memory type; only type 0 is implemented");
    case RT_DIRECT_UNSUPPORTED_MAPPING:
        rt_fatal(operation, "unsupported direct-memory mapping; only CPU protection 3 and flags 0 are implemented");
    case RT_DIRECT_UNSUPPORTED_RELEASE:
        rt_fatal(operation, "partial direct-memory release is unsupported; release one exact allocation");
    case RT_DIRECT_MAPPED_ALLOCATION:
        rt_fatal(operation, "release of a direct-memory allocation with live mappings is unsupported");
    case RT_DIRECT_UNSUPPORTED_HOST:
        rt_fatal(operation, "host page geometry cannot support the 16 KiB direct-memory policy");
    default:
        rt_fatal(operation, "direct-memory backing or cleanup failed");
    }
}

static uint64_t direct_size(void)
{
    pool_guard guard = lock_pool();
    require_pool("sceKernelGetDirectMemorySize", &guard);
    const uint64_t capacity = rt_direct_pool_capacity(runtime_pool);
    unlock_pool(&guard);
    return capacity;
}

static int32_t direct_allocate(uint64_t start, uint64_t end, uint64_t length, uint64_t alignment,
                               int32_t type, void *out_offset)
{
    uint64_t offset;
    pool_guard guard = lock_pool();
    require_pool("sceKernelAllocateDirectMemory", &guard);
    const rt_direct_result result = rt_direct_pool_allocate(runtime_pool, start, end, length,
                                                           alignment, type, &offset);
    unlock_pool(&guard);
    /* Borrowed guest slots are touched outside the pool lock: a guest output fault must not
       leave this lock owned when the boot harness returns to its signal handler. */
    if (result == RT_DIRECT_OK) memcpy(out_offset, &offset, sizeof offset);
    return status("sceKernelAllocateDirectMemory", result);
}

static int32_t direct_map(void *inout_address, uint64_t length, int32_t protection, int32_t flags,
                         uint64_t offset, uint64_t alignment)
{
    pool_guard guard = lock_pool();
    require_pool("sceKernelMapDirectMemory", &guard);
    unlock_pool(&guard);
    uint64_t address = 0;
    /* The captured wrapper reads this borrowed slot before checking mapping arguments. */
    memcpy(&address, inout_address, sizeof address);
    guard = lock_pool();
    require_pool("sceKernelMapDirectMemory", &guard);
    const rt_direct_result result = rt_direct_pool_map(runtime_pool, &address,
                                                      length, protection, flags, offset, alignment);
    unlock_pool(&guard);
    if (result == RT_DIRECT_OK) memcpy(inout_address, &address, sizeof address);
    return status("sceKernelMapDirectMemory", result);
}

static int32_t direct_release(uint64_t offset, uint64_t length)
{
    pool_guard guard = lock_pool();
    require_pool("sceKernelReleaseDirectMemory", &guard);
    const rt_direct_result result = rt_direct_pool_release(runtime_pool, offset, length);
    unlock_pool(&guard);
    return status("sceKernelReleaseDirectMemory", result);
}

RT_SYSLIB("libkernel", sceKernelGetDirectMemorySize, direct_size);
RT_SYSLIB("libkernel", sceKernelAllocateDirectMemory, direct_allocate);
RT_SYSLIB("libkernel", sceKernelMapDirectMemory, direct_map);
RT_SYSLIB("libkernel", sceKernelReleaseDirectMemory, direct_release);
