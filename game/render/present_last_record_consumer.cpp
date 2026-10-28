// SPDX-License-Identifier: GPL-2.0-or-later
// Consume the final record range, or update its tracking bank when commands are
// not admitted. The scratch caller forwards this function's full guard value.
#include "present_last_record_consumer.h"

namespace render {
namespace {
constexpr uint8_t suppress_commands = 0x20;
constexpr uint32_t resource_range_changed = 0x2;
constexpr uint32_t all_ranges_changed = 0x7;
constexpr uint8_t all_record_banks_changed = 0xf;
constexpr uint8_t range_marker = 0x1;
constexpr uint32_t command_begin_mode = 0x2c;
constexpr uint32_t state_mode_value = 0xcc;
constexpr uint32_t pipeline_mode = 0x11;
constexpr uint32_t record_step_mode = 0x3;
constexpr uint32_t preparation_slot_count = 0x8;
constexpr uint32_t parameter_half_bits = 0x10;
constexpr uint32_t parameter_half_mask = 0xffff;
constexpr uint32_t unset_word = UINT32_MAX;
constexpr uint64_t unset_address = UINT64_MAX;

// This aggregate preserves the known relative bytes exposed to ordinary
// register/copy helpers. It is not a claim about an identical host physical
// frame, unobserved helper access extents or additional ambient registers.
struct alignas(8) LastRecordFrame {
    PresentFinishField<uint64_t> token;
    PresentFinishField<LastPresentRecordContext *> context;
    PresentFinishField<uint64_t> force_or_shader;
    PresentFinishField<LastPresentRecordView *> record;
    RecordRegisterState restore_mode;
    RecordRegisterState mode;
    RecordRegisterState register_b;
    RecordRegisterState register_c;
    LastPresentRecordView copy;
    uint8_t unknown_0x74[0x4];
    PresentFinishField<uint64_t> guard;
};
static_assert(offsetof(LastRecordFrame, token) == 0x00);
static_assert(offsetof(LastRecordFrame, context) == 0x08);
static_assert(offsetof(LastRecordFrame, force_or_shader) == 0x10);
static_assert(offsetof(LastRecordFrame, record) == 0x18);
static_assert(offsetof(LastRecordFrame, restore_mode) == 0x20);
static_assert(offsetof(LastRecordFrame, mode) == 0x28);
static_assert(offsetof(LastRecordFrame, register_b) == 0x30);
static_assert(offsetof(LastRecordFrame, register_c) == 0x38);
static_assert(offsetof(LastRecordFrame, copy) == 0x40);
static_assert(offsetof(LastRecordFrame, guard) == 0x78);
static_assert(sizeof(LastRecordFrame) == 0x80);
static_assert(sizeof(uintptr_t) == sizeof(uint64_t));

uint64_t finish_guard(const uint64_t *pointer, const LastRecordFrame &frame)
{
    const uint64_t current = read_present_finish_guard(pointer);
    // Read the saved field after the current guard; helpers have borrowed nearby
    // frame storage. An unexpectedly returning failure import is unsupported.
    check_present_finish_guard_values(current, frame.guard.get(),
                                      "render_present_last_record_consume");
    return current;
}

void track_last_record_range(LastPresentRecordContext *context,
                            const LastPresentRecordPayload *payload)
{
    const uintptr_t resource = payload->resource.get();
    const uint32_t old_flags = context->flags_b498.get();
    if (resource != 0) {
        const uint32_t extent = payload->extent_14.get();
        context->flags_b498.set(old_flags | resource_range_changed);
        const uint32_t begin = record_range_begin(resource);
        const uint32_t end = record_range_length(uintptr_t(extent)) + begin;
        const uint32_t minimum = context->minimum_b48c.get();
        context->minimum_b48c.set(minimum < begin ? minimum : begin);
        const uint32_t maximum = context->maximum_b490.get();
        context->maximum_b490.set(maximum > end ? maximum : end);
        context->marker_b495.set(context->marker_b495.get() | range_marker);
    } else {
        context->flags_b498.set(old_flags | all_ranges_changed);
    }
    context->flags_b478.set(context->flags_b478.get() | all_record_banks_changed);
}

void copy_last_record(LastPresentRecordView &destination,
                      const LastPresentRecordView *source)
{
    // The high tail is copied first. The final two reads both precede either
    // low store; the four bytes between this 52-byte copy and guard stay unknown.
    destination.parameters_30.set(source->parameters_30.get());
    destination.words[5].set(source->words[5].get());
    destination.words[4].set(source->words[4].get());
    destination.words[3].set(source->words[3].get());
    destination.words[2].set(source->words[2].get());
    const uint64_t first = source->words[0].get();
    const uint64_t second = source->words[1].get();
    destination.words[1].set(second);
    destination.words[0].set(first);
}
}  // namespace
}  // namespace render

extern "C" uint64_t bb_render_present_last_record_consume(
    render::LastPresentRecordContext *context,
    render::LastPresentRecordView *record,
    render::LastPresentRecordPayload *payload,
    uint32_t first_index, uint32_t count, uint32_t force)
{
    using namespace render;
    LastRecordFrame frame;
    // Entry stores only the low DWORD; the slot later becomes the shader pointer.
    std::memcpy(frame.force_or_shader.bytes, &force, sizeof(force));
    frame.record.set(record);
    const uint64_t *const initial_guard_pointer = present_finish_guard_pointer.get();
    frame.guard.set(read_present_finish_guard(initial_guard_pointer));

    if (last_record_calls::classify_record(frame.record.get()) == 0) {
        track_last_record_range(context, payload);
        return finish_guard(initial_guard_pointer, frame);
    }
    uint32_t saved_force;
    std::memcpy(&saved_force, frame.force_or_shader.bytes, sizeof(saved_force));
    if (uint8_t(saved_force) == 0 &&
        (payload->flags_25.get() & suppress_commands) != 0) {
        return finish_guard(initial_guard_pointer, frame);
    }

    frame.context.set(context);
    RecordCommandBuffer *const commands = &context->commands;
    frame.force_or_shader.set(reinterpret_cast<uintptr_t>(&context->shader_state));
    RecordPreparationState *const preparation = &context->preparation;
    last_record_calls::reset_preparation(preparation, context);
    begin_record_commands(commands, command_begin_mode);

    const uint32_t parameters = frame.record.get()->parameters_30.get();
    const uint32_t first_parameter = parameters & parameter_half_mask;
    const uint32_t second_parameter = parameters >> parameter_half_bits;
    const float scalar = last_record_calls::scalar_parameter.get();
    configure_record_parameters(commands, 0, 0, first_parameter, second_parameter,
                                scalar, 0.0f);
    const uint32_t enabled = last_record_calls::record_option(frame.record.get()) != 0;
    configure_record_option(commands, enabled, 0);

    initialize_record_register_c(&frame.register_c);
    initialize_record_register_b(&frame.register_b);
    emit_record_register_c(commands, frame.register_c.word);
    context->register_b5a0.set(frame.register_c.word);
    frame.token.set(record_cache_token_b.get());
    context->token_b6f0.set(frame.token.get());
    emit_record_register_b(commands, frame.register_b.word);
    context->register_b5a4.set(frame.register_b.word);
    context->token_b6f0.set(frame.token.get());

    for (uint32_t slot = 0; slot < preparation_slot_count; ++slot) {
        bind_prepared_record(preparation, commands, slot, nullptr);
    }
    emit_record_state_mode(commands, 0, state_mode_value);
    last_record_calls::initialize_mode(&frame.mode);
    last_record_calls::set_first_mode(&frame.mode, 1);
    last_record_calls::set_second_mode(&frame.mode, 1);
    last_record_calls::emit_mode(commands, frame.mode.word);

    RecordShaderState *const shader = reinterpret_cast<RecordShaderState *>(
        uintptr_t(frame.force_or_shader.get()));
    configure_record_shader(shader, commands, shared_present_descriptor.address(), 0);
    emit_record_pipeline_mode(commands, pipeline_mode);
    emit_record_enable(commands, 1);
    configure_record_shader_secondary(shader, commands, shared_present_descriptor.address(), 0);

    copy_last_record(frame.copy, frame.record.get());
    uint32_t index = first_index;
    uint32_t remaining = count;
    while (remaining != 0) {
        last_record_calls::set_record_index(&frame.copy, uintptr_t(index), index);
        last_record_calls::select_preparation(preparation, commands, &frame.copy);
        emit_present_record_step(commands, record_step_mode);
        ++index;
        --remaining;
    }

    RecordShaderState *const final_shader = reinterpret_cast<RecordShaderState *>(
        uintptr_t(frame.force_or_shader.get()));
    finish_record_shader(final_shader, commands);
    LastPresentRecordContext *const final_context = frame.context.get();
    // Native OR touches only the low byte here, unlike tracking's DWORD store.
    final_context->flags_b498.bytes[0] |= uint8_t(all_ranges_changed);
    final_context->flags_b478.set(final_context->flags_b478.get() | all_record_banks_changed);
    final_context->sentinel_b548.set(unset_address);
    final_context->flags_b550.set(final_context->flags_b550.get() | range_marker);
    final_context->sentinel_b540.set(unset_word);
    emit_record_state_mode(commands, 1, state_mode_value);
    last_record_calls::initialize_mode(&frame.restore_mode);
    last_record_calls::emit_mode(commands, frame.restore_mode.word);

    for (uint32_t slot = 0; slot < preparation_slot_count; ++slot) {
        clear_prepared_record(preparation, commands, slot);
    }
    finish_record_preparation(preparation, commands);
    final_context->mode_b520.set(pipeline_mode);
    final_context->pending_b518.set(0);
    final_context->sentinel_b524.set(unset_word);
    // Only the full command path reloads the global guard pointer at its tail.
    const uint64_t *const final_guard_pointer = present_finish_guard_pointer.get();
    return finish_guard(final_guard_pointer, frame);
}
