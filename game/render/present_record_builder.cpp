// SPDX-License-Identifier: GPL-2.0-or-later
// Build one presentation record from the mode-specific normalized payload. Keep
// constructor returns as packed data, checked lower failures as status five, and
// every output publication before later failures or native division faults.
#include <bit>
#include <cstring>

#include "../engine/integer.h"
#include "present_record_builder.h"

namespace {
using namespace render;
constexpr uint8_t mode_two = 2, mode_three = 3, mode_four = 4;
constexpr uint32_t required_parameter_bit = 0x20, first_auxiliary_bit = 0x400;
constexpr uint32_t second_auxiliary_bit = 0x200, transformed_component_bit = 4;
constexpr uint32_t lower_failure = 5, shift_mask = 31, descriptor_tag = 12;
constexpr unsigned address_shift = 8, component_shift = 3, area_shift = 6, packed_shift = 32;
constexpr uint64_t transformed_component_multiplier = 0xaaaaaaab;
constexpr unsigned transformed_component_shift = 34;

struct BuilderParameters {
    uint8_t mode;
    uint32_t first;
    uint32_t second;
    uint32_t third;
    uint32_t count_override;
    uint32_t encoding;
    uint32_t classification;
    uint32_t first_kind;
    uint32_t second_kind;
    uint32_t flags;
    uint8_t normalized_mode;
};

uint32_t read_half(const uint16_t *source)
{
    uint16_t value;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

uint32_t align_word(uint32_t value, uint32_t alignment)
{
    // This is the native wrapping mask, including its zero/non-power-of-two cases.
    return (value + alignment - 1) & (uint32_t(0) - alignment);
}

uint32_t shifted_component(uint32_t value, uint32_t index)
{
    const uint32_t shifted = value >> (index & shift_mask);
    return shifted == 0 ? 1 : shifted;
}

uint32_t checked_failure(uint32_t status)
{
    record_builder_error_noop(status);
    return lower_failure;
}

uint32_t descriptor_count(const BuilderParameters &parameters)
{
    if (parameters.count_override != 0) return parameters.count_override;
    if (parameters.mode == mode_four)
        return std::bit_width(parameters.first | parameters.second | parameters.third);
    if (parameters.mode == mode_two) {
        const uint32_t count = std::bit_width(parameters.first);
        return count == 0 ? 1 : count;
    }
    return std::bit_width(parameters.first | parameters.second | uint32_t(1));
}

uint64_t construct_descriptor(RecordDescriptorPrefix *descriptor, const BuilderParameters &parameters)
{
    if (parameters.mode == mode_four)
        return initialize_triple_record_descriptor(descriptor, parameters.first, parameters.second,
            parameters.third, descriptor_count(parameters), parameters.encoding, parameters.classification);
    if (parameters.mode == mode_two) {
        const uint64_t packed = initialize_pair_record_descriptor(descriptor, parameters.first, 1,
            parameters.third, descriptor_count(parameters), parameters.encoding, parameters.classification, 0, 0);
        record_descriptor_set_high_field(descriptor, descriptor_tag);
        return packed;
    }
    const bool transformed = parameters.normalized_mode != 0
        && (parameters.flags & transformed_component_bit) != 0;
    const uint32_t third = transformed
        ? static_cast<uint32_t>((uint64_t(parameters.third) * transformed_component_multiplier)
                                >> transformed_component_shift)
        : parameters.third;
    const uint32_t level = combine_record_parameters(parameters.first_kind, parameters.second_kind,
        (parameters.flags & first_auxiliary_bit) != 0);
    return initialize_pair_record_descriptor(descriptor, parameters.first, parameters.second, third,
        descriptor_count(parameters), parameters.encoding, parameters.classification, level, transformed);
}

uint32_t build_short_mode_three(RecordConstructorPrefix *output, const BuilderParameters &parameters,
                               uint64_t base, uintptr_t range_begin, uint32_t range_parameter)
{
    uint64_t first_auxiliary = 0, second_auxiliary = 0;
    const uint32_t first_selector = choose_record_constructor_selector(parameters.first_kind, 1);
    const uint32_t second_selector = combine_record_parameters(parameters.first_kind, parameters.second_kind, 1);
    const uint64_t packed = record_constructor(output, parameters.first, parameters.second, parameters.third,
        parameters.encoding, parameters.classification, first_selector, second_selector,
        &first_auxiliary, &second_auxiliary);
    record_set_word_0c_fields(output, static_cast<uint32_t>(range_begin),
        static_cast<uint32_t>(range_begin) + range_parameter - 1);
    record_store_word_00(output, static_cast<uint32_t>(base >> address_shift));
    const uint32_t first_unit = static_cast<uint32_t>(first_auxiliary >> packed_shift);
    const uint32_t first_offset = align_word(static_cast<uint32_t>(packed), first_unit);
    record_store_word_1c(output, static_cast<uint32_t>((base + uint64_t(first_offset)) >> address_shift));
    record_set_word_10_byte_at_bit13(output, 1);

    uint32_t next = static_cast<uint32_t>(packed);
    if (first_unit != 0) next = align_word(next, first_unit) + static_cast<uint32_t>(first_auxiliary);
    const uint32_t second_unit = static_cast<uint32_t>(second_auxiliary >> packed_shift);
    const uint32_t second_offset = align_word(next, second_unit);
    record_store_word_24(output, static_cast<uint32_t>((base + uint64_t(second_offset)) >> address_shift));
    record_set_word_10_byte_at_bit14(output, 1);
    return 0;
}

uint32_t calculate_auxiliary_level(const RecordDescriptorPrefix *descriptor, uint32_t index,
                                 RecordGeometryDescriptor *level, RecordGeometryResult *geometry,
                                 uint64_t *total, uint32_t *unit)
{
    uint32_t status = initialize_record_level_descriptor(level, descriptor, index, 0);
    if (status != 0) return status;
    status = calculate_record_geometry(geometry, level);
    if (status != 0) return status;
    uint32_t first, second;
    return record_constructor_optional_first(total, unit, &first, &second,
        geometry->first_00.get(), geometry->second_04.get(), geometry->third_08.get(), 0);
}

uint32_t finish_auxiliary(RecordConstructorPrefix *output, const BuilderParameters &parameters,
                         uint32_t index, uint64_t base)
{
    RecordDescriptorPrefix current_descriptor{};
    const uint64_t packed = construct_descriptor(&current_descriptor, parameters);
    RecordDescriptorPrefix previous_descriptor{};
    construct_descriptor(&previous_descriptor, parameters);

    // Native secondary calculations reuse scratch without a geometry-result clear.
    // Each consumed component follows a checked successful calculation.
    RecordGeometryResult geometry;
    RecordGeometryDescriptor previous_level;
    uint32_t accumulated = 0, maximum_unit = 0;
    uint64_t total;
    uint32_t unit;
    for (uint32_t previous = 0; previous < index; ++previous) {
        const uint32_t status = calculate_auxiliary_level(&previous_descriptor, previous,
            &previous_level, &geometry, &total, &unit);
        if (status != 0) return checked_failure(status);
        if (unit != 0) {
            uint32_t increment = static_cast<uint32_t>(total);
            if (parameters.mode != mode_four) increment *= parameters.third;
            accumulated = align_word(accumulated, unit) + increment;
        }
        if (maximum_unit < unit) maximum_unit = unit;
    }
    RecordGeometryDescriptor current_level;
    const uint32_t status = calculate_auxiliary_level(&current_descriptor, index,
        &current_level, &geometry, &total, &unit);
    if (status != 0) return checked_failure(status);
    if (unit != 0) accumulated = align_word(accumulated, unit);
    if (maximum_unit < unit) maximum_unit = unit;
    uint32_t offset = static_cast<uint32_t>(packed);
    // Zero maximum keeps the first descriptor's packed return, narrowed only here.
    if (maximum_unit != 0) offset = accumulated + align_word(offset, maximum_unit);
    record_store_word_1c(output, static_cast<uint32_t>((base + uint64_t(offset)) >> address_shift));
    record_set_word_10_byte_at_bit13(output, 1);
    record_set_word_10_byte_at_bit14(output, 0);
    record_set_word_14_second_low5_field(output, record_read_word_14_low5(output));
    record_set_word_28_low22(output, record_read_word_08_low22(output));
    record_store_word_24(output, record_read_word_00(output));
    return 0;
}

uint32_t build_general(RecordConstructorPrefix *output, const BuilderParameters &parameters,
                       uint32_t index, uint64_t base, uintptr_t range_begin, uint32_t range_parameter)
{
    RecordGeometryResult geometry{};
    RecordDescriptorPrefix descriptor{};
    construct_descriptor(&descriptor, parameters);
    RecordGeometryDescriptor level;
    uint32_t status = initialize_record_level_descriptor(&level, &descriptor, index, 0);
    if (status != 0) return checked_failure(status);
    status = calculate_record_geometry(&geometry, &level);
    if (status != 0) return checked_failure(status);

    const bool first_enabled = (parameters.flags & first_auxiliary_bit) != 0;
    const bool any_auxiliary = first_enabled || (parameters.flags & second_auxiliary_bit) != 0;
    uint64_t auxiliary = 0;
    const uint32_t first_selector = choose_record_constructor_selector(parameters.first_kind, first_enabled);
    const uint32_t second_selector = combine_record_parameters(parameters.first_kind, parameters.second_kind, first_enabled);
    record_constructor(output, shifted_component(parameters.first, index),
        shifted_component(parameters.second, index), parameters.third, parameters.encoding,
        geometry.converted_30.get(), first_selector, second_selector,
        any_auxiliary ? &auxiliary : nullptr, nullptr);
    const uint32_t scale = reduce_record_parameter(second_selector);
    // Native DIVQ follows constructor publications; zero divisors fault without a status fallback.
    uint64_t quotient = engine::divide_u64(read_geometry_total(geometry.total_10),
                                         geometry.metadata_2c.get() >> component_shift);
    quotient = engine::divide_u64(quotient, scale);
    record_set_word_04_repeated_field(output, (geometry.first_00.get() >> component_shift) - 1);
    record_set_word_08_low22(output, static_cast<uint32_t>(quotient >> area_shift) - 1);
    record_set_word_0c_fields(output, static_cast<uint32_t>(range_begin),
        static_cast<uint32_t>(range_begin) + range_parameter - 1);

    uint64_t offset = 0;
    if (index != 0) {
        RecordDescriptorPrefix offset_descriptor{};
        construct_descriptor(&offset_descriptor, parameters);
        uint64_t ignored_total;
        status = calculate_record_descriptor_level_offset(&offset, &ignored_total, &offset_descriptor, index, 0);
        if (status != 0) return checked_failure(status);
    }
    record_store_word_00(output, static_cast<uint32_t>((base + offset) >> address_shift));
    if (!any_auxiliary) return 0;
    return finish_auxiliary(output, parameters, index, base);
}
}  // namespace

extern "C" uint32_t bb_render_build_present_source_record(
    render::PresentSourceRecord *record, render::PresentSourcePayloadView *payload,
    uint32_t selector, uint32_t index, uintptr_t range_begin, uint32_t range_parameter)
{
    using namespace render;
    const auto *prefix = reinterpret_cast<const PresentBuilderPayloadPrefix *>(payload->unknown_0x00);
    const auto *supplied = reinterpret_cast<const uint32_t *>(prefix->supplied_28.bytes);
    BuilderParameters parameters;
    switch (prefix->mode_20) {
    case mode_two: {
        // The extra trailing DWORD stays indeterminate, as in the native input scratch.
        RecordMode2Input input;
        input.component_00 = read_half(&payload->count_input_2c);
        input.count_04 = payload->record_count_32;
        input.parameter_08 = read_half(&payload->record_parameter_30);
        input.selector = selector;
        input.parameter_10 = prefix->parameter_21;
        input.parameter_14 = prefix->parameter_23;
        input.parameter_18 = prefix->parameter_22;
        input.flags = prefix->flags_24.get();
        NormalizedRecordMode2 normalized;
        record_normalize_mode2(&normalized, &input, supplied, 0);
        if (normalized.status != 0) return normalized.status;
        if ((normalized.input.parameter_14 & required_parameter_bit) == 0) return normalization_failed;
        parameters = {mode_two, normalized.input.component_00, 1, normalized.input.parameter_08,
            normalized.input.count_04, normalized.mapping, normalized.supplied, 1, 0, normalized.input.flags, 0};
        break;
    }
    case mode_three: {
        RecordMode3Input input;
        input.component_00 = read_half(&payload->count_input_2c);
        input.component_04 = read_half(&payload->count_input_2e);
        input.count_08 = payload->record_count_32;
        input.parameter_0c = read_half(&payload->record_parameter_30);
        input.selector = selector;
        input.parameter_14 = read_half(&payload->parameter_34);
        input.parameter_18 = read_half(&payload->parameter_36);
        input.parameter_1c = prefix->parameter_21;
        input.parameter_20 = prefix->parameter_23;
        input.parameter_24 = prefix->parameter_22;
        input.flags = prefix->flags_24.get();
        NormalizedRecordMode3 normalized;
        record_normalize_mode3(&normalized, &input, supplied, 0);
        if (normalized.status != 0) return normalized.status;
        if ((normalized.input.parameter_20 & required_parameter_bit) == 0) return normalization_failed;
        parameters = {mode_three, normalized.input.component_00, normalized.input.component_04,
            normalized.input.parameter_0c, normalized.input.count_08, normalized.mapping, normalized.supplied,
            normalized.input.parameter_14, normalized.input.parameter_18, normalized.input.flags, normalized.mode};
        break;
    }
    case mode_four: {
        RecordMode4Input input;
        input.component_00 = read_half(&payload->count_input_2c);
        input.component_04 = read_half(&payload->count_input_2e);
        input.component_08 = read_half(&payload->record_parameter_30);
        input.count_0c = payload->record_count_32;
        input.selector = selector;
        input.parameter_14 = prefix->parameter_21;
        input.parameter_18 = prefix->parameter_23;
        input.parameter_1c = prefix->parameter_22;
        input.flags = prefix->flags_24.get();
        NormalizedRecordMode4 normalized;
        record_normalize_mode4(&normalized, &input, supplied, 0);
        if (normalized.status != 0) return normalized.status;
        if ((normalized.input.parameter_18 & required_parameter_bit) == 0) return normalization_failed;
        parameters = {mode_four, normalized.input.component_00, normalized.input.component_04,
            normalized.input.component_08, normalized.input.count_0c, normalized.mapping, normalized.supplied,
            1, 0, normalized.input.flags, 0};
        break;
    }
    default:
        return normalization_failed;
    }
    // Cache the address only after successful normalization and required-bit admission.
    const uint64_t base = read_geometry_total(prefix->base_08);
    auto *output = reinterpret_cast<RecordConstructorPrefix *>(record->unknown_0x00);
    constexpr uint32_t first_short_kind = 2;
    if (parameters.mode == mode_three && (parameters.flags & first_auxiliary_bit) != 0
        && parameters.first_kind >= first_short_kind)
        return build_short_mode_three(output, parameters, base, range_begin, range_parameter);
    return build_general(output, parameters, index, base, range_begin, range_parameter);
}
