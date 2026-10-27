// SPDX-License-Identifier: GPL-2.0-or-later
// The native default callback is exactly XOR EAX,EAX; RET. Its three-byte entry
// remains unhooked. Keep the source form distinct from the stored original address.
#pragma once

#include "command_submit.h"

namespace render {
RT_ORIGINAL(0x014712b0, original_default_command_space_callback,
            uint8_t(CommandSubmissionContext *, uint32_t requested_words, uintptr_t argument));

// This useful callback profile consumes no argument and declines the capacity
// request. The native full RAX is zero; arbitrary flags/register equivalence is
// not claimed for this separate inline source form. It is never installed below.
inline uint8_t default_command_space_callback_source(CommandSubmissionContext *, uint32_t, uintptr_t)
{
    return 0;
}

inline CommandSpaceCallback original_default_command_space_callback_address()
{
    return reinterpret_cast<CommandSpaceCallback>(original_default_command_space_callback.address());
}
}  // namespace render
