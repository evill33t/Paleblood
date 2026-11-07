// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the actual writers and runtime call record on owned synthetic storage.
// Original-game comparisons remain separate from these observer and guard-page tests.
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "runtime/render_calls.h"
#include "../present_parameter_pairs.h"
#include "../viewport_observers.h"

extern "C" void bb_render_configure_viewport(render::PacketBuffer *, uint32_t,
    render::RecordConfigurationFloat3 *, render::RecordConfigurationFloat3 *, uint32_t, float, float);
extern "C" void bb_render_configure_screen_scissor(render::PacketBuffer *, uint32_t, uint32_t, uint32_t, uint32_t);

namespace {
unsigned checks, scenarios;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); std::exit(1); \
} } while (0)

constexpr std::size_t storage_bytes = 512;
constexpr uint32_t viewport_words = 12;
#if !defined(PB_RENDER_OBSERVER_ABSENT)
constexpr uint32_t scissor_words = 4;
constexpr uint32_t scissor_tl = 0xa00c;
constexpr uint32_t scissor_br = 0xa00d;
#endif
constexpr std::size_t source_words = render::viewport_axes * 2;
constexpr std::size_t depth_packet_bytes = sizeof(render::RecordParameterPairPacket);
constexpr std::size_t initial_offset = depth_packet_bytes * 2;
constexpr std::size_t transform_offset = initial_offset + depth_packet_bytes;
constexpr std::size_t callback_offset = initial_offset * 2;
constexpr unsigned mapped_pages = 2;
constexpr uint32_t negative_zero = 0x80000000, quiet_nan = 0x7fc54321;
constexpr uint8_t accepted = 0x80, packet_fill = 0x6b, replacement_fill = 0x7c;
constexpr uint32_t depth_header = 0xc0026900, transform_header = 0xc0066900;
constexpr std::array<uint32_t, source_words> initial_sources{
    0x7fc12345, 0x80000000, 0x3f800000, 0xffc54321, 0, 0xbf800000};
constexpr std::array<uint32_t, source_words> callback_sources{
    0x7fa45678, 0x40000000, 0x80000000, 0x3f000000, 0xffc98765, 0x40800000};
constexpr uint32_t negative_left = 0xffff8000, negative_top = 0x8000ffff;
constexpr uint32_t maximum_right = 0xaaaa4000, maximum_bottom = 0xbbbb4000;
#if !defined(PB_RENDER_OBSERVER_ABSENT)
constexpr std::size_t translation_input_offset = initial_offset + sizeof(render::RecordConfigurationFloat3);
constexpr std::size_t callback_transform_offset = callback_offset + depth_packet_bytes;
constexpr std::size_t depth_cursor_alias = offsetof(render::PacketBuffer, cursor) -
    offsetof(render::RecordParameterPairPacket, first);
constexpr std::size_t scissor_cursor_alias = offsetof(render::PacketBuffer, cursor) -
    offsetof(render::RecordParameterPairPacket, second);
constexpr unsigned pointer_high_shift = 32;
constexpr unsigned z_axis = 2;
constexpr unsigned scissor_register_count = 2;
constexpr uint32_t packed_negative_left_top = 0xffff8000, packed_maximum_right_bottom = 0x40004000;
constexpr uint32_t high_left_bits = 0xffff0001, high_right_bits = 0x80000002, coordinate_mask = 0xffff;
constexpr uint32_t alias_x = 1, alias_y = 2, alias_right = 9, alias_bottom = 7;
constexpr uint32_t published_alias_bottom_right = (alias_bottom << 16 | alias_right) + depth_packet_bytes;
#endif
constexpr uint32_t viewport_index = 3;
constexpr uint32_t unused_argument = 0x89abcdef;

uint32_t word_at(const void *source)
{
    uint32_t value;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

float float_word(uint32_t word)
{
    float value;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

template <typename T> void prefix_field(render::PacketBuffer *commands, std::size_t offset, T value)
{
    std::memcpy(reinterpret_cast<uint8_t *>(commands) + offset, &value, sizeof(value));
}

struct Fixture {
    alignas(uintptr_t) std::array<uint8_t, storage_bytes> prefix{}, packet{}, replacement{};
    render::RecordConfigurationFloat3 scale{}, translation{};
    std::array<uint32_t, source_words> sources = initial_sources;
    std::array<uint32_t, source_words> changed = callback_sources;
    uint32_t minimum = negative_zero, maximum = quiet_nan;
    uint32_t requested = viewport_words;
    uint8_t callback_result = accepted;
    bool change_storage{}, change_sources{};

    render::PacketBuffer *commands() { return reinterpret_cast<render::PacketBuffer *>(prefix.data()); }
    uintptr_t begin() const { return reinterpret_cast<uintptr_t>(packet.data()); }

    void inputs(const std::array<uint32_t, source_words> &words)
    {
        std::memcpy(&scale, words.data(), sizeof(scale));
        std::memcpy(&translation, words.data() + render::viewport_axes, sizeof(translation));
    }

    void storage(uintptr_t start, uintptr_t cursor, uintptr_t limit)
    {
        prefix_field(commands(), offsetof(render::PacketBuffer, begin), start);
        prefix_field(commands(), offsetof(render::PacketBuffer, cursor), cursor);
        prefix_field(commands(), offsetof(render::PacketBuffer, limit), limit);
    }

    Fixture()
    {
        packet.fill(packet_fill); replacement.fill(replacement_fill);
        inputs(sources);
        storage(begin(), begin() + initial_offset, begin() + packet.size());
    }

    void viewport(render::RecordConfigurationFloat3 *scale_input = nullptr,
                  render::RecordConfigurationFloat3 *translation_input = nullptr)
    {
        bb_render_configure_viewport(commands(), viewport_index, scale_input ? scale_input : &scale,
            translation_input ? translation_input : &translation, unused_argument,
            float_word(minimum), float_word(maximum));
    }

    void scissor(uint32_t x = negative_left, uint32_t y = negative_top,
                 uint32_t right = maximum_right, uint32_t bottom = maximum_bottom)
    {
        bb_render_configure_screen_scissor(commands(), x, y, right, bottom);
    }
};

uint8_t capacity(render::PacketBuffer *commands, uint32_t requested, uintptr_t argument)
{
    auto &fixture = *reinterpret_cast<Fixture *>(argument);
    CHECK(requested == fixture.requested);
    if (fixture.change_storage) {
        const uintptr_t start = reinterpret_cast<uintptr_t>(fixture.replacement.data());
        prefix_field(commands, offsetof(render::PacketBuffer, begin), start);
        prefix_field(commands, offsetof(render::PacketBuffer, cursor), start + callback_offset);
    }
    if (fixture.change_sources) fixture.inputs(fixture.changed);
    return fixture.callback_result;
}

void callback(Fixture &fixture)
{
    const uintptr_t cursor = render::packet_cursor(fixture.commands());
    prefix_field(fixture.commands(), offsetof(render::PacketBuffer, limit), cursor);
    prefix_field(fixture.commands(), offsetof(render::PacketBuffer, space_callback),
        render::PacketSpaceCallback(capacity));
    prefix_field(fixture.commands(), offsetof(render::PacketBuffer, callback_argument),
        reinterpret_cast<uintptr_t>(&fixture));
}

void headless_prefix_not_read()
{
    if (rt_render_observer_available()) return;
    ++scenarios;
    const long page = sysconf(_SC_PAGESIZE);
    CHECK(page > 0);
    auto *allocation = static_cast<uint8_t *>(mmap(nullptr, std::size_t(page) * mapped_pages,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
    CHECK(allocation != MAP_FAILED);
    CHECK(mprotect(allocation, std::size_t(page), PROT_NONE) == 0);
    auto *commands = reinterpret_cast<render::PacketBuffer *>(allocation + page - sizeof(uintptr_t));
    Fixture fixture;
    prefix_field(commands, offsetof(render::PacketBuffer, cursor), fixture.begin());
    prefix_field(commands, offsetof(render::PacketBuffer, limit), fixture.begin() + fixture.packet.size());
    bb_render_configure_viewport(commands, 0, &fixture.scale, &fixture.translation,
        unused_argument, float_word(fixture.minimum), float_word(fixture.maximum));
    CHECK(word_at(fixture.packet.data()) == depth_header);
    CHECK(word_at(fixture.packet.data() + depth_packet_bytes) == transform_header);
    bb_render_configure_screen_scissor(commands, 0, 0, 1, 1);
    CHECK(word_at(fixture.packet.data() + transform_offset) == depth_header);
#if !defined(PB_RENDER_OBSERVER_ABSENT)
    std::size_t count;
    rt_render_submitted(fixture.begin(), uint32_t(fixture.packet.size()), &count);
    CHECK(count == 0);
#endif
    CHECK(munmap(allocation, std::size_t(page) * mapped_pages) == 0);
    ++scenarios;
    render::record_viewport_observation({}, nullptr, nullptr);
    render::record_screen_scissor_observation({}, nullptr);
    CHECK(!rt_render_observer_available());
    Fixture redirected; redirected.change_storage = true; callback(redirected); redirected.viewport();
    CHECK(word_at(redirected.replacement.data() + callback_offset) == depth_header);
}

#if !defined(PB_RENDER_OBSERVER_ABSENT)
void enabled()
{
    CHECK(rt_render_observer_available());
    rt_render_forget();
}

const rt_render_call &viewport_record(uintptr_t buffer, uint32_t offset,
    const render::RecordParameterPairPacket *depth, const render::RecordParameterTransformPacket *transform)
{
    std::size_t count;
    const auto *calls = rt_render_submitted(buffer, offset + viewport_words * render::packet_word_bytes, &count);
    CHECK(count == 1);
    CHECK(calls && calls[0].kind == RT_CALL_VIEWPORT);
    CHECK(calls[0].offset == offset);
    CHECK(calls[0].u.viewport.index == viewport_index);
    const auto &call = calls[0].u.viewport;
    CHECK(word_at(&call.zmin) == depth->first.get());
    CHECK(word_at(&call.zmax) == depth->second.get());
    CHECK(word_at(&call.scale[0]) == transform->x_scale.get());
    CHECK(word_at(&call.scale[1]) == transform->y_scale.get());
    CHECK(word_at(&call.scale[z_axis]) == transform->z_scale.get());
    CHECK(word_at(&call.translate[0]) == transform->x_offset.get());
    CHECK(word_at(&call.translate[1]) == transform->y_offset.get());
    CHECK(word_at(&call.translate[z_axis]) == transform->z_offset.get());
    return calls[0];
}

void scissor_record(uintptr_t buffer, uint32_t offset, const render::RecordParameterPairPacket *packet)
{
    std::size_t count;
    const auto *calls = rt_render_submitted(buffer, offset + scissor_words * render::packet_word_bytes, &count);
    CHECK(count == scissor_register_count);
    CHECK(calls && calls[0].kind == RT_CALL_REGISTER && calls[1].kind == RT_CALL_REGISTER);
    CHECK(calls[0].offset == offset && calls[1].offset == offset);
    CHECK(calls[0].u.reg.address == RT_CONTEXT_REGISTERS_FIRST + packet->index.get() &&
        calls[1].u.reg.address == RT_CONTEXT_REGISTERS_FIRST + packet->index.get() + 1);
    CHECK(calls[0].u.reg.value == packet->first.get());
    CHECK(calls[1].u.reg.value == packet->second.get());
}

void viewport_bits_and_input_alias()
{
    ++scenarios; enabled(); Fixture fixture;
    fixture.viewport();
    const auto *depth = reinterpret_cast<const render::RecordParameterPairPacket *>(
        fixture.packet.data() + initial_offset);
    const auto *transform = reinterpret_cast<const render::RecordParameterTransformPacket *>(
        fixture.packet.data() + transform_offset);
    const auto &call = viewport_record(fixture.begin(), initial_offset, depth, transform);
    CHECK(word_at(&call.u.viewport.zmin) == fixture.minimum);
    CHECK(word_at(&call.u.viewport.scale[0]) == fixture.sources[0]);
    CHECK(word_at(&call.u.viewport.translate[0]) == fixture.sources[render::viewport_axes]);

    ++scenarios; enabled();
    std::memcpy(fixture.packet.data() + initial_offset, fixture.sources.data(), sizeof(fixture.sources));
    fixture.storage(fixture.begin(), fixture.begin() + initial_offset, fixture.begin() + fixture.packet.size());
    fixture.viewport(reinterpret_cast<render::RecordConfigurationFloat3 *>(fixture.packet.data() + initial_offset),
        reinterpret_cast<render::RecordConfigurationFloat3 *>(fixture.packet.data() + translation_input_offset));
    const auto &aliased = viewport_record(fixture.begin(), initial_offset, depth, transform);
    CHECK(word_at(&aliased.u.viewport.scale[0]) == fixture.sources[0]);
    CHECK(word_at(&aliased.u.viewport.translate[0]) == fixture.sources[render::viewport_axes]);
}

void callback_changes_and_declines()
{
    ++scenarios; enabled(); Fixture fixture;
    fixture.change_storage = fixture.change_sources = true; callback(fixture); fixture.viewport();
    const uintptr_t start = reinterpret_cast<uintptr_t>(fixture.replacement.data());
    const auto *depth = reinterpret_cast<const render::RecordParameterPairPacket *>(start + callback_offset);
    const auto *transform = reinterpret_cast<const render::RecordParameterTransformPacket *>(
        start + callback_transform_offset);
    const auto &call = viewport_record(start, callback_offset, depth, transform);
    CHECK(word_at(&call.u.viewport.scale[0]) == fixture.changed[0]);
    CHECK(word_at(&call.u.viewport.translate[1]) == fixture.changed[render::viewport_axes + 1]);

    ++scenarios; enabled(); Fixture declined;
    declined.change_storage = declined.change_sources = true; declined.callback_result = 0; callback(declined);
    const auto untouched = declined.replacement; declined.viewport();
    std::size_t count;
    rt_render_submitted(reinterpret_cast<uintptr_t>(declined.replacement.data()), uint32_t(storage_bytes), &count);
    CHECK(count == 0 && declined.replacement == untouched);

    ++scenarios; enabled(); Fixture scissor;
    scissor.requested = scissor_words; scissor.change_storage = true; callback(scissor); scissor.scissor();
    scissor_record(reinterpret_cast<uintptr_t>(scissor.replacement.data()), callback_offset,
        reinterpret_cast<const render::RecordParameterPairPacket *>(scissor.replacement.data() + callback_offset));
    ++scenarios; enabled(); Fixture rejected;
    rejected.requested = scissor_words; rejected.change_storage = true; rejected.callback_result = 0;
    callback(rejected);
    const auto original = rejected.replacement; rejected.scissor();
    rt_render_submitted(reinterpret_cast<uintptr_t>(rejected.replacement.data()), uint32_t(storage_bytes), &count);
    CHECK(count == 0 && rejected.replacement == original);
}

void final_viewport_payload_aliases()
{
    ++scenarios; enabled(); Fixture depth_alias;
    const uintptr_t prefix = reinterpret_cast<uintptr_t>(depth_alias.prefix.data());
    const uintptr_t target = reinterpret_cast<uintptr_t>(depth_alias.replacement.data()) + depth_packet_bytes;
    depth_alias.minimum = uint32_t(target); depth_alias.maximum = uint32_t(target >> pointer_high_shift);
    depth_alias.storage(prefix, prefix + depth_cursor_alias, prefix + storage_bytes); depth_alias.viewport();
    const auto *depth = reinterpret_cast<const render::RecordParameterPairPacket *>(prefix + depth_cursor_alias);
    const auto *transform = reinterpret_cast<const render::RecordParameterTransformPacket *>(
        target + depth_packet_bytes);
    CHECK(depth->first.get() != depth_alias.minimum);
    viewport_record(prefix, depth_cursor_alias, depth, transform);


}

void viewport_header_publication_alias()
{
    enabled(); Fixture metadata;
    const uintptr_t start = reinterpret_cast<uintptr_t>(metadata.prefix.data());
    metadata.storage(start, start, start + storage_bytes); metadata.viewport();


}

void overlapping_viewport_packets()
{
    enabled(); Fixture transform_alias;
    const uintptr_t owner = reinterpret_cast<uintptr_t>(transform_alias.prefix.data());
    transform_alias.minimum = uint32_t(owner - depth_cursor_alias);
    transform_alias.maximum = uint32_t((owner - depth_cursor_alias) >> pointer_high_shift);
    transform_alias.storage(owner, owner + depth_cursor_alias, owner + storage_bytes); transform_alias.viewport();
    const auto *overlap = reinterpret_cast<const render::RecordParameterTransformPacket *>(owner + depth_cursor_alias);
    (void)overlap;
}

void final_scissor_payload_and_raw_words()
{
    ++scenarios; enabled(); Fixture fixture; fixture.scissor();
    const auto *packet = reinterpret_cast<const render::RecordParameterPairPacket *>(
        fixture.packet.data() + initial_offset);
    scissor_record(fixture.begin(), initial_offset, packet);
    CHECK(RT_CONTEXT_REGISTERS_FIRST + packet->index.get() == scissor_tl && scissor_br == scissor_tl + 1);
    CHECK(packet->first.get() == packed_negative_left_top && packet->second.get() == packed_maximum_right_bottom);

    ++scenarios; enabled(); Fixture high_bits;
    high_bits.scissor(high_left_bits, 0, high_right_bits, 0);
    packet = reinterpret_cast<const render::RecordParameterPairPacket *>(high_bits.packet.data() + initial_offset);
    CHECK(packet->first.get() == (high_left_bits & coordinate_mask) &&
        packet->second.get() == (high_right_bits & coordinate_mask));
    scissor_record(high_bits.begin(), initial_offset, packet);

    ++scenarios; enabled(); Fixture alias;
    const uintptr_t start = reinterpret_cast<uintptr_t>(alias.prefix.data());
    alias.storage(start, start + scissor_cursor_alias, start + storage_bytes);
    alias.scissor(alias_x, alias_y, alias_right, alias_bottom);
    packet = reinterpret_cast<const render::RecordParameterPairPacket *>(start + scissor_cursor_alias);
    CHECK(packet->second.get() == published_alias_bottom_right);
    scissor_record(start, scissor_cursor_alias, packet);
}

void final_scissor_register_alias()
{
    ++scenarios; enabled(); Fixture alias;
    const uintptr_t start = reinterpret_cast<uintptr_t>(alias.prefix.data());
    constexpr std::size_t index_alias = offsetof(render::PacketBuffer, cursor) -
        offsetof(render::RecordParameterPairPacket, index);
    alias.storage(start, start + index_alias, start + storage_bytes);
    alias.scissor(alias_x, alias_y, alias_right, alias_bottom);
    const auto *packet = reinterpret_cast<const render::RecordParameterPairPacket *>(start + index_alias);
    constexpr uint32_t published_index = 0x1c;
    CHECK(packet->index.get() == published_index);
    scissor_record(start, index_alias, packet);
}

void expect_abort(void (*action)())
{
    constexpr int redirection_failed = 2;
    ++scenarios;
    const pid_t child = fork(); CHECK(child >= 0);
    if (child == 0) {
        const rlimit no_core{0, 0}; setrlimit(RLIMIT_CORE, &no_core);
        if (!std::freopen("/dev/null", "w", stderr)) std::_Exit(redirection_failed);
        action();
        std::_Exit(0);
    }
    int status; CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

void below_begin()
{
    enabled(); Fixture fixture;
    prefix_field(fixture.commands(), offsetof(render::PacketBuffer, begin), fixture.begin() + callback_offset);
    render::observe_packet(fixture.commands(), fixture.begin() + initial_offset);
}

void oversized_offset()
{
    enabled(); Fixture fixture;
    render::observe_packet(fixture.commands(), fixture.begin() + uint64_t(UINT32_MAX) + 1);
}

void rejected_viewport()
{
    enabled(); Fixture fixture;
    bb_render_configure_viewport(fixture.commands(), RT_VIEWPORTS, &fixture.scale, &fixture.translation,
        unused_argument, float_word(fixture.minimum), float_word(fixture.maximum));
}
void invalid_scissor_header()
{
    enabled(); Fixture fixture;
    render::RecordParameterPairPacket packet{};
    constexpr uint32_t nop_header = 0xc0001000;
    packet.header.set(nop_header); packet.index.set(scissor_tl - RT_CONTEXT_REGISTERS_FIRST);
    render::record_screen_scissor_observation({fixture.begin(), 0, true}, &packet);
}

void invalid_scissor_span()
{
    enabled(); Fixture fixture;
    render::RecordParameterPairPacket packet{};
    packet.header.set(depth_header);
    packet.index.set(RT_CONTEXT_REGISTERS_LAST - RT_CONTEXT_REGISTERS_FIRST);
    render::record_screen_scissor_observation({fixture.begin(), 0, true}, &packet);
}

void scissor_context_index_masquerades_as_uconfig()
{
    enabled(); Fixture fixture;
    render::RecordParameterPairPacket packet{};
    packet.header.set(depth_header);
    packet.index.set(RT_UCONFIG_REGISTERS_FIRST - RT_CONTEXT_REGISTERS_FIRST);
    render::record_screen_scissor_observation({fixture.begin(), 0, true}, &packet);
}

void invalid_viewport_header()
{
    enabled(); Fixture fixture;
    render::RecordParameterPairPacket depth{};
    render::RecordParameterTransformPacket transform{};
    constexpr uint32_t nop_header = 0xc0001000, viewport_depth_first = 0xb4, viewport_transform_first = 0x10f;
    depth.header.set(nop_header); depth.index.set(viewport_depth_first);
    transform.header.set(transform_header); transform.index.set(viewport_transform_first);
    render::record_viewport_observation({fixture.begin(), 0, true}, &depth, &transform);
}

void disagreeing_viewport_selection()
{
    enabled(); Fixture fixture;
    render::RecordParameterPairPacket depth{};
    render::RecordParameterTransformPacket transform{};
    constexpr uint32_t viewport_depth_first = 0xb4, viewport_transform_second = 0x115;
    depth.header.set(depth_header); depth.index.set(viewport_depth_first);
    transform.header.set(transform_header); transform.index.set(viewport_transform_second);
    render::record_viewport_observation({fixture.begin(), 0, true}, &depth, &transform);
}

#endif
}  // namespace

int main()
{
    headless_prefix_not_read();
#if !defined(PB_RENDER_OBSERVER_ABSENT)
    viewport_bits_and_input_alias();
    callback_changes_and_declines();
    final_viewport_payload_aliases();
    final_scissor_payload_and_raw_words();
    final_scissor_register_alias();
    expect_abort(below_begin); expect_abort(oversized_offset);
    expect_abort(rejected_viewport);
    expect_abort(overlapping_viewport_packets);
    expect_abort(viewport_header_publication_alias);
    expect_abort(invalid_scissor_header);
    expect_abort(invalid_scissor_span);
    expect_abort(scissor_context_index_masquerades_as_uconfig);
    expect_abort(disagreeing_viewport_selection);
    expect_abort(invalid_viewport_header);
    rt_render_forget();
#endif
    std::printf("viewport/scissor observers: %u scenarios, %u checks, zero failures\n", scenarios, checks);
    return 0;
}
