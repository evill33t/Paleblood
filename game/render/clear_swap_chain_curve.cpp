// SPDX-License-Identifier: GPL-2.0-or-later
// Preserve the full native RAX wire even when a null owner/resource skips cleanup.
#include "swap_chain_cleanup.h"

#include <cstdio>
#include <cstdlib>

extern "C" __attribute__((visibility("hidden"), noinline, used))
uint64_t bb_render_clear_swap_chain_curve_body(render::SwapChainCleanupRecord *record,
                     uint64_t incoming_rax)
{
    return render::clear_swap_chain_cleanup_record(record, incoming_rax);
}

#if defined(__x86_64__)
extern "C" __attribute__((naked, no_stack_protector))
uint64_t bb_render_clear_swap_chain_curve(render::SwapChainCleanupRecord *)
{
    // Incoming RSI is not consumed by this original body. RDI and the original
    // return address remain in place; tail-jump entry RSP is still 8 mod 16.
    // Capture RAX before any compiler-generated prologue can change it.
    __asm__(
        "movq %rax, %rsi\n"
#if defined(__APPLE__)
        "jmp _bb_render_clear_swap_chain_curve_body\n"
#else
        "jmp bb_render_clear_swap_chain_curve_body\n"
#endif
    );
}
#else
extern "C" uint64_t bb_render_clear_swap_chain_curve(render::SwapChainCleanupRecord *)
{
    std::fputs("bb_render_clear_swap_chain_curve: native entry requires AMD64 System V; "
               "use the explicit incoming-RAX body on other CPUs\n", stderr);
    std::abort();
}
#endif
