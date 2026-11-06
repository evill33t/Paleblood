// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise real shader packet writers and the runtime recorder on owned storage.
#include "../shader_observation.h"
#include "../command_flags.h"

#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" void bb_render_emit_shader_descriptor16(render::PacketBuffer *, uint32_t, uint32_t, const void *);
extern "C" void bb_render_emit_shader_descriptor32(render::PacketBuffer *, uint32_t, uint32_t, const void *);
extern "C" void bb_render_emit_shader_sampler16(render::PacketBuffer *, uint32_t, uint32_t, const void *);
extern "C" void bb_render_emit_shader_address(render::PacketBuffer *, uint32_t, uint32_t, const void *);
extern "C" void bb_render_emit_shader_word(render::PacketBuffer *, uint32_t, uint32_t, uint32_t);
extern "C" void bb_render_select_shader_command_mode(render::PacketBuffer *, uint32_t);
extern "C" { unsigned char *rt_image; }

namespace {
unsigned checks, scenarios;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "CHECK failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); std::exit(1); \
} } while (0)

constexpr std::size_t storage_bytes = 512;
constexpr uint32_t initial_offset = 64, callback_offset = 128;
constexpr uint32_t packet_fill = 0x6b, replacement_fill = 0x7c;
constexpr uint32_t raw_word = 0x89abcdef, shader_mode = 0xb0;
constexpr uintptr_t raw_address = 0xabcd12ef12345679;
constexpr uint8_t accepted = 0x80;
constexpr uint32_t packet_flags = 2;
constexpr unsigned mapped_pages = 2;
constexpr uint32_t sh_payload_bytes = offsetof(render::ShaderPacketWords, value_low);
constexpr uint32_t descriptor_payload_bytes = offsetof(render::ShaderDescriptorPacket, payload);
constexpr std::array<uint32_t, RT_USER_DATA_MAX_WORDS> source_words = {
    0x01020304, 0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00, 0x76543210, 0xfedcba98, 0x80000001};
constexpr std::array<uint32_t, RT_USER_DATA_MAX_WORDS> changed_words = {
    0xfedcba98, 0x7fc12345, 0x80000000, 0xffc54321, 0x40000000, 0x01234567, 0x89abcdef, 0xa5a55a5a};
// GFX7's COMPUTE_USER_DATA_0, then PS/VS/GS/ES/HS/LS user-data register bases.
constexpr std::array<uint16_t, RT_SHADER_STAGES> stage_bases = {
    0x2e40, 0x2c0c, 0x2c4c, 0x2c8c, 0x2ccc, 0x2d0c, 0x2d4c};

enum class Writer { Descriptor16, Descriptor32, Sampler16, Address, Word, Mode };
struct WriterInfo { Writer kind; unsigned payload_words, packet_words, payload_offset; };
constexpr std::array<WriterInfo, 5> writers = {{
    {Writer::Descriptor16, 4, 8, descriptor_payload_bytes},
    {Writer::Descriptor32, 8, 12, descriptor_payload_bytes},
    {Writer::Sampler16, 4, 8, descriptor_payload_bytes},
    {Writer::Address, 2, 4, sh_payload_bytes},
    {Writer::Word, 1, 3, sh_payload_bytes}}};
#if !defined(PB_SHADER_OBSERVER_ABSENT)
constexpr uint32_t sh_register_index_bytes = offsetof(render::ShaderPacketWords, register_index);
constexpr WriterInfo mode_writer{Writer::Mode, 1, 3, sh_payload_bytes};
#endif

template <uint32_t Address, typename T>
constexpr std::size_t table_offset(rt::global<Address, T>) { return Address - RT_EBOOT_BASE; }

struct OwnedStageTable {
    uint8_t *storage;
    std::size_t bytes;

    explicit OwnedStageTable(bool ambiguous = false)
    {
        const long page = sysconf(_SC_PAGESIZE);
        CHECK(page > 0);
        constexpr std::size_t offset = table_offset(render::shader_stage_register_bases);
        bytes = (offset + sizeof(stage_bases) + std::size_t(page) - 1) / std::size_t(page) * std::size_t(page);
        storage = static_cast<uint8_t *>(mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0));
        CHECK(storage != MAP_FAILED);
        const std::size_t page_start = offset / std::size_t(page) * std::size_t(page);
        CHECK(mprotect(storage + page_start, std::size_t(page), PROT_READ | PROT_WRITE) == 0);
        rt_image = storage;
        auto bases = stage_bases;
        if (ambiguous) bases[RT_STAGE_PIXEL] = bases[RT_STAGE_VERTEX];
        std::memcpy(const_cast<uint16_t *>(render::shader_stage_register_bases.address()),
                    bases.data(), sizeof(bases));
        CHECK(mprotect(storage + page_start, std::size_t(page), PROT_READ) == 0);
    }
    ~OwnedStageTable() { CHECK(munmap(storage, bytes) == 0); rt_image = nullptr; }
};

template <typename T> void field(render::PacketBuffer *commands, std::size_t offset, T value)
{
    std::memcpy(reinterpret_cast<uint8_t *>(commands) + offset, &value, sizeof(value));
}

uint32_t word_at(const void *address)
{
    uint32_t word;
    std::memcpy(&word, address, sizeof(word));
    return word;
}

struct Fixture {
    alignas(uintptr_t) std::array<uint8_t, storage_bytes> prefix{}, packet{}, replacement{};
    std::array<uint32_t, RT_USER_DATA_MAX_WORDS> source = source_words;
    unsigned request{};
    uint8_t result = accepted;
    bool redirect{}, change_source{};

    render::PacketBuffer *commands() { return reinterpret_cast<render::PacketBuffer *>(prefix.data()); }
    uintptr_t begin() const { return reinterpret_cast<uintptr_t>(packet.data()); }
    void storage(uintptr_t start, uintptr_t cursor, uintptr_t limit)
    {
        field(commands(), offsetof(render::PacketBuffer, begin), start);
        field(commands(), offsetof(render::PacketBuffer, cursor), cursor);
        field(commands(), offsetof(render::PacketBuffer, limit), limit);
    }
    Fixture()
    {
        packet.fill(packet_fill); replacement.fill(replacement_fill);
        storage(begin(), begin() + initial_offset, begin() + storage_bytes);
        field(commands(), offsetof(render::CommandPacketFlags, flags), packet_flags);
    }
    void emit(Writer writer, unsigned stage = RT_STAGE_VERTEX, unsigned slot = 0,
              const void *descriptor = nullptr, uint32_t mode = shader_mode)
    {
        if (!descriptor) descriptor = source.data();
        switch (writer) {
        case Writer::Descriptor16: bb_render_emit_shader_descriptor16(commands(), stage, slot, descriptor); break;
        case Writer::Descriptor32: bb_render_emit_shader_descriptor32(commands(), stage, slot, descriptor); break;
        case Writer::Sampler16: bb_render_emit_shader_sampler16(commands(), stage, slot, descriptor); break;
        case Writer::Address:
            bb_render_emit_shader_address(commands(), stage, slot, reinterpret_cast<const void *>(raw_address)); break;
        case Writer::Word: bb_render_emit_shader_word(commands(), stage, slot, raw_word); break;
        case Writer::Mode: bb_render_select_shader_command_mode(commands(), mode); break;
        }
    }
};

uint8_t reserve(render::PacketBuffer *commands, uint32_t request, uintptr_t argument)
{
    auto &fixture = *reinterpret_cast<Fixture *>(argument);
    CHECK(request == fixture.request);
    if (fixture.redirect) {
        const uintptr_t start = reinterpret_cast<uintptr_t>(fixture.replacement.data());
        field(commands, offsetof(render::PacketBuffer, begin), start);
        field(commands, offsetof(render::PacketBuffer, cursor), start + callback_offset);
    }
    if (fixture.change_source) fixture.source = changed_words;
    return fixture.result;
}

void callback(Fixture &fixture, unsigned request)
{
    fixture.request = request;
    field(fixture.commands(), offsetof(render::PacketBuffer, limit), render::packet_cursor(fixture.commands()));
    field(fixture.commands(), offsetof(render::PacketBuffer, space_callback), render::PacketSpaceCallback(reserve));
    field(fixture.commands(), offsetof(render::PacketBuffer, callback_argument), reinterpret_cast<uintptr_t>(&fixture));
}

void headless()
{
    if (rt_render_observer_available()) return;
    ++scenarios;
    const long page = sysconf(_SC_PAGESIZE); CHECK(page > 0);
    auto *mapping = static_cast<uint8_t *>(mmap(nullptr, std::size_t(page) * mapped_pages,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
    CHECK(mapping != MAP_FAILED);
    CHECK(mprotect(mapping, std::size_t(page), PROT_NONE) == 0);
    auto *commands = reinterpret_cast<render::PacketBuffer *>(mapping + page - sizeof(uintptr_t));
    Fixture fixture;
    field(commands, offsetof(render::PacketBuffer, cursor), fixture.begin());
    field(commands, offsetof(render::PacketBuffer, limit), fixture.begin() + storage_bytes);
    field(commands, offsetof(render::CommandPacketFlags, flags), packet_flags);
    bb_render_emit_shader_word(commands, RT_STAGE_VERTEX, 0, raw_word);
    constexpr uint32_t word_header = 0xc0017600;
    CHECK(word_at(fixture.packet.data()) == (word_header | packet_flags));
    CHECK(word_at(fixture.packet.data() + sh_payload_bytes) == raw_word);
    bb_render_emit_shader_address(commands, RT_STAGE_VERTEX, 0, reinterpret_cast<void *>(raw_address));
    bb_render_emit_shader_descriptor16(commands, RT_STAGE_VERTEX, 0, fixture.source.data());
    bb_render_emit_shader_descriptor32(commands, RT_STAGE_VERTEX, 0, fixture.source.data());
    bb_render_emit_shader_sampler16(commands, RT_STAGE_VERTEX, 0, fixture.source.data());
    bb_render_select_shader_command_mode(commands, shader_mode);
    // Invalid pointers are never evaluated by the disabled payload helper.
    render::record_shader_words<1>({}, reinterpret_cast<const render::Field<uint32_t> *>(mapping), mapping);
    CHECK(munmap(mapping, std::size_t(page) * mapped_pages) == 0);
    ++scenarios;
    Fixture moved; moved.redirect = moved.change_source = true;
    callback(moved, writers.front().packet_words); moved.emit(Writer::Descriptor16);
    CHECK(word_at(moved.replacement.data() + callback_offset + descriptor_payload_bytes) == changed_words.front());
}

#if !defined(PB_SHADER_OBSERVER_ABSENT)
void enabled() { CHECK(rt_render_observer_available()); rt_render_forget(); }

const rt_render_call &record(uintptr_t buffer, uint32_t offset, unsigned stage, unsigned slot,
                             unsigned words, const void *payload)
{
    std::size_t count;
    const auto *calls = rt_render_submitted(buffer, UINT32_MAX, &count);
    CHECK(count == 1 && calls && calls[0].kind == RT_CALL_USER_DATA);
    const auto &call = calls[0];
    CHECK(call.offset == offset);
    CHECK(call.u.user_data.stage == stage && call.u.user_data.slot == slot && call.u.user_data.count == words);
    CHECK(std::memcmp(call.u.user_data.words, payload, words * sizeof(uint32_t)) == 0);
    return call;
}

void direct_stages_and_slots()
{
    for (const auto &writer : writers) {
        for (unsigned stage = 0; stage < RT_SHADER_STAGES; ++stage) {
            for (unsigned slot : {0u, RT_USER_DATA_SLOTS - writer.payload_words}) {
                ++scenarios; enabled(); Fixture fixture; fixture.emit(writer.kind, stage, slot);
                const uint8_t *packet = fixture.packet.data() + initial_offset;
                const unsigned index_offset = writer.payload_offset - sizeof(uint32_t);
                CHECK(word_at(packet + index_offset) == stage_bases[stage] + slot - render::shader_register_origin);
                const auto &call = record(fixture.begin(), initial_offset, stage, slot,
                    writer.payload_words, packet + writer.payload_offset);
                if (writer.kind == Writer::Word) CHECK(call.u.user_data.words[0] == raw_word);
                else if (writer.kind == Writer::Address) {
                    constexpr unsigned high_word = 1;
                    CHECK(call.u.user_data.words[0] == uint32_t(raw_address));
                    CHECK(call.u.user_data.words[high_word] == uint32_t(raw_address >> render::shader_word_bits));
                } else CHECK(std::memcmp(call.u.user_data.words, source_words.data(),
                    writer.payload_words * sizeof(uint32_t)) == 0);
            }
        }
    }
}

void callbacks()
{
    for (const auto &writer : writers) {
        for (uint8_t result : {uint8_t(0), accepted}) {
            ++scenarios; enabled(); Fixture fixture;
            fixture.redirect = fixture.change_source = true; fixture.result = result;
            callback(fixture, writer.packet_words); const auto untouched = fixture.replacement;
            fixture.emit(writer.kind);
            const uintptr_t start = reinterpret_cast<uintptr_t>(fixture.replacement.data());
            if (result) {
                const auto &call = record(start, callback_offset, RT_STAGE_VERTEX, 0, writer.payload_words,
                    fixture.replacement.data() + callback_offset + writer.payload_offset);
                if (writer.kind != Writer::Address && writer.kind != Writer::Word)
                    CHECK(call.u.user_data.words[0] == changed_words[0]);
            } else {
                std::size_t count; rt_render_submitted(start, UINT32_MAX, &count);
                CHECK(count == 0 && untouched == fixture.replacement);
            }
        }
    }
}

void output_and_input_aliases()
{
    for (const auto &writer : writers) {
        ++scenarios; enabled(); Fixture fixture;
        const uintptr_t start = reinterpret_cast<uintptr_t>(fixture.prefix.data());
        const bool descriptor = writer.payload_offset == descriptor_payload_bytes;
        const uint32_t prefix_offset = descriptor ? 0 : sh_payload_bytes;
        fixture.storage(start, start + prefix_offset, start + storage_bytes);
        fixture.emit(writer.kind);
        const auto &call = record(start, prefix_offset, RT_STAGE_VERTEX, 0, writer.payload_words,
            fixture.prefix.data() + prefix_offset + writer.payload_offset);
        CHECK(call.u.user_data.words[0] != (writer.kind == Writer::Address ? uint32_t(raw_address) :
            writer.kind == Writer::Word ? raw_word : source_words[0]));
    }
    for (Writer writer : {Writer::Descriptor16, Writer::Sampler16, Writer::Descriptor32}) {
        ++scenarios; enabled(); Fixture fixture;
        constexpr unsigned source_overlap = 24;
        auto *output = fixture.packet.data() + initial_offset;
        std::memcpy(output + source_overlap, source_words.data(), sizeof(source_words));
        fixture.emit(writer, RT_STAGE_VERTEX, 0, output + source_overlap);
        const unsigned words = writer == Writer::Descriptor32 ? RT_USER_DATA_MAX_WORDS : writers.front().payload_words;
        const auto &call = record(fixture.begin(), initial_offset, RT_STAGE_VERTEX, 0, words,
                                 output + descriptor_payload_bytes);
        CHECK(call.u.user_data.words[0] == source_words[0]);
        if (writer == Writer::Descriptor32) {
            constexpr unsigned copied_upper_pair = 6;
            constexpr unsigned copied_lower_pair = 2;
            CHECK(call.u.user_data.words[copied_lower_pair] == source_words[copied_upper_pair]);
        }
    }
}

void register_alias_changes_stage()
{
    ++scenarios; enabled();
    constexpr unsigned target_slot = 4;
    constexpr uint32_t target_index = stage_bases[RT_STAGE_VERTEX] + target_slot - render::shader_register_origin;
    constexpr unsigned word_packet_bytes = writers.back().packet_words * sizeof(uint32_t);
    constexpr unsigned cursor_to_packet = offsetof(render::PacketBuffer, cursor) - sh_register_index_bytes;
    const long page = sysconf(_SC_PAGESIZE); CHECK(page > 0);
    // Reserve virtual address space, committing just one page at a DWORD wrap boundary.
    // This controls the published pointer's low word without replacing any existing mapping.
    const std::size_t reservation_bytes = std::size_t(UINT32_MAX) + 1 + std::size_t(page);
    auto *mapping = static_cast<uint8_t *>(mmap(nullptr, reservation_bytes,
        PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0));
    CHECK(mapping != MAP_FAILED);
    const uintptr_t start = (reinterpret_cast<uintptr_t>(mapping) + UINT32_MAX) & ~uintptr_t(UINT32_MAX);
    CHECK(start >= reinterpret_cast<uintptr_t>(mapping));
    CHECK(start + uintptr_t(page) <= reinterpret_cast<uintptr_t>(mapping) + reservation_bytes);
    CHECK(mprotect(reinterpret_cast<void *>(start), std::size_t(page), PROT_READ | PROT_WRITE) == 0);
    auto *prefix = reinterpret_cast<uint8_t *>(start) + target_index - word_packet_bytes - cursor_to_packet;
    auto *commands = reinterpret_cast<render::PacketBuffer *>(prefix);
    const uintptr_t packet = reinterpret_cast<uintptr_t>(prefix + cursor_to_packet);
    field(commands, offsetof(render::PacketBuffer, begin), start);
    field(commands, offsetof(render::PacketBuffer, cursor), packet);
    field(commands, offsetof(render::PacketBuffer, limit), start + uintptr_t(page));
    field(commands, offsetof(render::CommandPacketFlags, flags), packet_flags);
    bb_render_emit_shader_word(commands, RT_STAGE_PIXEL, 0, raw_word);
    CHECK(word_at(reinterpret_cast<void *>(packet + sh_register_index_bytes)) == target_index);
    record(start, uint32_t(packet - start), RT_STAGE_VERTEX, target_slot, 1,
           reinterpret_cast<void *>(packet + sh_payload_bytes));
    CHECK(munmap(mapping, reservation_bytes) == 0);
}

void modes()
{
    constexpr std::array<uint32_t, 5> admitted = {0, 0x45, 0xad, 0xb0, 0x200};
    constexpr std::array<uint32_t, 4> rejected = {1, 0x44, 0x201, 0xffffffff};
    constexpr uint32_t stages_register = 0xa2d5;
    for (uint32_t mode : admitted) {
        ++scenarios; enabled(); Fixture fixture; fixture.emit(Writer::Mode, 0, 0, nullptr, mode);
        std::size_t count; const auto *call = rt_render_submitted(fixture.begin(), UINT32_MAX, &count);
        CHECK(count == 1 && call && call->kind == RT_CALL_REGISTER);
        CHECK(call->offset == initial_offset && call->u.reg.address == stages_register && call->u.reg.value == mode);
    }
    for (uint32_t mode : rejected) {
        ++scenarios; enabled(); Fixture fixture; const auto untouched = fixture.packet;
        fixture.emit(Writer::Mode, 0, 0, nullptr, mode);
        std::size_t count; rt_render_submitted(fixture.begin(), UINT32_MAX, &count);
        CHECK(count == 0 && fixture.packet == untouched);
    }
    for (uint8_t result : {uint8_t(0), accepted}) {
        ++scenarios; enabled(); Fixture fixture; fixture.redirect = true; fixture.result = result;
        callback(fixture, mode_writer.packet_words); fixture.emit(Writer::Mode);
        std::size_t count; const auto *call = rt_render_submitted(
            reinterpret_cast<uintptr_t>(fixture.replacement.data()), UINT32_MAX, &count);
        CHECK(count == (result ? 1u : 0u));
        if (result) CHECK(call && call->offset == callback_offset && call->u.reg.value == shader_mode);
    }
    ++scenarios; enabled(); Fixture alias;
    const uintptr_t start = reinterpret_cast<uintptr_t>(alias.prefix.data());
    alias.storage(start, start + sh_payload_bytes, start + storage_bytes); alias.emit(Writer::Mode);
    std::size_t count; const auto *call = rt_render_submitted(start, UINT32_MAX, &count);
    CHECK(count == 1 && call && call->u.reg.address == stages_register);
    CHECK(call->u.reg.value == word_at(alias.prefix.data() + sh_payload_bytes + mode_writer.payload_offset));
    CHECK(call->u.reg.value != shader_mode);
}

void invalid_span()
{
    enabled(); Fixture fixture;
    fixture.emit(Writer::Descriptor32, RT_STAGE_VERTEX, RT_USER_DATA_SLOTS - 1);
}
void unknown_register()
{
    enabled(); Fixture fixture;
    fixture.emit(Writer::Word, RT_STAGE_VERTEX, UINT32_MAX);
}
void ambiguous_register()
{
    enabled(); OwnedStageTable ambiguous(true); Fixture fixture;
    fixture.emit(Writer::Word);
}
void mode_outside_context()
{
    enabled(); render::ShaderPacketWords packet{};
    constexpr uint32_t index_into_user_config = RT_UCONFIG_REGISTERS_FIRST - RT_CONTEXT_REGISTERS_FIRST;
    packet.register_index.set(index_into_user_config);
    packet.value_low.set(shader_mode);
    render::record_shader_mode(render::PacketObservation{1, 0, true}, &packet);
}
void expect_abort(void (*action)())
{
    ++scenarios; const pid_t child = fork(); CHECK(child >= 0);
    if (child == 0) {
        const rlimit no_core{0, 0}; setrlimit(RLIMIT_CORE, &no_core);
        std::freopen("/dev/null", "w", stderr); action(); std::_Exit(0);
    }
    int status; CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}
#endif
}  // namespace

int main()
{
    OwnedStageTable table;
    headless();
#if !defined(PB_SHADER_OBSERVER_ABSENT)
    direct_stages_and_slots(); callbacks(); output_and_input_aliases(); register_alias_changes_stage(); modes();
    expect_abort(invalid_span); expect_abort(unknown_register); expect_abort(ambiguous_register);
    expect_abort(mode_outside_context);
    rt_render_forget();
#endif
    std::printf("shader observers: %u scenarios, %u checks, zero failures\n", scenarios, checks);
    return 0;
}
