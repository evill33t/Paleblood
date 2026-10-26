# macOS Metal platform unit

On macOS, CMake defaults to `PB_METAL_ONLY=ON`. This builds `pbmetal`, a native static library
with a C API and an Objective-C++ implementation linked to Apple's Metal and Foundation
frameworks. This path does not build `pbboot`, `pbloader`, `bbrt`, `bbgame` or the Linux tests.
The separate [owned Darwin startup path](own-runtime-darwin-startup.md) requires
`PB_METAL_ONLY=OFF`, `PB_DARWIN_GUEST_BOOT=ON` and an explicit x86-64 architecture.
That path does not connect guest graphics to Metal.

The Metal unit creates a device, command queue and RGBA8Unorm offscreen texture, submits a
render-pass clear/store, then reads pixels after command-buffer completion. It also accepts complete RGBA8 frames
through a synchronous CPU upload to the same texture. This first
shared-texture implementation requires a unified-memory Metal device. Successful CMake
configuration or compilation does not prove that a suitable device exists. A borrowed `CAMetalLayer` can now present completed frames. Game shader translation,
guest GPU commands and guest execution are not connected to this unit.

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
Complete Darwin services, the engine's real render/resource contracts, shader support and
a whole-code recompilation or separately established guest-execution strategy remain needed
for gameplay. Native frame presentation is an independent host primitive; it does not supply
the game renderer or a guest GPU bridge.

## Presenting completed frames

The caller creates a `CAMetalLayer`, attaches it to its own view/window, then calls
`rt_metal_offscreen_configure_layer`. Pass the Objective-C object using an unretained bridge to
`void *`. Configuration assigns this context's device, `BGRA8Unorm`, its exact pixel dimensions,
`allowsNextDrawableTimeout=YES` and `presentsWithTransaction=NO`. The caller still controls
bounds, `contentsScale`, `framebufferOnly`, attachment, resizing, window and application policy.
The library does not create an `NSApplication` or window, detach a layer, or restore its previous
configuration when the context is destroyed. Configuration and presentation require the main
thread. Serialize all context operations and all use or mutation of the borrowed layer.

After a successful upload or clear, call `rt_metal_offscreen_present`. It rechecks the layer
profile and uses a fixed fullscreen Metal shader to read the source texel at each fragment's
pixel coordinates. Source and drawable extents must match. The render pass performs no scaling,
vertical flip, channel swizzle or alpha blending. Logical RGBA output is stored by Metal in the
BGRA drawable format. Core Animation may still scale the result to the layer's bounds and apply
its display color processing; this API does not establish physical display pixel/color identity.

The optional `rt_metal_bgra_readback` captures the actual rendered drawable for diagnostics.
Set the layer's `framebufferOnly=NO` when requesting it. The implementation encodes a texture
blit into owned shared storage **before** presentation, then copies BGRA rows to caller memory
only after successful GPU completion. Caller padding is preserved and capacity may end at the
final pixel. `NULL` capture avoids readback allocation and copying, and permits the usual
framebuffer-only drawable. Invalid input and backend failures leave capture memory unchanged.
The implementation follows Apple's
[drawable readback ordering](https://developer.apple.com/documentation/metal/reading-pixel-data-from-a-drawable-texture);
a drawable cannot be read after presentation to verify its former contents.

`RT_METAL_OK` means the GPU commands completed and presentation was scheduled. It does not mean
the display presented that frame. A caller can observe a drawable's
[presented handler](https://developer.apple.com/documentation/metal/mtldrawable/addpresentedhandler(_:))
and a positive [presented time](https://developer.apple.com/documentation/metal/mtldrawable/presentedtime)
for that separate fact. The `timeout_ms` argument bounds the GPU wait after command submission.
Lazy shader/pipeline compilation is outside this bound, as is `nextDrawable`, which uses its
[system acquisition timeout](https://developer.apple.com/documentation/quartzcore/cametallayer/allowsnextdrawabletimeout).
`RT_METAL_NO_DRAWABLE` leaves the context usable. GPU failure or a GPU-wait timeout poisons it;
destroy the context before reuse. Timeout does not cancel submitted work, and a frame may still
present later. Command resources and strong layer/drawable callback references survive the wait;
no callback references the context, a stack semaphore address or caller capture memory.

The presentation test is separately opt-in because it opens and closes its own small window and
requires a logged-in graphical macOS session and a real Metal device:

```sh
cmake -S . -B build-metal-window -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DPB_METAL_ONLY=ON -DPB_ENABLE_METAL_SMOKE_TEST=ON -DPB_ENABLE_METAL_PRESENTATION_TEST=ON
cmake --build build-metal-window --parallel 2
ctest --test-dir build-metal-window -R '^metal_' --output-on-failure
```

The fixture runs within its own AppKit application event loop, owns its application/window
policy, observes genuine `CAMetalLayer` drawables, and
checks exact pre-present BGRA pixels, all channels/row order, capture padding, validation before
acquisition, framebuffer-only presentation without capture, and three separately verified positive presentation timestamps. It explicitly records a startup
submission after initial configuration and after the final capture-profile restoration; an
initial skipped frame is not counted as a verified presentation. Each checked frame must have
its own positive timestamp, independent of those startup observations. The fixture submits at
a fixed 10Hz while servicing AppKit events; it does not retry until a frame is displayed.
Application frame pacing remains with the caller, outside this synchronous primitive.
It fails on an unavailable GPU, missing presentation or pixel mismatch, with a 30-second CTest
limit. It does not capture the user's screen, verify scanout colors, translate guest graphics,
or force device loss, allocation exhaustion or hardware timeout. Ordinary Metal builds and the
existing smoke option do not register this window test.
