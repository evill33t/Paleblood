// SPDX-License-Identifier: GPL-2.0-or-later
// A verifier-like host exports no renderer: the canonical bridge must not prevent loading or read observer-only fields.
#include "../command_packets.h"
#include <cstdio>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    constexpr int argument_count = 2;
    constexpr uint32_t type_header = 0xc0002a00;
    constexpr uint32_t raw_type = 0x80000001;
    constexpr size_t packet_words = 2;
    constexpr size_t mapped_pages = 2;
    if (argc != argument_count) return 1;
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
    using Writer = void (*)(render::PacketBuffer *, uint32_t);
    auto writer = reinterpret_cast<Writer>(dlsym(library, "bb_render_set_index_type"));
    if (!writer) return 1;
    uint32_t words[packet_words]{};
    const uintptr_t cursor = reinterpret_cast<uintptr_t>(words);
    render::PacketBuffer commands{0, cursor + sizeof words, cursor, nullptr, 0};
    writer(&commands, raw_type);
    const bool passed = words[0] == type_header && words[1] == raw_type && commands.cursor == cursor + sizeof words;
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void *mapping = mmap(nullptr, mapped_pages * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return 1;
    auto *split = reinterpret_cast<render::PacketBuffer *>(static_cast<unsigned char *>(mapping) + page - sizeof(uintptr_t));
    *split = {0, cursor + sizeof words, cursor, nullptr, 0};
    if (mprotect(mapping, page, PROT_NONE)) return 1;
    writer(split, raw_type); // begin is inaccessible; an absent host must skip that read.
    const bool unreadable_begin = words[1] == raw_type;
    munmap(mapping, mapped_pages * page);
    dlclose(library);
    std::printf("index-state absent renderer: %s\n", passed && unreadable_begin ? "passed" : "failed");
    return !(passed && unreadable_begin);
}
