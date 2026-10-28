// SPDX-License-Identifier: GPL-2.0-or-later
// Establish the observed swap-chain storage/VideoOut route. All lower resource
// and platform operations remain original, including partial-publication errors.
#include "swap_chain_setup.h"
#include "../engine/scalar.h"

namespace render {
namespace {
constexpr uint32_t admitted_selector = 0x57;
constexpr uint32_t construction_failed = 5;
constexpr uint32_t unsupported_setup = 8;
constexpr uint32_t allocation_failed = 9;
constexpr uint32_t tiled_unit = 8;
constexpr uint32_t high_word_shift = 32;
constexpr uint32_t encoded_address_shift = 8;
constexpr uint32_t video_format = 0x80000000;
constexpr uint32_t video_handle_sentinel = 0x80290001;
constexpr uint32_t setup_flag_first = 0x20;
constexpr uint32_t setup_flag_second = 0x08;
constexpr uint32_t setup_flag_third = 0x80;
constexpr uint32_t setup_flag_shift_first = 5;
constexpr uint32_t setup_flag_shift_second = 2;
constexpr uint32_t state_info_words = 9;
constexpr uint32_t cached_record_byte3 = 0x20;
constexpr uint64_t curve_plane_bytes = setup_curve_entries * sizeof(float);
constexpr uint64_t curve_storage_bytes = curve_plane_bytes * 3;
constexpr uint64_t allocation_tail_bytes = 8;

struct SetupFrameSlot {
    uint8_t bytes[8];
    void set_u64(uint64_t v) { std::memcpy(bytes, &v, sizeof(v)); }
    uint64_t get_u64() const { uint64_t v; std::memcpy(&v, bytes, sizeof(v)); return v; }
    void set_u32(uint32_t v) { std::memcpy(bytes, &v, sizeof(v)); }
    uint32_t get_u32() const { uint32_t v; std::memcpy(&v, bytes, sizeof(v)); return v; }
};
struct SetupOutgoingSlots { SetupFrameSlot slots[4]; };
struct SetupFrameWord {
    uint8_t bytes[4];
    void set(uint32_t v) { std::memcpy(bytes, &v, sizeof(v)); }
    uint32_t get() const { uint32_t v; std::memcpy(&v, bytes, sizeof(v)); return v; }
};
// Base corresponds to native RBP-0x1c0. Unknown bytes are not initialized. The
// aggregate preserves exposed relative storage through the guard; host alignment
// padding, saved registers and a complete physical stack frame are unclaimed.
struct alignas(16) SetupFrame {
    SetupOutgoingSlots outgoing;
    PresentFinishField<uint64_t> packed_quantity;
    PresentFinishField<SetupObjectSlot *> output_slot;
    PresentFinishField<SetupOwner *> manager;
    SetupFrameWord mapped_or_tiled;
    SetupFrameWord second_or_stride;
    SetupFrameSlot first_or_storage;
    SetupFrameSlot resource_output_or_status;
    PresentFinishField<SetupServices *> services;
    PresentFinishField<SetupState *> state;
    SetupTemporaryAllocator temporary;
    uint8_t shared_parameters[0x30];
    SetupVideoAttribute video_attribute;
    PresentFinishField<uint32_t> unit;
    PresentFinishField<uint32_t> mapped_selector;
    uint8_t unknown_0xd4[4];
    PresentFinishField<uint64_t> object_scratch;
    PresentFinishField<uint64_t> addresses[16];
    SetupRecord descriptor;
    uint8_t unknown_0x18c[4];
    PresentFinishField<uint64_t> guard;
};
static_assert(offsetof(SetupFrame, outgoing) == 0x00);
static_assert(offsetof(SetupFrame, packed_quantity) == 0x20);
static_assert(offsetof(SetupFrame, output_slot) == 0x28);
static_assert(offsetof(SetupFrame, manager) == 0x30);
static_assert(offsetof(SetupFrame, mapped_or_tiled) == 0x38);
static_assert(offsetof(SetupFrame, second_or_stride) == 0x3c);
static_assert(offsetof(SetupFrame, first_or_storage) == 0x40);
static_assert(offsetof(SetupFrame, resource_output_or_status) == 0x48);
static_assert(offsetof(SetupFrame, services) == 0x50);
static_assert(offsetof(SetupFrame, state) == 0x58);
static_assert(offsetof(SetupFrame, temporary) == 0x60);
static_assert(offsetof(SetupFrame, shared_parameters) == 0x70);
static_assert(offsetof(SetupFrame, video_attribute) == 0xa0);
static_assert(offsetof(SetupFrame, unit) == 0xcc);
static_assert(offsetof(SetupFrame, mapped_selector) == 0xd0);
static_assert(offsetof(SetupFrame, object_scratch) == 0xd8);
static_assert(offsetof(SetupFrame, addresses) == 0xe0);
static_assert(offsetof(SetupFrame, descriptor) == 0x160);
static_assert(offsetof(SetupFrame, guard) == 0x190);
static_assert(sizeof(uintptr_t) == sizeof(uint64_t));

struct SetupInfoWords { PresentFinishField<uint64_t> words[state_info_words]; };
struct SetupCurveHeap {
    PresentFinishField<float> first[setup_curve_entries];
    PresentFinishField<float> second[setup_curve_entries];
    PresentFinishField<float> third[setup_curve_entries];
};
static_assert(sizeof(SetupInfoWords) == sizeof(SetupInfo));
static_assert(offsetof(SetupCurveHeap, second) == 0x1004);
static_assert(offsetof(SetupCurveHeap, third) == 0x2008);
static_assert(sizeof(SetupCurveHeap) == 0x300c);

SetupRecord *record_at(SetupState *state, uint32_t index)
{
    // Native uses a zero-extended DWORD index and a full64 product by44. No
    // count bound is invented; invalid/out-of-storage counts remain unsupported.
    const uintptr_t address = reinterpret_cast<uintptr_t>(state) +
        uint64_t(index) * sizeof(SetupRecord);
    return reinterpret_cast<SetupRecord *>(address);
}

PresentFinishField<uint64_t> *address_at(SetupFrame &frame, uint32_t index)
{
    // The native count is not bounded to the nominal sixteen address slots.
    // Keep integer addressing into the known relative frame instead of adding
    // an array-bound check or silently allocating unrelated storage.
    const uintptr_t address = reinterpret_cast<uintptr_t>(&frame.addresses[0]) +
        uint64_t(index) * sizeof(frame.addresses[0]);
    return reinterpret_cast<PresentFinishField<uint64_t> *>(address);
}

uint16_t read_low_word(const PresentFinishField<uint32_t> &field)
{
    uint16_t value;
    std::memcpy(&value, field.bytes, sizeof(value));
    return value;
}

void release_owned(SetupOwner *owner, uintptr_t allocation)
{
    SetupOwnerVtable *const vtable = owner->vtable.get();
    const SetupRelease release = vtable->release_70.get();
    release(owner, allocation);
}

uint64_t checked_status(const uint64_t *guard_pointer, const SetupFrame &frame,
                        uint32_t status)
{
    const uint64_t current = read_present_finish_guard(guard_pointer);
    check_present_finish_guard_values(current, frame.guard.get(), "render_setup_swap_chain");
    return uint64_t(status);
}

void copy_record(SetupRecord *destination, const SetupRecord &source)
{
    destination->tail_28.set(source.tail_28.get());
    destination->words[4].set(source.words[4].get());
    destination->words[3].set(source.words[3].get());
    destination->words[2].set(source.words[2].get());
    const uint64_t first = source.words[0].get();
    const uint64_t second = source.words[1].get();
    destination->words[1].set(second);
    destination->words[0].set(first);
}

enum class QueueClear { captured_state_slot, frame_before_delete, frame_after_delete };

uint64_t cleanup_video_setup(SetupState *state, SetupFrame &frame,
                             const uint64_t *guard_pointer, bool refresh_guard,
                             QueueClear queue_clear, bool capture_manager_early)
{
    swap_setup_calls::clear_curve(&state->curve);
    swap_setup_calls::clear_cached_record(&state->cached_record);
    const uint64_t queue = state->event_queue.get();
    PresentFinishField<uint64_t> *queue_slot = &state->event_queue;
    if (queue_clear == QueueClear::frame_before_delete) {
        queue_slot = reinterpret_cast<PresentFinishField<uint64_t> *>(
            uintptr_t(frame.first_or_storage.get_u64()));
    }
    // Only VideoOutOpen's failure reloads here, after both lower cleanups and
    // the queue load. Other VideoOut failures preserve the earlier pointer.
    if (refresh_guard) guard_pointer = present_finish_guard_pointer.get();
    if (queue != 0) {
        swap_setup_calls::delete_event_queue(queue);
        if (queue_clear == QueueClear::frame_after_delete) {
            queue_slot = reinterpret_cast<PresentFinishField<uint64_t> *>(
                uintptr_t(frame.first_or_storage.get_u64()));
        }
        queue_slot->set(0);
    }
    const int32_t handle = state->video_handle.get();
    SetupObjectSlot *const output = frame.output_slot.get();
    if (uint32_t(handle) != video_handle_sentinel) {
        swap_setup_calls::video_close(handle);
        state->video_handle.set(int32_t(video_handle_sentinel));
    }
    SetupOwner *const allocator = state->allocator_2c8.get();
    // RegisterBuffers failure captures this owner here, before storage release;
    // earlier failures reload it only after destroying the output object.
    SetupOwner *const captured_manager = capture_manager_early ? frame.manager.get() : nullptr;
    if (allocator != nullptr) {
        const uintptr_t storage = state->storage_2d0.get();
        if (storage != 0) {
            release_owned(allocator, storage);
            state->storage_2d0.set(0);
        }
    }
    SetupObject *const object = output->value.get();
    if (object != nullptr) {
        swap_setup_calls::destroy_object(object);
        SetupOwner *const manager = capture_manager_early ? captured_manager : frame.manager.get();
        release_owned(manager, reinterpret_cast<uintptr_t>(object));
    }
    // Native does not clear the borrowed output-object slot after release.
    return checked_status(guard_pointer, frame, construction_failed);
}
}  // namespace
}  // namespace render

extern "C" __attribute__((visibility("hidden"), noinline, used))
uint64_t bb_render_setup_swap_chain_body(
    render::SetupState *initial_state, render::SetupObjectSlot *output,
    render::SetupResourceSlot *resource_output, render::SetupServices *services,
    render::SetupInfo *info,
    render::SetupOwner *manager, const render::SetupOwnerSlot *live_allocator_slot)
{
    using namespace render;
    SetupFrame frame;
    const uint64_t *const initial_guard_pointer = present_finish_guard_pointer.get();
    frame.guard.set(read_present_finish_guard(initial_guard_pointer));
    if (info->selector_10.get() != admitted_selector) {
        return checked_status(initial_guard_pointer, frame, unsupported_setup);
    }
    frame.resource_output_or_status.set_u64(reinterpret_cast<uintptr_t>(resource_output));
    frame.state.set(initial_state);
    frame.services.set(services);
    SetupObject *const object = swap_setup_calls::create_object(1, 1, manager, services, info, manager);
    if (object == nullptr) {
        output->value.set(nullptr);
        return checked_status(initial_guard_pointer, frame, allocation_failed);
    }
    swap_setup_calls::initialize_object(object, &frame.object_scratch);
    output->value.set(object);
    const uint32_t selector_status = swap_setup_calls::map_selector(
        &frame.mapped_selector, info->selector_10.get());
    if (selector_status != 0) return checked_status(initial_guard_pointer, frame, selector_status);
    const uint32_t unit_status = swap_setup_calls::resolve_unit(
        &frame.unit, 1, frame.mapped_selector.get(), 1);
    if (unit_status != 0) {
        swap_setup_calls::note_unit_failure(unit_status);
        return checked_status(initial_guard_pointer, frame, unsupported_setup);
    }

    frame.output_slot.set(output);
    frame.manager.set(manager);
    const uint32_t unit = frame.unit.get();
    const uint8_t is_tiled = unit == tiled_unit;
    frame.first_or_storage.set_u32(info->first.get());
    frame.second_or_stride.set(info->second.get());
    frame.mapped_or_tiled.set(frame.mapped_selector.get());
    const uint32_t first = swap_setup_calls::first_parameter(info->parameter_1c.get(), 0);
    const uint32_t second = swap_setup_calls::second_parameter(
        info->parameter_1c.get(), info->parameter_20.get(), 0);
    frame.outgoing.slots[1].set_u32(second);
    frame.outgoing.slots[0].set_u32(first);
    frame.outgoing.slots[3].set_u64(0);
    frame.outgoing.slots[2].set_u64(0);
    const uint64_t packed = swap_setup_calls::construct_record(
        &frame.descriptor, frame.first_or_storage.get_u32(), frame.second_or_stride.get(),
        1, frame.mapped_or_tiled.get(), unit,
        frame.outgoing.slots[0].get_u32(), frame.outgoing.slots[1].get_u32(),
        frame.outgoing.slots[2].get_u64(), frame.outgoing.slots[3].get_u64());
    if (uint32_t(packed) == 0) {
        return checked_status(present_finish_guard_pointer.get(), frame, construction_failed);
    }
    frame.mapped_or_tiled.bytes[0] = is_tiled;
    const bool has_records = info->buffer_count.get() != 0;
    SetupOwner *const allocation_manager = frame.manager.get();
    SetupState *const template_destination = frame.state.get();
    uint32_t count = 0;
    if (has_records) {
        uint32_t index = 0;
        do {
            copy_record(record_at(template_destination, index), frame.descriptor);
            ++index;
            count = info->buffer_count.get();
        } while (index < count);
    }

    // First live read of the original seventh-argument stack slot.
    SetupOwner *const allocation_owner = live_allocator_slot->value.get();
    const uint32_t alignment = uint32_t(packed >> high_word_shift);
    const uint32_t rounding_sum = uint32_t(packed) + alignment - 1;
    const uint32_t rounded = rounding_sum & (uint32_t(0) - alignment);
    frame.packed_quantity.set(packed);
    frame.second_or_stride.set(rounded);
    const uint32_t allocation_bytes = rounded * count + uint32_t(allocation_tail_bytes);
    const SetupAllocate58 allocate = allocation_owner->vtable.get()->allocate_58.get();
    const uintptr_t storage = allocate(allocation_owner, uint64_t(allocation_bytes),
                                      uint64_t(alignment), uint64_t(rounding_sum));
    if (storage == 0) {
        SetupObject *const existing = frame.output_slot.get()->value.get();
        if (existing != nullptr) {
            swap_setup_calls::destroy_object(existing);
            release_owned(allocation_manager, reinterpret_cast<uintptr_t>(existing));
        }
        return checked_status(present_finish_guard_pointer.get(), frame, allocation_failed);
    }
    frame.first_or_storage.set_u64(storage);
    if (swap_setup_calls::storage_admitted(storage) == 0) {
        release_owned(allocation_owner, uintptr_t(frame.first_or_storage.get_u64()));
        SetupObject *const existing = frame.output_slot.get()->value.get();
        if (existing != nullptr) {
            swap_setup_calls::destroy_object(existing);
            release_owned(allocation_manager, reinterpret_cast<uintptr_t>(existing));
        }
        return checked_status(present_finish_guard_pointer.get(), frame, unsupported_setup);
    }
    swap_setup_calls::fill_storage(uintptr_t(frame.first_or_storage.get_u64()), 0,
                                  uint64_t(allocation_bytes));
    const bool has_address_records = info->buffer_count.get() != 0;
    uint32_t stride = frame.second_or_stride.get();
    if (has_address_records) {
        uint32_t index = 0;
        uint32_t offset = 0;
        do {
            const uintptr_t address = uintptr_t(frame.first_or_storage.get_u64()) + uint64_t(offset);
            const uint32_t saved_stride = stride;
            const uint32_t encoded = swap_setup_calls::encode_address(address);
            swap_setup_calls::set_record_address(record_at(frame.state.get(), index), uintptr_t(encoded));
            stride = saved_stride;
            ++index;
            offset += stride;
        } while (index < info->buffer_count.get());
    }

    SetupState *const state = frame.state.get();
    state->manager_2c0.set(frame.manager.get());
    state->allocator_2c8.set(live_allocator_slot->value.get());
    const uintptr_t captured_storage = uintptr_t(frame.first_or_storage.get_u64());
    state->storage_2d0.set(captured_storage);
    state->service_2d8.set(swap_setup_calls::storage_service(frame.services.get()));
    state->zero_2e0.set(0);
    state->last_2e8.set(captured_storage + uint64_t(allocation_bytes) - allocation_tail_bytes);
    state->video_handle.set(int32_t(video_handle_sentinel));
    PresentFinishField<uint64_t> *const queue_slot = &state->event_queue;
    queue_slot->set(0);
    auto *info_words = reinterpret_cast<SetupInfoWords *>(info);
    for (uint32_t index = 0; index < state_info_words; ++index) {
        state->info_copy[index].set(info_words->words[index].get());
    }
    const int32_t handle = swap_setup_calls::video_open(0, 0, 0, 0);
    state->video_handle.set(handle);
    if (handle < 0) {
        return cleanup_video_setup(state, frame, nullptr, true,
                                   QueueClear::captured_state_slot, false);
    }
    const int32_t flip_status = swap_setup_calls::video_flip_rate(handle, 0);
    const uint64_t *const video_guard_pointer = present_finish_guard_pointer.get();
    if (flip_status < 0) {
        return cleanup_video_setup(state, frame, video_guard_pointer, false,
                                   QueueClear::captured_state_slot, false);
    }
    const int32_t queue_status = swap_setup_calls::create_event_queue(
        queue_slot, swap_setup_calls::queue_name.address()->bytes);
    if (queue_status < 0) {
        return cleanup_video_setup(state, frame, video_guard_pointer, false,
                                   QueueClear::captured_state_slot, false);
    }
    frame.first_or_storage.set_u64(reinterpret_cast<uintptr_t>(queue_slot));
    const uint64_t queue = state->event_queue.get();
    const int32_t event_handle = state->video_handle.get();
    const int32_t event_status = swap_setup_calls::add_flip_event(queue, event_handle, 0);
    if (event_status < 0) {
        return cleanup_video_setup(state, frame, video_guard_pointer, false,
                                   QueueClear::frame_before_delete, false);
    }
    const uint32_t tiled = frame.mapped_or_tiled.bytes[0];
    const uint32_t attribute_first = info->first.get();
    const uint32_t attribute_second = info->second.get();
    frame.outgoing.slots[0].set_u32(attribute_first);
    swap_setup_calls::set_video_attribute(&frame.video_attribute, video_format, tiled, 0,
                                         attribute_first, attribute_second,
                                         frame.outgoing.slots[0].get_u32());
    uint32_t registered_count = 0;
    if (info->buffer_count.get() != 0) {
        uint32_t index = 0;
        do {
            const uint32_t encoded = swap_setup_calls::get_record_address(record_at(state, index));
            address_at(frame, index)->set(uint64_t(encoded) << encoded_address_shift);
            ++index;
            registered_count = info->buffer_count.get();
        } while (index < registered_count);
    }
    const int32_t register_handle = state->video_handle.get();
    const int32_t register_status = swap_setup_calls::register_video_buffers(
        register_handle, 0, &frame.addresses[0], registered_count, &frame.video_attribute);
    if (register_status < 0) {
        return cleanup_video_setup(state, frame, video_guard_pointer, false,
                                   QueueClear::frame_after_delete, true);
    }

    const uint32_t flags = info->flags_24.get();
    uint32_t selected = info->override_3c.get();
    const uint32_t shifted = flags << setup_flag_shift_first;
    const uint32_t mapped_flags = (shifted & setup_flag_first) |
        ((flags << setup_flag_shift_second) & setup_flag_second) | (shifted & setup_flag_third);
    if (selected == 0) selected = info->selector_10.get();
    auto *parameters = reinterpret_cast<SetupParameters *>(&frame.shared_parameters);
    parameters->words[0].set(info->first.get());
    parameters->words[1].set(info->second.get());
    parameters->words[2].set(1);
    parameters->words[3].set(1);
    parameters->words[4].set(selected);
    parameters->words[5].set(1);
    parameters->words[6].set(0);
    parameters->words[7].set(0);
    parameters->words[8].set(mapped_flags);
    parameters->words[9].set(0);
    parameters->words[10].set(info->parameter_40.get());
    parameters->words[11].set(0);
    frame.outgoing.slots[0].set_u64(0);
    SetupServices *const cached_services = frame.services.get();
    SetupResourceSlot *const cached_output = reinterpret_cast<SetupResourceSlot *>(
        uintptr_t(frame.resource_output_or_status.get_u64()));
    SetupOwner *const cached_manager = frame.manager.get();
    SetupOwner *const init_allocator = live_allocator_slot->value.get();
    const uint32_t cached_status = swap_setup_calls::initialize_cached_record(
        cached_services, cached_output, parameters, nullptr, cached_manager, init_allocator,
        frame.outgoing.slots[0].get_u64());
    if (cached_status != 0) {
        // This failure deliberately has no generalized rollback.
        return checked_status(video_guard_pointer, frame, cached_status);
    }
    frame.resource_output_or_status.set_u32(construction_failed);
    SetupCachedRecord &cached = state->cached_record;
    cached.zero_00.set(0);
    cached.storage_08.set(state->storage_2d0.get());
    cached.mode_10.set(swap_setup_calls::current_storage_mode());
    cached.quantity_14.set(uint32_t(frame.packed_quantity.get()));
    cached.bytes_20[0].set(3);
    cached.bytes_20[1].set(0);
    cached.bytes_20[2].set(0);
    cached.bytes_20[3].set(cached_record_byte3);
    cached.zero_24.set(0);
    cached.unit_28.set(frame.unit.get());
    cached.first_2c.set(read_low_word(info->first));
    cached.second_2e.set(read_low_word(info->second));
    cached.one_30.set(1);
    cached.one_32.set(1);
    cached.selector_33.set(info->selector_10.bytes[0]);
    cached.parameter_34.set(read_low_word(info->parameter_1c));
    cached.parameter_36.set(read_low_word(info->parameter_20));

    const void *const temporary_vtable = &swap_setup_calls::temporary_vtable.address()->address_point;
    frame.temporary.vtable.set(temporary_vtable);
    frame.temporary.token.set(swap_setup_calls::capture_upload_token());
    uintptr_t upload = swap_setup_calls::allocate_upload(&frame.temporary, curve_storage_bytes);
    SetupOwner *fallback_owner = nullptr;
    if (cached_manager != nullptr && upload == 0) {
        const SetupAllocate fallback = cached_manager->vtable.get()->allocate_50.get();
        upload = fallback(cached_manager, curve_storage_bytes);
        fallback_owner = cached_manager;
    }
    bool failed = false;
    if (upload == 0) {
        frame.resource_output_or_status.set_u32(allocation_failed);
        failed = true;
    } else {
        const uintptr_t second_plane = upload + curve_plane_bytes;
        const uintptr_t third_plane = upload + curve_plane_bytes + curve_plane_bytes;
        auto *heap = reinterpret_cast<SetupCurveHeap *>(upload);
        const float multiplier = swap_setup_calls::curve_multiplier.get();
        for (uint32_t index = 0; index < setup_curve_entries; ++index) {
            const float value = engine::multiply(engine::u64_to_float(uint64_t(index)), multiplier);
            state->curve.triplets[index][0].set(value);
            state->curve.triplets[index][1].set(value);
            state->curve.triplets[index][2].set(value);
            heap->first[index].set(value);
            heap->second[index].set(value);
            heap->third[index].set(value);
        }
        auto *channels = reinterpret_cast<SetupChannel *>(&frame.shared_parameters);
        channels[0].data.set(upload);
        channels[0].bytes.set(uint32_t(curve_plane_bytes));
        channels[0].zero.set(0);
        channels[1].data.set(second_plane);
        channels[1].bytes.set(uint32_t(curve_plane_bytes));
        channels[1].zero.set(0);
        channels[2].data.set(third_plane);
        channels[2].bytes.set(uint32_t(curve_plane_bytes));
        channels[2].zero.set(0);
        const uintptr_t curve_service = swap_setup_calls::curve_service(frame.services.get());
        frame.outgoing.slots[0].set_u64(0);
        const uint32_t curve_status = swap_setup_calls::initialize_curve(
            &state->curve, curve_service, swap_setup_calls::curve_settings.address(), channels,
            cached_manager, live_allocator_slot->value.get(), frame.outgoing.slots[0].get_u64());
        failed = curve_status != 0;
        if (failed) frame.resource_output_or_status.set_u32(curve_status);
        if (fallback_owner != nullptr) release_owned(fallback_owner, upload);
    }
    frame.temporary.vtable.set(temporary_vtable);
    swap_setup_calls::release_upload_token(frame.temporary.token.get());
    const uint32_t saved_final_status = frame.resource_output_or_status.get_u32();
    const uint32_t final_status = failed ? saved_final_status : 0;
    return checked_status(present_finish_guard_pointer.get(), frame, final_status);
}

#if defined(__x86_64__)
extern "C" __attribute__((naked, no_stack_protector))
uint64_t bb_render_setup_swap_chain(
    render::SetupState *, render::SetupObjectSlot *, render::SetupResourceSlot *,
    render::SetupServices *, render::SetupInfo *, render::SetupOwner *, render::SetupOwner *)
{
    // One live outgoing pointer argument: original entry RSP is8mod16, subtract8
    // aligns the CALL. Original argument7 at entryRSP+8 is never copied/changed.
    __asm__(
        "leaq 8(%rsp), %rax\n"
        "subq $8, %rsp\n"
        "movq %rax, (%rsp)\n"
#if defined(__APPLE__)
        "callq _bb_render_setup_swap_chain_body\n"
#else
        "callq bb_render_setup_swap_chain_body\n"
#endif
        "addq $8, %rsp\n"
        "retq\n");
}
#else
extern "C" uint64_t bb_render_setup_swap_chain(
    render::SetupState *, render::SetupObjectSlot *, render::SetupResourceSlot *,
    render::SetupServices *, render::SetupInfo *, render::SetupOwner *, render::SetupOwner *)
{
    std::fputs("render swap-chain setup: native entry requires AMD64 System V; "
               "live argument7 storage and engine arithmetic need a platform boundary\n", stderr);
    std::abort();
}
#endif
