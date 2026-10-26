// SPDX-License-Identifier: GPL-2.0-or-later
// Initialize a command-buffer view without touching its auxiliary fields. Capacity is stored
// in DWORDs; the end address rounds the byte count down and wraps as an unsigned address.
// A null callback selects the original default callback and discards the supplied argument.
#include "command_submit.h"

namespace {
RT_ORIGINAL(0x014712b0, default_command_space_callback,
            void(render::CommandSubmissionContext *, uintptr_t zero, uintptr_t argument));
}  // namespace

extern "C" void bb_render_command_buffer_init(render::CommandBufferSnapshot *self,
                                               uintptr_t begin, uint32_t size,
                                               render::CommandSpaceCallback callback,
                                               uintptr_t callback_argument)
{
    self->cursor = begin;
    self->begin = begin;
    self->capacity_words = size / render::command_word_bytes;
    const uint32_t capacity_bytes = self->capacity_words * render::command_word_bytes;
    self->limit = begin + capacity_bytes;
    if (callback != nullptr) {
        self->space_callback = callback;
        self->callback_argument = callback_argument;
    } else {
        self->space_callback = reinterpret_cast<render::CommandSpaceCallback>(default_command_space_callback.address());
        self->callback_argument = 0;
    }
}
