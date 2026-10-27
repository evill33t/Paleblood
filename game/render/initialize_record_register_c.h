// SPDX-License-Identifier: GPL-2.0-or-later
// The native seven-byte entry stays unhooked. Inline only for callers that ignore its
// ambient RAX/flags; clear exactly one DWORD and leave all trailing storage untouched.
#pragma once

#include "present_register_state.h"

namespace render {
inline void initialize_record_register_c(RecordRegisterState *state)
{
    record_register_word(state)->set(0);
}
}  // namespace render
