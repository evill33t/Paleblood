// SPDX-License-Identifier: GPL-2.0-or-later
// Test-owned query/copy providers exercise the loaded writer without loading an executable.
#include "../command_flags.h"
#include "../indexed_draw.h"
#include "runtime/guest.h"
#include "runtime/render_calls.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>


namespace {
constexpr size_t storage_size = 512;
constexpr size_t initial_offset = 32;
constexpr uint32_t inline_data_prefix_words = 10;
constexpr uint32_t word_bytes = sizeof(uint32_t);
constexpr uint32_t query_address = 0x01489c40;
constexpr uint32_t copy_address = 0x02fbe578;
constexpr uint32_t nop_header = 0xc0001000;
constexpr uint32_t event_header = 0xc0004600;
constexpr uint32_t raw_tag = 0x68752000;
constexpr uint32_t marker = 0x35;
constexpr uint32_t shader_flag = 2;
constexpr uint32_t tail_pair_count = 2;
constexpr uint32_t count_shift = 16;
constexpr uint64_t high_mask = 0xffffffff00000000;
constexpr uint32_t high_shift = 32;
constexpr uintptr_t opaque = 0x123456789abcdef0;
constexpr size_t prefix_offset = 64;
using Writer = void (*)(render::PacketBuffer *, uint32_t, const void *, uint32_t);
struct Commands { render::PacketBuffer packet; render::Field<uint32_t> flags; };
static_assert(offsetof(Commands, flags) == sizeof(render::PacketBuffer));
Writer draw;
unsigned char **image;
int failures, checks;
unsigned query_calls, copy_calls, callback_calls;
uint8_t query_result, callback_result;
uint32_t expected_request;
render::PacketBuffer *active;
uintptr_t query_cursor, redirect_cursor, copy_cursor;
void *copied_to;
const void *copied_from;
uint64_t copied_bytes;
bool redirect_query, perform_copy = true;
std::array<uint8_t, storage_size> redirected;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)

void *owned_copy(void *destination, const void *source, uint64_t bytes)
{
    ++copy_calls;
    copied_to = destination;
    copied_from = source;
    copied_bytes = bytes;
    copy_cursor = render::packet_cursor(active);
    if (perform_copy && bytes) {
        CHECK(bytes <= storage_size);
        if (bytes <= storage_size) std::memmove(destination, source, static_cast<size_t>(bytes));
    }
    return destination;
}

uint8_t owned_query()
{
    ++query_calls;
    query_cursor = render::packet_cursor(active);
    if (redirect_query) {
        render::set_packet_cursor(active, redirect_cursor);
        reinterpret_cast<render::CommandPacketFlags *>(active)->flags.set(shader_flag);
    }
    // Both original declarations share an image base. This owned target binds them in call order;
    // no production source or original game code is altered or executed.
    const uintptr_t copy_bias = copy_address - RT_EBOOT_BASE;
    *image = reinterpret_cast<unsigned char *>(reinterpret_cast<uintptr_t>(&owned_copy) - copy_bias);
    return query_result;
}

uint8_t reserve(render::PacketBuffer *commands, uint32_t words, uintptr_t argument)
{
    ++callback_calls;
    CHECK(words == expected_request && argument == opaque);
    commands->begin = reinterpret_cast<uintptr_t>(redirected.data());
    commands->cursor = commands->begin + initial_offset;
    commands->limit = commands->cursor;
    return callback_result;
}

Commands setup(void *storage, uint32_t bytes)
{
    const uintptr_t begin = reinterpret_cast<uintptr_t>(storage);
    const uint32_t inline_words = (bytes + word_bytes - 1) / word_bytes;
    Commands commands{{begin, begin + initial_offset + (inline_words + inline_data_prefix_words) * word_bytes,
                        begin + initial_offset, reserve, opaque}, {}};
    commands.flags.set(1);
    return commands;
}

void invoke(Commands &commands, uint32_t count, const void *source, uint32_t bytes)
{
    active = &commands.packet;
    query_calls = copy_calls = callback_calls = 0;
    copied_to = nullptr;
    copied_from = nullptr;
    copied_bytes = 0;
    const uintptr_t query_bias = query_address - RT_EBOOT_BASE;
    *image = reinterpret_cast<unsigned char *>(reinterpret_cast<uintptr_t>(&owned_query) - query_bias);
    draw(active, count, source, bytes);
}

const rt_render_call *recorded(uint64_t begin, size_t expected)
{
    size_t found{};
    const auto *calls = rt_render_submitted(begin, UINT32_MAX, &found);
    CHECK(found == expected);
    return found == expected ? calls : nullptr;
}

void lengths_and_count()
{
    constexpr std::array<uint32_t, 9> sizes{0, 1, 2, 3, 4, 5, 7, 8, 9};
    constexpr std::array<uint32_t, 3> counts{0, 1, 7};
    constexpr uint8_t fill = 0xa5;
    constexpr uint8_t payload = 0x11;
    std::array<uint8_t, storage_size> storage, source;
    source.fill(payload);
    redirect_query = false;
    perform_copy = true;
    for (uint32_t count : counts) for (uint32_t bytes : sizes) {
        storage.fill(fill);
        query_result = 0;
        auto commands = setup(storage.data(), bytes);
        const uintptr_t start = commands.packet.cursor;
        const uint32_t words = (bytes + word_bytes - 1) / word_bytes;
        const uintptr_t end = start + (words + inline_data_prefix_words) * word_bytes;
        rt_render_forget();
        invoke(commands, count, source.data(), bytes);
        CHECK(query_calls == 1 && copy_calls == 1 && callback_calls == 0);
        CHECK(copied_to == reinterpret_cast<void *>(start + inline_data_prefix_words * word_bytes));
        CHECK(copied_from == source.data() && copied_bytes == bytes);
        CHECK(commands.packet.cursor == end && copy_cursor == end);
        const uint32_t header = 0xc0042701;
        CHECK(render::Field<uint32_t>::read(reinterpret_cast<void *>(start)) == header);
        const uint32_t nop = render::Field<uint32_t>::read(reinterpret_cast<void *>(start + inline_data_prefix_words * word_bytes - word_bytes - word_bytes));
        CHECK(nop == (nop_header | (words << count_shift)));
        if (!bytes) {
            const uint32_t tag = render::Field<uint32_t>::read(reinterpret_cast<void *>(end - word_bytes));
            CHECK(tag == (count ? 0 : raw_tag));
        } else {
            const size_t rounded = words * word_bytes;
            for (size_t i = 0; i < rounded; ++i) {
                const uint8_t actual = *reinterpret_cast<const uint8_t *>(start + inline_data_prefix_words * word_bytes + i);
                const uint8_t expected = i < bytes ? payload : count ? 0 : fill;
                CHECK(actual == expected);
            }
        }
        const auto *call = recorded(commands.packet.begin, 1);
        if (call) {
            CHECK(call->kind == RT_CALL_DRAW_INDEXED && call->offset == initial_offset);
            CHECK(call->u.draw_indexed.header == header && call->u.draw_indexed.max_size == count);
            CHECK(call->u.draw_indexed.address == start + inline_data_prefix_words * word_bytes);
            CHECK(call->u.draw_indexed.index_count == count && call->u.draw_indexed.initiator == 0);
        }
    }
}

void callbacks_and_wrapping()
{
    constexpr uint32_t accepted = 0x80;
    constexpr std::array<uint32_t, 4> sizes{0, 3, 0xfffffffc, 0xffffffff};
    std::array<uint8_t, storage_size> storage{}, source{};
    query_result = 1;
    redirect_query = false;
    perform_copy = false;
    for (uint32_t bytes : sizes) for (uint8_t result : {uint8_t{0}, uint8_t{accepted}}) {
        auto commands = setup(storage.data(), bytes);
        commands.packet.limit = commands.packet.cursor;
        expected_request = (bytes + word_bytes - 1) / word_bytes + inline_data_prefix_words;
        callback_result = result;
        rt_render_forget();
        invoke(commands, 0, source.data(), bytes);
        CHECK(callback_calls == 1 && query_calls == (result ? 1u : 0u) && copy_calls == (result ? 1u : 0u));
        const auto *call = recorded(reinterpret_cast<uintptr_t>(redirected.data()), result ? 1 : 0);
        if (result && call) {
            CHECK(copied_bytes == bytes);
            CHECK(copied_to == redirected.data() + initial_offset + inline_data_prefix_words * word_bytes);
            CHECK(call->u.draw_indexed.address == reinterpret_cast<uintptr_t>(copied_to));
        }
    }
    perform_copy = true;
}

void query_and_tail_aliases()
{
    std::array<uint8_t, storage_size> storage{}, source{};
    query_result = 1;
    redirect_query = true;
    auto commands = setup(storage.data(), 1);
    const uintptr_t start = commands.packet.cursor;
    redirect_cursor = reinterpret_cast<uintptr_t>(redirected.data());
    rt_render_forget();
    invoke(commands, 1, source.data(), 1);
    CHECK(query_cursor == start + sizeof(render::DrawIndexedPacket));
    CHECK(copied_to == reinterpret_cast<void *>(start + inline_data_prefix_words * word_bytes));
    CHECK(commands.packet.cursor == redirect_cursor + inline_data_prefix_words * word_bytes - sizeof(render::DrawIndexedPacket) + word_bytes);
    CHECK(render::Field<uint32_t>::read(redirected.data()) == (event_header | shader_flag));
    redirect_query = false;
    // The false route keeps its captured tail when the NOP overwrites the cursor.
    constexpr size_t tail_offset = 12;
    commands = setup(storage.data(), 0);
    constexpr uintptr_t alias_hint = 0x5000000000;
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void *alias_mapping = mmap(reinterpret_cast<void *>(alias_hint), page, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(alias_mapping != MAP_FAILED);
    if (alias_mapping == MAP_FAILED) return;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(alias_mapping);
    CHECK((begin & UINT32_MAX) == 0);
    if ((begin & UINT32_MAX) != 0) { munmap(alias_mapping, page); return; }
    auto *aliased = reinterpret_cast<Commands *>(static_cast<uint8_t *>(alias_mapping) + prefix_offset);
    const uintptr_t prefix_address = reinterpret_cast<uintptr_t>(aliased);
    aliased->packet = {begin, begin + storage_size, prefix_address + tail_offset - sizeof(render::DrawIndexedPacket), reserve, opaque};
    aliased->flags.set(0);
    query_result = 0;
    rt_render_forget();
    invoke(*aliased, 1, source.data(), 0);
    const uintptr_t captured_end = prefix_address + tail_offset + sizeof(render::RecordControlPair) * tail_pair_count;
    CHECK(copy_cursor == captured_end && aliased->packet.cursor == captured_end);
    CHECK(render::Field<uint32_t>::read(reinterpret_cast<void *>(captured_end - word_bytes)) == 0);
    CHECK(munmap(alias_mapping, page) == 0);
}

void event_cursor_reread()
{
    constexpr uintptr_t address_hint = 0x4000000000;
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void *mapping = mmap(reinterpret_cast<void *>(address_hint), page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(mapping != MAP_FAILED);
    if (mapping == MAP_FAILED) return;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(mapping);
    CHECK((begin & UINT32_MAX) == 0);
    if ((begin & UINT32_MAX) != 0) { munmap(mapping, page); return; }
    auto *commands = reinterpret_cast<Commands *>(static_cast<uint8_t *>(mapping) + prefix_offset);
    const uintptr_t prefix = reinterpret_cast<uintptr_t>(commands);
    constexpr size_t tail_offset = 12;
    commands->packet = {begin, begin + page, prefix + tail_offset - sizeof(render::DrawIndexedPacket), reserve, opaque};
    commands->flags.set(shader_flag);
    query_result = 1;
    redirect_query = false;
    uint8_t source{};
    rt_render_forget();
    invoke(*commands, 1, &source, 0);
    const uintptr_t reread_tail = begin + marker;
    const uintptr_t end = reread_tail + tail_pair_count * sizeof(render::RecordControlPair);
    CHECK(copy_cursor == end && commands->packet.cursor == end);
    CHECK(render::Field<uint32_t>::read(reinterpret_cast<void *>(reread_tail + sizeof(render::RecordControlPair))) == nop_header);
    CHECK(render::Field<uint32_t>::read(reinterpret_cast<void *>(end - word_bytes)) == 0);
    const auto *call = recorded(begin, 1);
    if (call) CHECK(call->u.draw_indexed.max_size == 1 && call->u.draw_indexed.index_count == 0);
    CHECK(munmap(mapping, page) == 0);
}

void full_address_word()
{
    constexpr uintptr_t address_hint = 0x40000000000;
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void *mapping = mmap(reinterpret_cast<void *>(address_hint), page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(mapping != MAP_FAILED);
    if (mapping == MAP_FAILED) return;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(mapping);
    CHECK((begin >> high_shift) > UINT8_MAX);
    if ((begin >> high_shift) <= UINT8_MAX) { munmap(mapping, page); return; }
    uint8_t source{};
    auto commands = setup(mapping, 1);
    query_result = 0;
    redirect_query = false;
    rt_render_forget();
    invoke(commands, 1, &source, 1);
    const auto *call = recorded(begin, 1);
    if (call) CHECK(call->u.draw_indexed.address == begin + initial_offset + inline_data_prefix_words * word_bytes);
    CHECK(munmap(mapping, page) == 0);
}

void publication_aliases()
{
    constexpr size_t tail_offset = 4;
    constexpr uint8_t source_value = 0x11;
    std::array<uint8_t, storage_size> storage{}, source{};
    source[0] = source_value;
    auto *commands = reinterpret_cast<Commands *>(storage.data() + prefix_offset);
    const uintptr_t begin = reinterpret_cast<uintptr_t>(storage.data());
    const uintptr_t prefix = reinterpret_cast<uintptr_t>(commands);
    query_result = 0;
    redirect_query = false;
    commands->packet = {begin, begin + storage_size, prefix + tail_offset - sizeof(render::DrawIndexedPacket), reserve, opaque};
    commands->flags.set(0);
    rt_render_forget();
    invoke(*commands, 1, source.data(), 0);
    const uintptr_t cleared_cursor = (prefix & high_mask);
    CHECK(copy_cursor == cleared_cursor && commands->packet.cursor == cleared_cursor);
    CHECK(copied_to == reinterpret_cast<void *>(prefix + tail_offset + sizeof(render::RecordControlPair) * tail_pair_count));
    commands->packet = {begin, begin + storage_size, prefix + tail_offset - sizeof(render::DrawIndexedPacket), reserve, opaque};
    commands->flags.set(0);
    rt_render_forget();
    invoke(*commands, 0, source.data(), 1);
    const uintptr_t before_copy = prefix + tail_offset + sizeof(render::RecordControlPair) * tail_pair_count + word_bytes;
    CHECK(copy_cursor == before_copy);
    const uintptr_t after_copy = (before_copy & ~(uintptr_t{0xff} << high_shift)) | (static_cast<uintptr_t>(source_value) << high_shift);
    CHECK(commands->packet.cursor == after_copy);
}
} // namespace

int main(int argc, char **argv)
{
    constexpr int argument_count = 2;
    if (argc != argument_count) return 1;
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
    draw = reinterpret_cast<Writer>(dlsym(library, "bb_render_draw_indexed_inline"));
    image = static_cast<unsigned char **>(dlsym(library, "rt_image"));
    CHECK(draw && image);
    if (draw && image) {
        unsigned char *saved = *image;
        lengths_and_count();
        callbacks_and_wrapping();
        query_and_tail_aliases();
        publication_aliases();
        event_cursor_reread();
        full_address_word();
        *image = saved;
    }
    rt_render_forget();
    dlclose(library);
    std::printf("inline-index observers: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
