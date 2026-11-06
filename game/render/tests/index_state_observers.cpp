// SPDX-License-Identifier: GPL-2.0-or-later
// Owned command storage tests the loaded writers and their optional renderer boundary.
#include "../command_reservation.h"
#include "../index_observe.h"
#include "runtime/render_calls.h"

#include <array>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>



namespace {
constexpr size_t storage_words = 32;
constexpr uint32_t word_bytes = sizeof(uint32_t);
constexpr uint32_t initial_offset = 16;
constexpr uint32_t redirected_offset = 32;
constexpr uint32_t high_word = 0xffffffff;
constexpr uint32_t test_word = 0x80010003;
constexpr uint64_t test_address = 0xabcd12ef12345679;
constexpr uint64_t emitted_address = 0xef12345678;
constexpr uintptr_t callback_argument = 0x123456789abcdef0;
constexpr uint8_t accepted = 0x80;
constexpr size_t cursor_overlap = 12;
constexpr size_t base_overlap = 8;
constexpr uint32_t reject_count = 0;
int failures;
int checks;

uint8_t callback_result;
uint32_t expected_request;
unsigned callback_calls;
render::PacketBuffer *redirect_to;
std::array<uint32_t, storage_words> redirected;
using WordWriter = void (*)(render::PacketBuffer *, uint32_t);
using BaseWriter = void (*)(render::PacketBuffer *, uint64_t);
WordWriter set_type;
WordWriter set_size;
BaseWriter set_base;

#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)

uint8_t reserve(render::PacketBuffer *commands, uint32_t requested, uintptr_t argument)
{
    ++callback_calls;
    CHECK(requested == expected_request);
    CHECK(argument == callback_argument);
    if (redirect_to) {
        commands->begin = redirect_to->begin;
        commands->cursor = redirect_to->cursor;
        commands->limit = commands->cursor; // An accepted callback is not checked again.
    }
    return callback_result;
}

render::PacketBuffer buffer(void *storage, uint32_t words, uint32_t offset = initial_offset)
{
    const uintptr_t begin = reinterpret_cast<uintptr_t>(storage);
    return {begin, begin + offset + words * word_bytes, begin + offset, reserve, callback_argument};
}

const rt_render_call *submitted(uint64_t begin, size_t expected)
{
    size_t count{};
    const rt_render_call *calls = rt_render_submitted(begin, UINT32_MAX, &count);
    CHECK(count == expected);
    return count == expected ? calls : nullptr;
}

void direct()
{
    constexpr uint32_t type_header = 0xc0002a00;
    constexpr uint32_t base_header = 0xc0012600;
    constexpr uint32_t size_header = 0xc0001300;
    constexpr uint32_t pair_words = 2;
    constexpr uint32_t triple_words = 3;
    std::array<uint32_t, storage_words> words{};
    {
        rt_render_forget();
        auto commands = buffer(words.data(), pair_words);
        const uintptr_t start = commands.cursor;
        set_type(&commands, test_word);
        CHECK(words[initial_offset / word_bytes] == type_header);
        CHECK(words[initial_offset / word_bytes + 1] == test_word);
        CHECK(commands.cursor == start + pair_words * word_bytes);
        auto calls = submitted(commands.begin, 1);
        if (calls) {
            CHECK(calls->kind == RT_CALL_INDEX_TYPE && calls->offset == initial_offset);
            CHECK(calls->u.index_type == test_word);
        }
        rt_render_forget();
        commands = buffer(words.data(), triple_words);
        set_base(&commands, test_address);
        CHECK(words[initial_offset / word_bytes] == base_header);
        const uint64_t observed = static_cast<uint64_t>(words[initial_offset / word_bytes + pair_words]) << 32 |
                                  words[initial_offset / word_bytes + 1];
        CHECK(observed == emitted_address);
        CHECK(commands.cursor == start + triple_words * word_bytes);
        calls = submitted(commands.begin, 1);
        if (calls) CHECK(calls->kind == RT_CALL_INDEX_BASE && calls->u.index_base == observed);
        rt_render_forget();
        commands = buffer(words.data(), pair_words);
        set_size(&commands, high_word);
        CHECK(words[initial_offset / word_bytes] == size_header);
        CHECK(words[initial_offset / word_bytes + 1] == high_word);
        calls = submitted(commands.begin, 1);
        if (calls) CHECK(calls->kind == RT_CALL_INDEX_BUFFER_SIZE && calls->u.index_buffer_size == high_word);
    }
}

void callbacks()
{
    constexpr uint32_t pair_words = 2;
    std::array<uint32_t, storage_words> words{};
    expected_request = pair_words;
    auto replacement = buffer(redirected.data(), reject_count, redirected_offset);
    redirect_to = &replacement;
    for (uint8_t result : {uint8_t{0}, accepted}) {
        callback_result = result;
        callback_calls = 0;
        rt_render_forget();
        auto commands = buffer(words.data(), reject_count);
        set_type(&commands, test_word);
        CHECK(callback_calls == 1);
        const auto calls = submitted(replacement.begin, result ? 1 : 0);
        if (result && calls) {
            CHECK(calls->offset == redirected_offset && calls->u.index_type == test_word);
            CHECK(commands.cursor == replacement.cursor + pair_words * word_bytes);
        } else {
            CHECK(commands.cursor == replacement.cursor);
        }
    }
    redirect_to = nullptr;
}

void aliases()
{
    constexpr uint32_t pair_words = 2;
    constexpr uint32_t triple_words = 3;
    constexpr uint32_t value_after_cursor_add = 7;
    constexpr uint64_t base_after_cursor_add = 0x10b12345678;
    constexpr uint64_t address_with_carrying_high_byte = 0xff12345679;
    rt_render_forget();
    render::PacketBuffer commands{};
    commands = buffer(&commands, pair_words, cursor_overlap);
    const uintptr_t begin = commands.begin;
    set_type(&commands, high_word);
    const auto calls = submitted(begin, 1);
    if (calls) CHECK(calls->offset == cursor_overlap && calls->u.index_type == value_after_cursor_add);
    rt_render_forget();
    commands = buffer(&commands, pair_words, cursor_overlap);
    set_size(&commands, high_word);
    const auto size_overlap_calls = submitted(begin, 1);
    if (size_overlap_calls) CHECK(size_overlap_calls->offset == cursor_overlap &&
                                 size_overlap_calls->u.index_buffer_size == value_after_cursor_add);
    rt_render_forget();
    commands = buffer(&commands, triple_words, base_overlap);
    set_base(&commands, address_with_carrying_high_byte);
    const auto base_calls = submitted(begin, 1);
    if (base_calls) CHECK(base_calls->offset == base_overlap && base_calls->u.index_base == base_after_cursor_add);
    rt_render_forget();
    commands = buffer(&commands, pair_words, 0);
    set_size(&commands, test_word); // The packet overwrites begin: use the pre-store snapshot.
    const auto size_calls = submitted(begin, 1);
    if (size_calls) CHECK(size_calls->offset == 0 && size_calls->u.index_buffer_size == test_word);
}

void invalid_available_offsets()
{
    constexpr uint64_t wider_than_offset = uint64_t{1} << 32;
    std::array<uint32_t, storage_words> words{};
    for (bool before_begin : {false, true}) {
        const pid_t child = fork();
        CHECK(child >= 0);
        if (!child) {
            const rlimit no_core{0, 0};
            setrlimit(RLIMIT_CORE, &no_core);
                    auto commands = buffer(words.data(), 1 + 1);
            commands.begin = before_begin ? commands.cursor + 1 : commands.cursor - wider_than_offset;
            set_type(&commands, test_word);
            _exit(0);
        }
        if (child > 0) {
            int status{};
            CHECK(waitpid(child, &status, 0) == child);
            CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
        }
    }
}
} // namespace

int main(int argc, char **argv)
{
    constexpr int argument_count = 2;
    if (argc != argument_count) return 1;
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
    set_type = reinterpret_cast<WordWriter>(dlsym(library, "bb_render_set_index_type"));
    set_size = reinterpret_cast<WordWriter>(dlsym(library, "bb_render_set_index_buffer_size"));
    set_base = reinterpret_cast<BaseWriter>(dlsym(library, "bb_render_set_index_base"));
    CHECK(set_type && set_size && set_base);
    if (set_type && set_size && set_base) {
        direct();
        callbacks();
        aliases();
        invalid_available_offsets();
    }
    rt_render_forget();
    dlclose(library);
    std::printf("index-state observers: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
