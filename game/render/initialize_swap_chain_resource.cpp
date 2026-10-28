// SPDX-License-Identifier: GPL-2.0-or-later
// Build a wrapper around an original lower resource-info result. All admission
// words are kept neutral; no GPU-format or console service defaults are inferred.
#include "swap_chain_resource_init.h"

#include <cstdio>
#include <cstdlib>

namespace {
constexpr uint32_t unsupported_status = 8;
constexpr uint32_t allocation_failed_status = 9;
constexpr uint32_t primary_rejected_bits = 0x17;
constexpr uint32_t secondary_rejected_bits = 0x38;
constexpr uint32_t flag_20 = 0x20;
constexpr uint32_t flag_40 = 0x40;
constexpr uint32_t secondary_requires_40 = 0x100;
constexpr uint32_t secondary_requires_20 = 0x200;
constexpr uint32_t secondary_pair_mask = 0x8100;
constexpr uint32_t secondary_pair_rejected = 0x8000;
constexpr uint32_t low_nibble_complement = 0xf0;
constexpr uint32_t count_requirement_bit = 4;
constexpr uint32_t minimum_count = 6;
constexpr uint32_t capability_flag_maximum = 0xff;
constexpr uint32_t capability_selection_maximum = 3;
constexpr uint32_t capability_secondary_maximum = 0xffff;
constexpr uint32_t conflicting_low_bit = 4;
constexpr uint32_t conflicting_pair_a = 0x60;
constexpr uint32_t conflicting_pair_b = 0xc0;
constexpr uint32_t conflicting_pair_c = 0x90;
constexpr uint32_t object_initial_state = 1;
constexpr uint64_t object_alignment = 8;

enum class CompatibilityKind : uint32_t { zero, one, two, three };

uint32_t leading_zero_count(uint32_t value)
{
    if (value == 0) return 32;
    uint32_t count = 0;
    while ((value & 0x80000000u) == 0) {
        value <<= 1;
        ++count;
    }
    return count;
}

uint64_t read_guard(const uint64_t *pointer)
{
    uint64_t value;
    std::memcpy(&value, pointer, sizeof(value));
    return value;
}
}  // namespace

extern "C" uint64_t bb_render_initialize_swap_chain_resource(
    render::SwapResourceServices *services, render::SwapResourceOutput *output,
    render::SwapResourceParameters *parameters, render::SwapResourceInitialEntry *initial,
    render::SwapResourceOwner *allocator_override, render::SwapResourceOwner *owner_override,
    uint64_t argument)
{
    using namespace render;
    // Native captures this pointer once, before any parameter reads, and retains
    // it through every lower call. The saved guard remains part of the frame view.
    const uint64_t *guard_pointer = swap_resource_calls::guard_pointer.get();
    SwapResourceInitFrame frame;
    frame.saved_guard.set(read_guard(guard_pointer));
    const auto finish = [&](uint32_t status) -> uint64_t {
        const uint64_t current_guard = read_guard(guard_pointer);
        const uint64_t saved_guard = frame.saved_guard.get();
        if (current_guard != saved_guard) {
            swap_resource_calls::fail_guard();
            std::fputs("initialize_swap_chain_resource: __stack_chk_fail returned; "
                       "native continuation into trailing table is unclosed\n", stderr);
            std::abort();
        }
        return uint64_t(status);
    };

    uint32_t flags = parameters->flags.get();
    if ((flags & primary_rejected_bits) != 0) return finish(unsupported_status);
    const uint32_t secondary_flags = parameters->secondary_flags.get();
    if ((secondary_flags & secondary_rejected_bits) != 0) return finish(unsupported_status);

    if (initial != nullptr) {
        uint32_t count = parameters->parameter_08.get();
        if (count == 0) {
            const uint32_t first_zeroes = leading_zero_count(parameters->parameter_00.get());
            const uint32_t second_zeroes = leading_zero_count(parameters->parameter_04.get());
            uint32_t minimum_zeroes = second_zeroes < 31 ? second_zeroes : 31;
            if (first_zeroes < minimum_zeroes) minimum_zeroes = first_zeroes;
            count = 32 - minimum_zeroes;
        }
        count *= parameters->parameter_0c.get();  // Native DWORD wrap.
        uintptr_t entry_address = reinterpret_cast<uintptr_t>(initial);
        for (uint32_t index = 0; index < count; ++index) {
            auto *entry = reinterpret_cast<SwapResourceInitialEntry *>(entry_address);
            if (entry->value.get() == 0) return finish(unsupported_status);
            entry_address += sizeof(SwapResourceInitialEntry);
        }
    }
    if ((secondary_flags & count_requirement_bit) != 0 &&
        parameters->parameter_0c.get() < minimum_count) return finish(unsupported_status);
    if ((flags & flag_40) == 0 && (secondary_flags & secondary_requires_40) != 0)
        return finish(unsupported_status);
    if ((secondary_flags & secondary_pair_mask) == secondary_pair_rejected)
        return finish(unsupported_status);
    if ((flags & flag_20) == 0 && (secondary_flags & secondary_requires_20) != 0)
        return finish(unsupported_status);
    if (parameters->parameter_14.get() >= 2 && parameters->parameter_08.get() != 1)
        return finish(unsupported_status);

    // Both default-pointer reads occur before compatibility selection, including
    // routes that later reject. Capability reads are ordered and short-circuited.
    SwapResourceOwner *default_allocator = services->default_allocator.get();
    SwapResourceOwner *default_owner = services->default_owner.get();
    frame.default_owner.set(default_owner);
    bool capability = services->capability_00.get() != 0 &&
                      services->capability_08.get() != 0 &&
                      services->capability_20.get() != 0;
    frame.default_allocator.set(default_allocator);
    if (flags > capability_flag_maximum ||
        parameters->selection.get() > capability_selection_maximum ||
        secondary_flags > capability_secondary_maximum) capability = false;

    bool candidate = false;
    switch (static_cast<CompatibilityKind>(parameters->kind.get())) {
    case CompatibilityKind::zero:
        if (parameters->selection.get() != 0) capability = false;
        candidate = capability;
        break;
    case CompatibilityKind::one:
        if (initial != nullptr && parameters->selection.get() == 0 &&
            flags != 0 && (flags & low_nibble_complement) == 0) candidate = capability;
        break;
    case CompatibilityKind::two:
        if ((parameters->selection.get() & 1) != 0 && flags != 0 &&
            (flags & low_nibble_complement) == 0) {
            if ((secondary_flags & 2) != 0) capability = false;
            candidate = capability;
        }
        break;
    case CompatibilityKind::three:
        if (flags == 0) {
            const uint32_t selection = parameters->selection.get();
            flags = 0;  // Native MOV ESI,0 before the selection-zero branch.
            if (selection != 0) {
                flags = 0;  // Native XOR ESI,ESI on this route as well.
                if ((secondary_flags & 0x18) != 0) capability = false;
                candidate = capability;
            }
        }
        break;
    default:
        break;
    }

    bool rejected = true;
    if (!((flags & conflicting_low_bit) != 0 && (flags & ~conflicting_low_bit) != 0) &&
        (flags & conflicting_pair_a) != conflicting_pair_a &&
        (flags & conflicting_pair_b) != conflicting_pair_b) {
        rejected = (flags & conflicting_pair_c) == conflicting_pair_c || !candidate;
    }
    SwapResourceOwner *allocator = frame.default_allocator.get();
    if (allocator_override != nullptr) allocator = allocator_override;
    SwapResourceOwner *owner = frame.default_owner.get();
    if (owner_override != nullptr) owner = owner_override;
    if (owner == nullptr) return finish(unsupported_status);
    if (allocator == nullptr) return finish(unsupported_status);
    if (rejected) return finish(unsupported_status);

    // The original seventh incoming slot is read once here, before any opaque
    // callback. Preserve its exposed outgoing-frame word, without claiming a
    // compiler's complete physical frame or unknown lower access extents.
    frame.outgoing_argument.set(argument);
    const uint32_t status = swap_resource_calls::prepare_resource_info(
        &frame.info, services, parameters, initial, allocator, owner,
        frame.outgoing_argument.get());
    if (status != 0) return finish(status);

    SwapResourceOwnerVtable *allocator_vtable = allocator->vtable.get();
    const SwapResourceAllocate allocate = allocator_vtable->allocate.get();
    SwapResourceObject *object = allocate(allocator, sizeof(SwapResourceObject), object_alignment);
    if (object != nullptr) {
        object->vtable.set(reinterpret_cast<uintptr_t>(swap_resource_calls::initial_vtable.address()));
        object->allocator.set(allocator);
        object->state_10.set(object_initial_state);
        object->vtable.set(reinterpret_cast<uintptr_t>(swap_resource_calls::final_vtable.address()));
        object->info.value_30.set(frame.info.value_30.get());
        object->info.value_28.set(frame.info.value_28.get());
        object->info.value_20.set(frame.info.value_20.get());
        object->info.value_18.set(frame.info.value_18.get());
        object->info.value_10.set(frame.info.value_10.get());
        SwapResourceOwner *info_owner = frame.info.owner.get();
        const uintptr_t info_resource = frame.info.resource.get();
        object->info.resource.set(info_resource);
        object->info.owner.set(info_owner);
        output->object.set(object);
        return finish(0);
    }

    // Publication can alias the scratch: clear output before rereading either
    // info pointer, and keep the resource read conditional on the owner read.
    output->object.set(nullptr);
    SwapResourceOwner *info_owner = frame.info.owner.get();
    if (info_owner != nullptr) {
        const uintptr_t info_resource = frame.info.resource.get();
        if (info_resource != 0) {
            SwapResourceOwnerVtable *info_vtable = info_owner->vtable.get();
            const SwapResourceRelease release = info_vtable->release.get();
            release(info_owner, info_resource);
            frame.info.resource.set(0);
        }
    }
    return finish(allocation_failed_status);
}
