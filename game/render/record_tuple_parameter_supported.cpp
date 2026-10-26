// SPDX-License-Identifier: GPL-2.0-or-later
// The tuple parameter must be a nonzero power of two. Only AL is a Boolean
// interface in the original; its higher return bits are not a status value.
#include <cstdint>

extern "C" uint8_t bb_render_record_tuple_parameter_supported(uint32_t parameter)
{
    return parameter != 0 && (parameter & (parameter - 1)) == 0;
}
