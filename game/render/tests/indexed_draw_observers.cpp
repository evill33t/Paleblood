// SPDX-License-Identifier: GPL-2.0-or-later
// Owned query callbacks and packets exercise the loaded indexed writers without an executable.
#include "../command_flags.h"
#include "../draw_index.h"
#include "runtime/guest.h"
#include "runtime/render_calls.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>


namespace {
constexpr size_t storage_words = 128;
constexpr uint32_t count = 3;
constexpr uint32_t index_offset = 7;
constexpr uint64_t address = 0xabcdef1212345679;
constexpr uint64_t emitted_address = 0xabcdef1212345678;
constexpr uint32_t direct_header = 0xc0042700;
constexpr uint32_t offset_header = 0xc0033500;
constexpr uint32_t event_header = 0xc0004600;
constexpr uint32_t nop_header = 0xc0001000;
constexpr uint32_t marker = 0x35;
constexpr size_t initial_offset = 32;
constexpr size_t redirect_offset = 64;
constexpr uint32_t direct_words = 6;
constexpr uint32_t offset_words = 5;
constexpr uint32_t tail_words = 2;
constexpr uint32_t word_bytes = sizeof(uint32_t);
constexpr uint32_t query_address = 0x01489c40;
constexpr uintptr_t callback_argument = 0x123456789abcdef0;
constexpr uint64_t high_word_mask = 0xffffffff00000000;
constexpr size_t prefix_offset = 64;
constexpr uint32_t accepted = 0x80;
struct Commands { render::PacketBuffer packet; render::Field<uint32_t> flags; };
static_assert(offsetof(Commands, flags) == sizeof(render::PacketBuffer));
using Direct = void (*)(render::PacketBuffer *, uint32_t, uint64_t);
using Offset = void (*)(render::PacketBuffer *, uint32_t, uint32_t);
Direct draw_direct;
Offset draw_offset;
int failures, checks;
unsigned query_calls, callback_calls;
uint8_t query_result, callback_result;
render::PacketBuffer *active_commands;
uintptr_t queried_cursor, redirect_cursor;
uint32_t redirected_flags, requested_words;
bool query_redirect;
bool rewrite_payload, relative_active;
uintptr_t payload_pointer;
constexpr uint32_t modified_max_size = 0x11223344;
constexpr uint32_t modified_count = 0x88776655;
constexpr uint32_t modified_initiator = 0x101;
constexpr uint32_t modified_offset = 0x99aabbcc;
constexpr uint64_t modified_address = 0xaabbccddeeff0011;
constexpr uint32_t word_bits = 32;
std::array<uint32_t, storage_words> tail_storage;
std::array<uint32_t, storage_words> callback_storage;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)

uint8_t owned_query()
{
    ++query_calls;
    queried_cursor = render::packet_cursor(active_commands);
    if (rewrite_payload) {
        if (relative_active) {
            auto *packet = reinterpret_cast<render::DrawIndexedOffsetPacket *>(payload_pointer);
            packet->header.set(offset_header | 1);
            packet->max_size.set(modified_max_size);
            packet->index_offset.set(modified_offset);
            packet->index_count.set(modified_count);
            packet->initiator.set(modified_initiator);
        } else {
            auto *packet = reinterpret_cast<render::DrawIndexedPacket *>(payload_pointer);
            packet->header.set(direct_header | 1);
            packet->max_size.set(modified_max_size);
            packet->base_low.set(static_cast<uint32_t>(modified_address));
            packet->base_high.set(static_cast<uint32_t>(modified_address >> word_bits));
            packet->index_count.set(modified_count);
            packet->initiator.set(modified_initiator);
        }
    }
    if (query_redirect) {
        render::set_packet_cursor(active_commands, redirect_cursor);
        auto *flags = reinterpret_cast<render::CommandPacketFlags *>(active_commands);
        flags->flags.set(redirected_flags);
    }
    return query_result;
}

uint8_t reserve(render::PacketBuffer *commands, uint32_t words, uintptr_t argument)
{
    ++callback_calls;
    CHECK(words == requested_words && argument == callback_argument);
    commands->begin = reinterpret_cast<uintptr_t>(callback_storage.data());
    commands->cursor = commands->begin + redirect_offset;
    commands->limit = commands->cursor; // Accepted storage is not checked a second time.
    reinterpret_cast<render::CommandPacketFlags *>(commands)->flags.set(1);
    return callback_result;
}

void invoke(bool relative, render::PacketBuffer *commands)
{
    active_commands = commands;
    relative_active = relative;
    query_calls = callback_calls = 0;
    if (relative) draw_offset(commands, index_offset, count);
    else draw_direct(commands, count, address);
}

Commands setup(void *storage, bool relative, uint32_t flags = 0)
{
    const uintptr_t begin = reinterpret_cast<uintptr_t>(storage);
    const uint32_t words = (relative ? offset_words : direct_words) + tail_words;
    Commands commands{{begin, begin + initial_offset + words * word_bytes, begin + initial_offset, reserve, callback_argument}, {}};
    commands.flags.set(flags);
    return commands;
}

const rt_render_call *call_at(uint64_t buffer, size_t expected = 1)
{
    size_t found{};
    const auto *calls = rt_render_submitted(buffer, UINT32_MAX, &found);
    CHECK(found == expected);
    return found == expected ? calls : nullptr;
}

void ordinary()
{
    std::array<uint32_t, storage_words> storage{};
    for (bool relative : {false, true}) for (uint8_t result : {uint8_t{0}, uint8_t{accepted}}) {
        query_result = result;
        query_redirect = false;
        auto commands = setup(storage.data(), relative, 1);
        const uintptr_t initial = commands.packet.cursor;
        const uint32_t first_words = relative ? offset_words : direct_words;
        rt_render_forget();
        invoke(relative, &commands.packet);
        CHECK(query_calls == 1 && callback_calls == 0);
        CHECK(queried_cursor == initial + first_words * word_bytes);
        CHECK(commands.packet.cursor == initial + (first_words + tail_words) * word_bytes);
        CHECK(storage[initial_offset / word_bytes] == ((relative ? offset_header : direct_header) | 1));
        CHECK(storage[initial_offset / word_bytes + first_words] == (result ? event_header : nop_header));
        CHECK(storage[initial_offset / word_bytes + first_words + 1] == (result ? marker : 0));
        const auto *call = call_at(commands.packet.begin, 1);
        if (call) {
            CHECK(call->offset == initial_offset);
            if (relative) {
                CHECK(call->kind == RT_CALL_DRAW_INDEX_OFFSET && call->u.draw_index.header == (offset_header | 1));
                CHECK(call->u.draw_index.max_size == count && call->u.draw_index.count == count);
                CHECK(call->u.draw_index.first == index_offset && call->u.draw_index.initiator == 0);
            } else {
                CHECK(call->kind == RT_CALL_DRAW_INDEXED && call->u.draw_indexed.header == (direct_header | 1));
                CHECK(call->u.draw_indexed.max_size == count && call->u.draw_indexed.index_count == count);
                CHECK(call->u.draw_indexed.address == emitted_address && call->u.draw_indexed.initiator == 0);
            }
        }
    }
}

void callback_paths()
{
    std::array<uint32_t, storage_words> storage{};
    query_redirect = false;
    query_result = 1;
    for (bool relative : {false, true}) for (uint8_t result : {uint8_t{0}, uint8_t{accepted}}) {
        auto commands = setup(storage.data(), relative);
        commands.packet.limit = commands.packet.cursor;
        requested_words = (relative ? offset_words : direct_words) + tail_words;
        callback_result = result;
        rt_render_forget();
        invoke(relative, &commands.packet);
        CHECK(callback_calls == 1 && query_calls == (result ? 1u : 0u));
        const auto *call = call_at(reinterpret_cast<uintptr_t>(callback_storage.data()), result ? 1 : 0);
        if (result && call) CHECK(call->offset == redirect_offset);
    }
}

void query_effects()
{
    std::array<uint32_t, storage_words> storage{};
    for (bool relative : {false, true}) {
        auto commands = setup(storage.data(), relative, 1);
        const uintptr_t begin = commands.packet.begin;
        query_result = accepted;
        query_redirect = true;
        redirected_flags = tail_words;
        redirect_cursor = reinterpret_cast<uintptr_t>(tail_storage.data());
        rt_render_forget();
        invoke(relative, &commands.packet);
        CHECK(tail_storage[0] == (event_header | redirected_flags) && tail_storage[1] == marker);
        CHECK(commands.packet.cursor == redirect_cursor + tail_words * word_bytes);
        const auto *call = call_at(begin);
        if (call) CHECK(call->offset == initial_offset);
        // The query makes its tail overwrite the first draw's MAX_SIZE and address/offset word.
        commands = setup(storage.data(), relative, 1);
        redirect_cursor = commands.packet.cursor + word_bytes;
        rt_render_forget();
        invoke(relative, &commands.packet);
        call = call_at(begin);
        if (call && relative) {
            CHECK(call->u.draw_index.max_size == (event_header | redirected_flags));
            CHECK(call->u.draw_index.first == marker);
        } else if (call) {
            const uint64_t overwritten_address = 0xabcdef1200000035;
            CHECK(call->u.draw_indexed.max_size == (event_header | redirected_flags));
            CHECK(call->u.draw_indexed.address == overwritten_address); // Keep the overwritten low bit too.
        }
    }
    query_redirect = false;
}

void query_payload_effects()
{
    std::array<uint32_t, storage_words> storage{};
    query_redirect = false;
    query_result = accepted;
    rewrite_payload = true;
    for (bool relative : {false, true}) {
        auto commands = setup(storage.data(), relative);
        payload_pointer = commands.packet.cursor;
        rt_render_forget();
        invoke(relative, &commands.packet);
        const auto *call = call_at(commands.packet.begin);
        if (call && relative) {
            CHECK(call->u.draw_index.header == (offset_header | 1));
            CHECK(call->u.draw_index.max_size == modified_max_size);
            CHECK(call->u.draw_index.first == modified_offset);
            CHECK(call->u.draw_index.count == modified_count && call->u.draw_index.initiator == modified_initiator);
        } else if (call) {
            CHECK(call->u.draw_indexed.header == (direct_header | 1));
            CHECK(call->u.draw_indexed.max_size == modified_max_size);
            CHECK(call->u.draw_indexed.address == modified_address);
            CHECK(call->u.draw_indexed.index_count == modified_count && call->u.draw_indexed.initiator == modified_initiator);
        }
    }
    rewrite_payload = false;
}

void prefix_aliases()
{
    std::array<uint8_t, storage_words * word_bytes> bytes{};
    for (bool relative : {false, true}) {
        auto *commands = reinterpret_cast<Commands *>(bytes.data() + prefix_offset);
        const uintptr_t start = reinterpret_cast<uintptr_t>(commands) - (relative ? 0 : word_bytes);
        const uintptr_t begin = reinterpret_cast<uintptr_t>(bytes.data());
        commands->packet = {begin, begin + bytes.size(), start, reserve, callback_argument};
        commands->flags.set(0);
        query_redirect = true;
        query_result = accepted;
        redirected_flags = tail_words;
        redirect_cursor = reinterpret_cast<uintptr_t>(tail_storage.data());
        rt_render_forget();
        invoke(relative, &commands->packet);
        const uint32_t first_words = relative ? offset_words : direct_words;
        CHECK(queried_cursor == (start & high_word_mask) + first_words * word_bytes);
        CHECK(commands->packet.cursor == redirect_cursor + tail_words * word_bytes);
        const auto *call = call_at(begin);
        if (call) {
            CHECK(call->offset == start - begin);
            const uint32_t final_initiator = static_cast<uint32_t>(redirect_cursor + tail_words * word_bytes);
            CHECK(relative ? call->u.draw_index.initiator == final_initiator :
                             call->u.draw_indexed.initiator == final_initiator);
        }
    }
    query_redirect = false;
}

void tail_aliases()
{
    constexpr std::array<size_t, 2> offsets{12, 16};
    constexpr uint32_t high_shift = 32;
    constexpr uint32_t shader_flag = 2;
    std::array<uint8_t, storage_words * word_bytes> bytes{};
    query_redirect = false;
    for (bool relative : {false, true}) for (size_t offset : offsets) for (uint8_t result : {uint8_t{0}, uint8_t{accepted}}) {
        const uint32_t first_words = relative ? offset_words : direct_words;
        auto *commands = reinterpret_cast<Commands *>(bytes.data() + prefix_offset);
        const uintptr_t begin = reinterpret_cast<uintptr_t>(bytes.data());
        const uintptr_t tail = reinterpret_cast<uintptr_t>(commands) + offset;
        const uintptr_t start = tail - first_words * word_bytes;
        commands->packet = {begin, begin + bytes.size(), start, reserve, callback_argument};
        commands->flags.set(shader_flag);
        query_result = result;
        rt_render_forget();
        invoke(relative, &commands->packet);
        CHECK(query_calls == 1 && queried_cursor == tail);
        uintptr_t expected = tail + tail_words * word_bytes;
        if (result) {
            const uintptr_t after_pair = offset == offsets[0] ? (tail & high_word_mask) | marker :
                                         static_cast<uintptr_t>(marker) << high_shift | (event_header | shader_flag);
            expected = after_pair + tail_words * word_bytes;
        }
        CHECK(commands->packet.cursor == expected);
        const auto *call = call_at(begin);
        if (call) CHECK(call->offset == start - begin);
    }
}

} // namespace

int main(int argc, char **argv)
{
    constexpr int argument_count = 2;
    if (argc != argument_count) return 1;
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
    draw_direct = reinterpret_cast<Direct>(dlsym(library, "bb_render_draw_index"));
    draw_offset = reinterpret_cast<Offset>(dlsym(library, "bb_render_draw_index_offset"));
    auto **image = static_cast<unsigned char **>(dlsym(library, "rt_image"));
    CHECK(draw_direct && draw_offset && image);
    if (draw_direct && draw_offset && image) {
        unsigned char *saved = *image;
        // Bind only this test target's original-query declaration to an owned native function.
        const uintptr_t query_bias = query_address - RT_EBOOT_BASE;
        *image = reinterpret_cast<unsigned char *>(reinterpret_cast<uintptr_t>(&owned_query) - query_bias);
        ordinary();
        callback_paths();
        query_effects();
        query_payload_effects();
        prefix_aliases();
        tail_aliases();
        *image = saved;
    }
    rt_render_forget();
    dlclose(library);
    std::printf("indexed-draw observers: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
