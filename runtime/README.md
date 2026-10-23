# runtime

Paleblood's own PS4 runtime, game-agnostic: nothing here may name the game, a tracked function,
or an address (`tools/check_agnostic.py` enforces it). If a feature needs game knowledge, the
mechanism lives here and the knowledge lives in `game/` as data.

| Part | Files | State |
|---|---|---|
| Loader | `loader.c`, `include/runtime/loader.h` | maps a PS4 executable: segments, relocations, imports bound through a callback, TLS template, thread-pointer loads moved to GS |
| Boot harness | `boot.c` (`pbboot`) | runs the executable's entry on the runtime; stops at the first unimplemented import and names it |
| System libraries | `syslib/*.c`, `include/runtime/syslib.h` | implementations by library and symbol (`RT_SYSLIB`), one file per system call group |
| Direct memory | `direct_memory.c`, `include/runtime/direct_memory.h`, `syslib/kernel_memory.c` | explicitly configured logical backing pool; allocation/map/release policy and limitations in [direct memory](../docs/own-runtime-direct-memory.md) |
| NIDs | `nid.c`, `include/runtime/nid.h` | symbol name to NID |
| CPU topology | `cpu.c`, `include/runtime/cpu.h` | own main-thread guest-bit map over actual allowed host CPUs; dynamic Linux query; explicit configuration and limits in [CPU policy](../docs/own-runtime-cpu-topology.md) |
| Hooks and captures | `hooks.c`, `capture.c`, `include/runtime/recorder.hpp`, `probe.c`, `plugin.c` | hook registry and opt-in case writer; production hook dispatch does not yet observe calls or connect the Recorder to replacements; serialized cases carry function/run provenance and stable buffer ordering |

## Host v1 hook callback

`runtime/host_hooks.c` provides a Paleblood-owned implementation for the version-1
`install_hook(offset, size, target)` callback. The host calls `rt_host_hooks_initialize` with its
v1 interface before `bbgame_init`; the callback then writes a 14-byte x86-64 indirect absolute
jump at the image-relative function entry. This form preserves every guest argument register.
The host must keep the mapped image writable and executable until hook installation finishes.
The callback bounds-checks the complete function extent and rejects short functions and null
targets. The own boot harness can load a same-architecture host library with
`pbboot EBOOT.elf --game-lib /absolute/path/to/libbbgame.so`. After mapping all objects and
registering the main thread, it initializes this callback and calls the library's `bbgame_init`
before the first module or executable entry. A missing library/entry or initialization failure
stops before guest execution; a failed initializer may already have changed hook bytes, so that
image must not be entered. The library stays loaded until process exit. The current Linux image
is writable/executable during installation; a future memory backend must preserve this ordering
while introducing its own protection transitions.

Omitting the flag preserves plugin-free diagnostic boot. `--imports` lists bindings without
loading the game library, even when both flags are provided. This bridge connects completed
replacements to the own runtime; remaining boot services and graphics still determine how far
the guest reaches.

Roadmap, in order: loader (done) → kernel, threads and memory → files → input → audio → video
out → GNM and shaders (the largest). `tools/boot.py` runs the boot harness and records how far
it gets (`symbols/boot.json`, `symbols/boot_history.csv`, `symbols/imports.csv`); the README's
progress table shows the import coverage and the furthest boot milestone. The boot history keeps
the outcome, phase, import return site, and guest fault location so changes to a failure's exact
execution point remain visible even when import totals and the blocking symbol stay the same.

Clean room: written from public documentation (NID databases, psdevwiki), our own
reverse engineering and observed behaviour. No code copied from, or modelled line by line on,
another emulator or runtime.

Image allocations use `rt_map_anonymous_at`: Linux retains `MAP_FIXED_NOREPLACE`; other POSIX
hosts can request an address as a hint and release a result at another address. This never
replaces an existing mapping. The portable allocation primitive alone does not make `pbboot`
build or execute guests on macOS; its thread state, signals and other boot dependencies still
need platform support.

## Owned Darwin startup

The explicit x86-64 macOS boot target composes the owned pthread-key TCB with the bounded loader
rewrite; its owned tests and remaining limits are in
[Darwin startup](../docs/own-runtime-darwin-startup.md). The default macOS build remains the
standalone Metal unit. This startup path does not connect guest graphics to Metal.
