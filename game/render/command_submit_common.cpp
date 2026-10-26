// SPDX-License-Identifier: GPL-2.0-or-later
// Shared native submission boundary: compact the nonempty command regions and preserve their
// order. The original ignores both native return values, then always calls submit-done after
// a nonempty submission. It makes neither call when both regions are empty.
#include "command_submit.h"

namespace render {
namespace {

RT_ORIGINAL(0x01489a20, gnm_submit_and_flip,
            int32_t(uint32_t count, const uint64_t *addresses, const uint32_t *sizes,
                    const uint64_t *constant_addresses, const uint32_t *constant_sizes,
                    uint64_t handle_word, uint32_t buffer_index, uint32_t mode, uint64_t flip_arg));
RT_ORIGINAL(0x01489b50, gnm_submit_done, int32_t());
constexpr std::size_t maximum_command_regions = 2;
constexpr unsigned buffer_index_shift = 32;
constexpr uint32_t command_size_mask = ~(command_word_bytes - 1);

uint32_t command_region_size(const CommandBufferSnapshot &region)
{
    // The original subtracts only the low 32 bits and rounds down to a DWORD boundary.
    return (static_cast<uint32_t>(region.cursor) - static_cast<uint32_t>(region.begin)) & command_size_mask;
}

}  // namespace

void submit_command_regions(const CommandBufferSnapshot &first, const CommandBufferSnapshot &second,
                            const FlipSubmission *submission, SubmitHandleArgument handle_argument)
{
    uint64_t addresses[maximum_command_regions];
    uint32_t sizes[maximum_command_regions];
    uint32_t count = 0;
    const uint32_t first_size = command_region_size(first);
    const uint32_t second_size = command_region_size(second);
    if (first_size != 0) {
        addresses[count] = first.begin;
        sizes[count++] = first_size;
    }
    if (second_size != 0) {
        addresses[count] = second.begin;
        sizes[count++] = second_size;
    }
    if (count == 0) return;
    // The empty path does not access the flip record at all; it may be null there.
    uint64_t handle_word = submission->video_out;
    if (handle_argument == SubmitHandleArgument::packed_video_and_index)
        handle_word |= static_cast<uint64_t>(submission->buffer_index) << buffer_index_shift;
    gnm_submit_and_flip(count, addresses, sizes, nullptr, nullptr, handle_word,
                       submission->buffer_index, submission->mode, submission->flip_arg);
    gnm_submit_done();
}

}  // namespace render
