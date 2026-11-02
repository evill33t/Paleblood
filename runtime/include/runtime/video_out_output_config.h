/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef RUNTIME_VIDEO_OUT_OUTPUT_CONFIG_H
#define RUNTIME_VIDEO_OUT_OUTPUT_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RT_VIDEO_OUT_OUTPUT_FRAME_BYTES = 88,
       RT_VIDEO_OUT_OUTPUT_FRAME_ALIGNMENT = 16 };

/* OWNED transport for the known outgoing native wires, not extra functional
   arguments of the original service. stack_slot addresses the actual projected
   QWORD, not the incoming late DWORD. The genuine bridge must supply these
   wires to the actual target, including full RAX and all 32 YMM0 bytes. Remaining
   native registers/flags, physical stack/frame and continuation are unproved. */
typedef struct {
    uint64_t rdi, rsi, rdx, rcx, r8, r9;
    uint64_t rax;
    uint64_t stack_slot;
    uint8_t ymm0[32];
} rt_video_out_output_native_call;

/* These are OWNED bridge prefixes, not inferred native provider prototypes.
   Raw live import targets, including zero, are forwarded unchanged. SDK,
   diagnostic and stack-failure prefixes express only the known first GP wire;
   the stack-failure first wire is the selected guard pointer. Other ambient
   native state is not captured or materialized by this C source.
   No bridge, service response, kernel value or success fallback is supplied. */
typedef uint64_t (*rt_video_out_output_sdk_fn)(uint64_t target, void *output);
typedef uint64_t (*rt_video_out_output_diagnostic_fn)(uint64_t target, const char *message);
typedef uint64_t (*rt_video_out_output_av_fn)(uint64_t target,
    const rt_video_out_output_native_call *call);
typedef void (*rt_video_out_output_stack_fail_fn)(uint64_t target,
    uint64_t guard_pointer);

/* Immutable descriptor per call; actual pointed storage and import cells stay
   live. All data, literals, guard pointer CELL and provider targets must belong
   to the same actual module instance. Relative tables require the original
   code/table relationship: 38, 20 and 195 signed DWORDs respectively. The error table
   holds 11 live result DWORDs. Unknown computed destinations stop explicitly;
   the source does not execute arbitrary changed code or substitute a default
   table. No initialized values, native export or provider installation. */
typedef struct {
    const void *guard_pointer_slot;
    const void *variant_07_table;
    const void *variant_06_table;
    const void *parameter_10_table;
    const void *provider_error_table;
    void *key_word;
    void *stack_word;
    const void *sdk_slot;
    const void *diagnostic_slot;
    const void *av_slot;
    const void *stack_fail_slot;
    const char *blocked_configuration_message;
    rt_video_out_output_sdk_fn invoke_sdk;
    rt_video_out_output_diagnostic_fn invoke_diagnostic;
    rt_video_out_output_av_fn invoke_av;
    rt_video_out_output_stack_fail_fn invoke_stack_fail;
} rt_video_out_output_config_bindings;

extern const rt_video_out_output_config_bindings *volatile rt_video_out_output_config_imports;

/* Six observed incoming GP prefixes: low 32-bit selector, full descriptor pointer,
   full optional pointer, low 32-bit signed level, low 32-bit flags, low 32-bit key. The last
   two arguments are OWNED transport: the first incoming stack-slot ADDRESS
   (only its low DWORD is read, late after nonnegative AV result), and an 88-byte,
   16-byte-aligned frame preserving its initial bytes. They are not two extra native
   functional arguments. Do not pass an early by-value copy of the stack word.
   Descriptor accesses cover 32 bytes, optional accesses 16 bytes when reached;
   those direct extents do not bound opaque provider footprints or lifetimes.
   Preserve live SDK spills, unconditional SDK-word read, chosen guard pointer,
   repeated field epochs, 16-byte snapshot, cached tail byte, partial/overlapping
   stores and ordered publications. Return full zero-extended 32-bit status.
   Uncompiled/unvalidated flat64 little-endian source progress. Physical native
   ABI/bridge implementation, exact emitted access/fault/concurrency behavior,
   unknown provider effects and wider native frame aliases remain unproved. */
uint64_t rt_video_out_configure_output_with_storage(uint64_t selector_bits,
    uint64_t descriptor_address, uint64_t optional_address, uint64_t level_bits,
    uint64_t flag_bits, uint64_t key_bits, const void *incoming_stack_word,
    void *frame_storage);

#ifdef __cplusplus
}
#endif
#endif
