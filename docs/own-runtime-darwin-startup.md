# Owned x86-64 Darwin startup

This unit runs Paleblood's own `pbboot` in a whole x86-64 Mach-O process. On an ARM Mac it
requires an installed x86-64 translation runtime. The owned tests use synthetic ELF fixtures;
no original game or system-module bytes are included.

The default macOS configuration remains `PB_METAL_ONLY=ON`. The separate, explicit selection is
`PB_METAL_ONLY=OFF`, `PB_DARWIN_GUEST_BOOT=ON`, `CMAKE_OSX_ARCHITECTURES=x86_64`. ARM64, universal,
missing explicit architecture, mixed Metal/boot selection and a Metal smoke-test request on the
boot path are rejected. The selected targets are `pbboot`, `pbloader`, `pbdirect_memory` and
`test_boot_darwin`, `test_boot_darwin_zero_map`, plus the explicit logical-policy test targets. They build C startup and the in-image syslib registry, without the Linux probe,
C++ game plugin, guest graphics, shader translation, Metal bridge or GPU test. Linux keeps the
previous normal targets and test registration.

Startup owns one public pthread key and the executable's variant-II TLS template/TCB. The
candidate key range is metadata, not a universal Darwin ABI. Before any image rewrite, the main
thread binds two distinct live owned pointers and NULL, compares public pthread storage with
our ten-byte GS-slot read, then binds and checks the real TCB. The read is RW-to-RX owned code,
called on an owned stack through the same initialized thread-local fault/timeout recovery used
for entry. A fault, mismatch, unsupported candidate, allocation or protection failure stops
startup; no guessed offset is used. Cleanup clears the key before deleting it and freeing its
TCB. A binding is not inherited/qualified on another thread. Guest thread creation and cancellation
are not supplied by this unit.

Only after that check does the delivered loader replace each exact nine-byte
`mov rax, fs:[0]` pattern in file-backed executable load bytes with the equal-length
`mov rax, gs:[key*8]`. It is a pattern scan, not instruction-boundary decoding: embedded matches
can be changed, and other FS instructions, TLS access sequences, arbitrary GS uses and module
TLS resolution are not established. Host GS base and GS:0 are never written. Host pthread/TLS
remains owned by Darwin. Image/trampoline executable mappings retain the existing runtime's
unhardened-process protection policy; this unit does not add entitlements or bypass a denied
mapping. The qualification code uses a distinct RW-to-RX allocation.

Mach-O assembly uses the C symbol underscore, saved fault PC uses Darwin's x86 ucontext, and
stack mapping omits Linux's `MAP_STACK`. Recovery is initialized per calling thread. Signals on
an unprepared thread cannot jump into another thread's guest context; non-alarm faults terminate.
`pbboot` owns SIGALRM, cancels it after each scope and ignores a late own alarm between scopes.
Teardown blocks/drains its pending alarm, restores its saved handlers/mask and alternate stack,
and releases the key/TCB. For an originally disabled Darwin alternate stack, restoration preserves
its saved pointer/flags and supplies the already accepted 64 KiB size (the saved zero-size request
was independently observed to fail). An enabled old stack is restored exactly. An unrecoverable
host abort/fault outside the owned scope still has process-exit cleanup only. Images, import data
and the guest stack retain their existing process lifetime; the cleanup log specifically describes
TLS and signal state.

By default Darwin main-thread registration supplies only the owned identity and stack.
`scePthreadSelf` therefore has a real runtime object, while `scePthreadAttrGet` stops by name.
The separate explicit `--guest-cpu-count N|host` option registers immutable unpinned logical slots;
see [CPU topology](own-runtime-cpu-topology.md) for its declared policy and count limits. These
slots provide a captured attribute mask and never promise native CPU pinning. Native affinity
remains unsupported. Linux's topology provider and default registration path are unchanged. Darwin also rejects the absent adaptive-mutex policy by name;
ordinary existing providers are compiled, not newly proven equivalent to all console behavior.
The configured direct-memory pool remains an explicit own-port policy, not a console capacity.
There is no new syscall trap, kernel page-size policy or general exception emulation here.

The original startup unit supplied three owned CTests. The Release-active C test exercises rejected template and
wrong-reader rollback, actual main-thread TCB qualification, owned UD2 saved-PC recovery, one
bounded spin timeout, and key/TCB/handler/alternate-stack cleanup. It includes the exact production
boot translation unit, without production fault-injection hooks. The Python test writes owned
ELFs and checks stack entry, template/zero-fill through rewritten TLS, module-before-entry order,
return, exit callback, unimplemented import, fault, timeout, no-execution import listing, and a
named affinity stop. Tests use explicit checks rather than `assert` disabled by Release. The
fixture builder's format sanity assertion is test-data validation, not a success oracle for guest
execution. The isolated third test includes the exact TLS helper with test-only mapping/allocation
aliases and a tight fake public-key API. A synthetic NULL mapping followed by an unmap failure
must retain its nonzero owned size, reject reprepare without another allocation, and allow later
successful/idempotent cleanup. No actual page-zero mapping, GS read or platform key is used by
that failure test. No test compiles a plugin or uses a proprietary fixture.

Before the new logical-policy integration, Release configuration and compilation passed with
Apple Clang on the measured ARM Mac under
x86-64 translation. All three CTests passed, including nine Python-owned startup cases, without
skips. Normal Linux GCC and Clang builds also passed twelve selected CTests and 68 existing
Python methods, with unchanged source hashes. These results cover the owned startup cases on
the measured hosts; original-game startup, universal Darwin TSD layout, complete guest TLS,
GPU submission and gameplay remain unestablished.

The logical-policy integration adds native-mock and logical-registry CTests plus a separate owned
boot-policy Python suite. The normal boot target is used for explicit N cases; an exact-source
test-only count wrapper covers deterministic host-query and no-query/error paths. The original
nine startup fixtures remain unchanged, including the default named affinity diagnostic. Release
configuration/build and all six macOS CTests passed, including 16 owned Python methods, without
skips. Linux GCC 14 and Clang 19 also passed 14 selected CTests and 68 existing Python methods
in total. All 276 source hashes remained unchanged. These results establish owned startup
fixtures on the measured hosts; complete original-game startup and graphics remain unestablished.
The default macOS Metal selection, whole-process x86-64 requirement, TLS qualification and signal
ownership rules above remain unchanged.
