// SPDX-License-Identifier: GPL-2.0-or-later
// Preserve resource-info partial publication and the distinct native initial-data
// paths. Lower functions remain named originals; no service defaults are invented.
#include "swap_chain_resource_info.h"

#include <cstdio>
#include <cstdlib>

namespace {
constexpr uint32_t unsupported_status = 8;
constexpr uint32_t allocation_failed_status = 9;
constexpr uint32_t restricted_parameter = 16;
constexpr uint32_t restricted_parameter_flag = 0x400;
constexpr uint32_t first_allocator_flag = 0x4000;
constexpr uint32_t padded_allocation_flag = 0x80;
constexpr uint32_t allocation_padding = 23;
constexpr uint32_t allocation_rounding_mask = 0xfffffff8;
constexpr uint32_t minimum_alignment = 8;
constexpr uint32_t published_kind = 3;
constexpr uint32_t control_selector = 0xffffffff;

uint64_t read_guard(const uint64_t *pointer)
{
    uint64_t value;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}
uint16_t low_word(const render::ResourceInfoField<uint32_t> &field)
{
    uint16_t value;
    std::memcpy(&value, field.bytes, sizeof(value));
    return value;
}
render::ResourceInfoInitialEntry *initial_entry(render::ResourceInfoInitialEntry *base,
                                               uint32_t index)
{
    return reinterpret_cast<render::ResourceInfoInitialEntry *>(
        reinterpret_cast<uintptr_t>(base) + uint64_t(index) * sizeof(*base));
}
void zero_qword(uintptr_t address)
{
    const uint64_t zero = 0;
    std::memcpy(reinterpret_cast<void *>(address), &zero, sizeof(zero));
}
}  // namespace

extern "C" __attribute__((visibility("hidden"), noinline, used))
uint64_t bb_render_prepare_swap_chain_resource_info_body(
    render::ResourceInfoOutput *initial_output, render::ResourceInfoServices *services,
    render::ResourceInfoParameters *parameters, render::ResourceInfoInitialEntry *initial,
    render::ResourceInfoOwner *candidate_a, render::ResourceInfoOwner *candidate_b,
    const render::ResourceInfoOptionsSlot *live_options_slot)
{
    using namespace render;
    ResourceInfoFrame frame;
    frame.saved_guard.set(read_guard(resource_info_calls::guard_pointer.get()));
    const auto finish = [&](uint32_t status) -> uint64_t {
        // Native reloads the global POINTER here on every path, then reads the
        // saved frame value after the current guard load.
        const uint64_t *current_pointer = resource_info_calls::guard_pointer.get();
        const uint64_t current = read_guard(current_pointer);
        const uint64_t expected = frame.saved_guard.get();
        if (current != expected) {
            resource_info_calls::fail_guard();
            std::fputs("prepare_swap_chain_resource_info: __stack_chk_fail returned; "
                       "native continuation is unclosed\n", stderr);
            std::abort();
        }
        return uint64_t(status);
    };

    const uint32_t parameter_14 = parameters->words[5].get();
    const uint32_t secondary_flags = parameters->words[10].get();
    if (parameter_14 == restricted_parameter &&
        (secondary_flags & restricted_parameter_flag) == 0) return finish(unsupported_status);

    ResourceInfoOptions *first_options = live_options_slot->value.get();
    ResourceInfoOwner *allocator = candidate_a;
    if ((secondary_flags & first_allocator_flag) == 0) allocator = candidate_b;
    frame.owner_or_processor.set(reinterpret_cast<uintptr_t>(allocator));
    const uint64_t zero = 0;
    std::memcpy(&frame.requirements, &zero, sizeof(zero));
    void *optional_payload = nullptr;
    if (first_options != nullptr && first_options->payload_enabled.get() != 0)
        optional_payload = &first_options->payload_address;
    frame.output_index_or_destination.set(reinterpret_cast<uintptr_t>(initial_output));
    frame.services.set(services);
    frame.initial.set(initial);
    frame.parameters.set(parameters);
    const uint32_t descriptor_status = resource_info_calls::prepare_descriptor(
        &frame.requirements, &frame.descriptor, parameters, optional_payload, initial, candidate_b);
    if (descriptor_status != 0) return finish(descriptor_status);

    const uint32_t required_bytes = frame.requirements.bytes.get();
    uint32_t allocation_alignment = frame.requirements.alignment.get();
    uint32_t allocation_bytes = required_bytes;
    if (first_options != nullptr && first_options->capacity_enabled.get() != 0) {
        allocation_bytes = first_options->capacity.get();
        if (allocation_bytes < required_bytes) return finish(unsupported_status);
        const uint32_t override_alignment = first_options->alignment.get();
        if (override_alignment < allocation_alignment) return finish(unsupported_status);
        allocation_alignment = override_alignment;
    }
    ResourceInfoParameters *allocation_parameters = frame.parameters.get();
    const uint32_t allocation_flag = allocation_parameters->words[8].get() & padded_allocation_flag;
    frame.allocation_flag.set(allocation_flag);
    frame.required_bytes.set(uint64_t(required_bytes));
    if (allocation_flag != 0) {
        allocation_bytes = (allocation_bytes + allocation_padding) & allocation_rounding_mask;
        if (allocation_alignment <= minimum_alignment) allocation_alignment = minimum_alignment;
    }
    frame.alignment.set(allocation_alignment);
    const uint32_t levels = resource_info_calls::descriptor_levels(&frame.descriptor);
    const ResourceInfoAllocate allocate = allocator->vtable.get()->allocate.get();
    const uintptr_t allocation = allocate(allocator, uint64_t(allocation_bytes),
                                          uint64_t(allocation_alignment));
    if (allocation == 0) return finish(allocation_failed_status);
    frame.level_count.set(levels + 1);
    frame.levels.set(uint64_t(levels));
    if (frame.allocation_flag.get() != 0) {
        zero_qword(allocation + uint64_t(allocation_bytes) - 8);
        zero_qword(allocation + uint64_t(allocation_bytes) - 16);
    }
    auto *output = reinterpret_cast<ResourceInfoOutput *>(
        uintptr_t(frame.output_index_or_destination.get()));
    output->owner.set(allocator);
    output->allocation.set(allocation);
    output->mode.set(resource_info_calls::current_mode());
    output->required_bytes.set(uint32_t(frame.required_bytes.get()));
    output->allocation_bytes.set(allocation_bytes);
    output->alignment.set(frame.alignment.get());
    output->kind.set(published_kind);
    ResourceInfoParameters *published_parameters = frame.parameters.get();
    output->parameter_1c.set(published_parameters->words[7].bytes[0]);
    output->parameter_24.set(published_parameters->words[9].bytes[0]);
    output->flags.set(published_parameters->words[8].bytes[0]);
    output->secondary_flags.set(published_parameters->words[10].get());
    output->classification.set(resource_info_calls::descriptor_classification(&frame.descriptor));
    output->first.set(low_word(published_parameters->words[0]));
    output->second.set(low_word(published_parameters->words[1]));
    output->count.set(low_word(published_parameters->words[3]));
    output->levels.set(uint8_t(frame.level_count.get()));
    output->selector.set(published_parameters->words[4].bytes[0]);
    output->parameter_14.set(low_word(published_parameters->words[5]));
    output->parameter_18.set(low_word(published_parameters->words[6]));

    // This is a second LIVE incoming-slot read after opaque calls/publication.
    ResourceInfoOptions *current_options = live_options_slot->value.get();
    bool alternate_path = false;
    if (current_options != nullptr) alternate_path = current_options->alternate_path.get() != 0;
    ResourceInfoInitialEntry *initial_for_copy = frame.initial.get();
    if (initial_for_copy == nullptr) return finish(0);
    ResourceInfoParameters *predicate_parameters = frame.parameters.get();
    if (resource_info_calls::needs_initial_copy(predicate_parameters->words[4].get()) == 0)
        return finish(0);
    if (current_options != nullptr && current_options->span_copy_enabled.get() != 0) {
        const uintptr_t source = initial_for_copy->data.get();
        resource_info_calls::copy_initial_span(allocation, source, frame.required_bytes.get());
        return finish(0);
    }

    if (!alternate_path && resource_info_calls::storage_admitted(output->allocation.get()) != 0) {
        ResourceInfoServices *processor_services = frame.services.get();
        ResourceInfoControl *control = processor_services->control.get();
        const ResourceInfoControl18 operation = control->vtable.get()->operation_18.get();
        operation(control, uint64_t(control_selector));
        auto *processor = reinterpret_cast<ResourceInfoProcessor *>(&processor_services->processor_address);
        frame.owner_or_processor.set(reinterpret_cast<uintptr_t>(processor));
        if (frame.level_count.get() != 0) {
            ResourceInfoParameters *count_parameters = frame.parameters.get();
            uint32_t current_count = count_parameters->words[3].get();
            frame.levels.set(uint64_t(uint32_t(frame.levels.get()) + 1));
            ResourceInfoParameters *loop_parameters = frame.parameters.get();
            const uint32_t step = uint32_t(frame.levels.get());
            uint32_t outer = 0;
            for (;;) {
                frame.output_index_or_destination.set(uint64_t(outer));
                uint32_t flattened = outer;
                uint32_t inner = 0;
                if (current_count != 0) {
                    do {
                        ResourceInfoInitialEntry *entry = initial_entry(frame.initial.get(), flattened);
                        auto *current_processor = reinterpret_cast<ResourceInfoProcessor *>(
                            frame.owner_or_processor.get());
                        resource_info_calls::process_initial_entry(
                            current_processor, output, entry, uint64_t(flattened), uint64_t(0));
                        ++inner;
                        current_count = loop_parameters->words[3].get();
                        flattened += step;
                    } while (inner < current_count);
                }
                outer = uint32_t(frame.output_index_or_destination.get()) + 1;
                const bool repeat = outer != frame.level_count.get();  // Native JNE.
                processor = reinterpret_cast<ResourceInfoProcessor *>(frame.owner_or_processor.get());
                if (!repeat) break;
            }
        }
        resource_info_calls::finish_processing_first(processor);
        resource_info_calls::finish_processing_second(processor);
        ResourceInfoServices *final_services = frame.services.get();
        ResourceInfoControl *final_control = final_services->control.get();
        const ResourceInfoControl28 final_operation = final_control->vtable.get()->operation_28.get();
        final_operation(final_control);
        return finish(0);
    }

    const uint8_t copy_admission = resource_info_calls::alternate_copy_admitted(output->allocation.get());
    if (frame.level_count.get() == 0) return finish(0);
    const uint64_t previous_levels = frame.levels.get();
    frame.saved_allocation.set(allocation);
    if (copy_admission != 1) return finish(0);  // Exact native XOR AL,1 / JNE.
    ResourceInfoParameters *count_parameters = frame.parameters.get();
    uint32_t current_count = count_parameters->words[3].get();
    frame.levels.set(uint64_t(uint32_t(previous_levels) + 1));
    uint32_t level = 0;
    do {
        uint32_t flattened = level;
        uint32_t inner = 0;
        if (current_count != 0) {
            do {
                frame.offset.set(0);
                const uint32_t offset_status = resource_info_calls::calculate_level_offset(
                    &frame.offset, frame.size_slot, &frame.descriptor, level, inner);
                if (offset_status != 0) {
                    resource_info_calls::note_offset_failure(offset_status);
                    return finish(unsupported_status);  // Native does not release here.
                }
                const uintptr_t destination = uintptr_t(frame.offset.get()) + frame.saved_allocation.get();
                frame.output_index_or_destination.set(destination);
                resource_info_calls::initialize_level_descriptor(
                    &frame.temporary_descriptor, &frame.descriptor, level, inner);
                const uintptr_t source = initial_entry(frame.initial.get(), flattened)->data.get();
                const uint32_t copy_status = resource_info_calls::copy_initial_level(
                    uintptr_t(frame.output_index_or_destination.get()), source,
                    &frame.temporary_descriptor);
                if (copy_status != 0) {
                    auto *release_owner = reinterpret_cast<ResourceInfoOwner *>(frame.owner_or_processor.get());
                    const ResourceInfoRelease release = release_owner->vtable.get()->release.get();
                    release(release_owner, frame.saved_allocation.get());
                    return finish(unsupported_status);  // Published output is not cleared.
                }
                ++inner;
                ResourceInfoParameters *live_count_parameters = frame.parameters.get();
                current_count = live_count_parameters->words[3].get();
                flattened += uint32_t(frame.levels.get());
            } while (inner < current_count);
        }
        ++level;
    } while (level < frame.level_count.get());  // Native unsigned JB.
    return finish(0);
}

#if defined(__x86_64__)
extern "C" __attribute__((naked, no_stack_protector))
uint64_t bb_render_prepare_swap_chain_resource_info(
    render::ResourceInfoOutput *, render::ResourceInfoServices *, render::ResourceInfoParameters *,
    render::ResourceInfoInitialEntry *, render::ResourceInfoOwner *, render::ResourceInfoOwner *,
    render::ResourceInfoOptions *)
{
    // Entry RSP is8mod16; subtract8 aligns CALL and supplies the hidden body's
    // seventh pointer argument. The actual incoming slot remains unchanged/live.
    __asm__(
        "leaq 8(%rsp), %rax\n"
        "subq $8, %rsp\n"
        "movq %rax, (%rsp)\n"
#if defined(__APPLE__)
        "callq _bb_render_prepare_swap_chain_resource_info_body\n"
#else
        "callq bb_render_prepare_swap_chain_resource_info_body\n"
#endif
        "addq $8, %rsp\n"
        "retq\n");
}
#else
extern "C" uint64_t bb_render_prepare_swap_chain_resource_info(
    render::ResourceInfoOutput *, render::ResourceInfoServices *, render::ResourceInfoParameters *,
    render::ResourceInfoInitialEntry *, render::ResourceInfoOwner *, render::ResourceInfoOwner *,
    render::ResourceInfoOptions *)
{
    std::fputs("prepare_swap_chain_resource_info: native entry requires AMD64 System V; "
               "use an explicit live options slot on other CPUs\n", stderr);
    std::abort();
}
#endif
