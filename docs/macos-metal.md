# macOS Metal platform unit

On macOS, CMake defaults to `PB_METAL_ONLY=ON`. This builds `pbmetal`, a native static library
with a C API and an Objective-C++ implementation linked to Apple's Metal and Foundation
frameworks. This path does not build `pbboot`, `pbloader`, `bbrt`, `bbgame` or the Linux tests.
Turning `PB_METAL_ONLY` off on macOS fails with an explicit unsupported-runtime diagnostic.
The full guest boot/runtime path currently requires Linux x86-64.

The Metal unit creates a device, command queue and RGBA8Unorm offscreen texture, submits a
render-pass clear/store, then reads pixels after command-buffer completion. It also accepts complete RGBA8 frames
through a synchronous CPU upload to the same texture. This first
shared-texture implementation requires a unified-memory Metal device. Successful CMake
configuration or compilation does not prove that a suitable device exists. There is no
window, presentation, game shader translation or guest execution in this unit.

Use Apple Clang and an installed macOS SDK. The intended deployment target is macOS 13 or
newer. For a native Apple Silicon library build, from the repository root:

```sh
cmake -S . -B build-metal -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DPB_METAL_ONLY=ON
cmake --build build-metal --target pbmetal --parallel 2
```

`PB_ENABLE_METAL_SMOKE_TEST` defaults to `OFF`: ordinary builds do not register or execute
a GPU test. On a machine where a real unified-memory Metal device is available, opt in:

```sh
cmake -S . -B build-metal -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DPB_METAL_ONLY=ON -DPB_ENABLE_METAL_SMOKE_TEST=ON
cmake --build build-metal --target test_metal_offscreen test_metal_upload --parallel 2
ctest --test-dir build-metal -R '^metal_(offscreen|upload)$' --output-on-failure
```

Each test has a 30-second CTest timeout. It fails if device creation is unavailable or any check
fails; an unavailable GPU is not reported as a passing or skipped smoke. The offscreen test checks four actual
clear/store submissions, all RGBA channels, readback padding/canaries and API lifecycle/error
contracts. It reports the architecture selected by the compiler; an x86-64 or universal build
is not evidence that the guest runtime works through Rosetta. The upload test checks patterned frames, all RGBA channels and row orientation, arbitrary
source padding, exact final-pixel capacity, protected source memory, unchanged destination
padding, source lifetime, invalid-input preservation and alternation with GPU clears. Uploads
are CPU texture updates; the test counts GPU clear submissions separately. Device loss,
hardware timeout and allocation exhaustion are not deliberately induced.

`rt_metal_offscreen_upload` copies `width * 4` bytes from each source row and retains no caller
pointer. It packs rows before calling Apple's
[`replaceRegion:mipmapLevel:withBytes:bytesPerRow:`](https://developer.apple.com/documentation/metal/mtltexture/replace(region:mipmaplevel:withbytes:bytesperrow:)).
The format is linear `RGBA8Unorm`: row zero stays row zero, without channel swaps, alpha
premultiplication, gamma conversion or a vertical flip. A successful upload makes the texture
readable without a prior clear. The owner must serialize upload, clear and read. Clear waits
for its submitted GPU work, and these shared-texture CPU updates must not overlap GPU access;
see Apple's [resource synchronization guide](https://developer.apple.com/documentation/metal/synchronizing-a-managed-resource-in-macos).
Invalid input preserves the prior frame/readiness; an upload backend exception poisons the
context. This establishes a frame input boundary for later presentation; it does not translate
guest layouts, detile surfaces or connect guest GPU commands.

On non-Apple builds, `PB_METAL_ONLY` defaults to `OFF` and the existing Linux build/tests are
unchanged. Explicitly requesting either Metal option on a non-macOS platform fails rather
than silently creating unsupported targets. The existing Linux verification suite is preserved
for the normal GCC and Clang builds.

This native Metal library does not bridge ARM64 and x86-64 guest execution in one process.
Own-runtime Darwin services, the engine's real render/resource contracts, shader support and
a whole-code recompilation or separately established guest-execution strategy remain needed
for gameplay. `MTKView`/`CAMetalLayer` presentation is a future unit, not part of this clear test.
