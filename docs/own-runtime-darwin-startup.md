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

## Bounded rwlock composition

This source composition adds the published default unnamed rwlock lifecycle, writer and balanced
nonrecursive reader adapter. The guest holds an eight-byte pointer slot; the host lock and
reader/writer ownership metadata remain private. It supports `scePthreadRwlockInit`, Destroy,
Rdlock, Wrlock and Unlock only in the initialized, default, unnamed, balanced own-thread domain.
See [rwlock lifecycle](rwlock-lifecycle.md) for the exact port policy and unsupported boundaries.
No console object layout, recursion, timed/lazy behavior, waiter fairness or error translation is
inferred from host pthread behavior.

The two dedicated `test_rwlock` and `test_rwlock_failures` targets are added to the selected Darwin
path as `rwlock_lifecycle` and `rwlock_failures`, each with the existing ten-second CTest bound.
Their published source is unchanged. They use actual registry entries and real host-thread
reader/writer exclusion, plus an exact-source test-only failure/interleaving translation unit.
All six earlier startup/logical CTests remain registered, so the proposed Darwin gate is now
those six plus these two. The legacy broad `test_syslib` target is not enabled on this path:
its default main-thread registration does not select the explicit logical CPU policy.

Writer identity and reader ledgers use ordinary host `_Thread_local` storage, not the guest TCB
or a new GS/FS write. Every thread must release its own holds before exiting. Destroy requires
external quiescence with no callers or waiters. A process-terminal timeout/cancellation may
abandon a candidate record or host lock; continuing use or automatic lock cleanup after such
abandonment remains unsupported. The existing Darwin TLS qualification and per-thread signal
recovery are unchanged, and this unit adds neither guest thread creation nor cross-thread signal
routing.

Validation used a normal whole-x86-64 Darwin Release build: all eight selected CTests passed,
with zero skips. Linux GCC and Clang each passed the three scoped syslib and rwlock CTests.
All source hashes remained unchanged. One subsequent original macOS game boot passed the lock
calls and stopped at `sceKernelCreateSema`, returning to `eboot+0x1fc2443`, with no fault and
owned TLS/signal state released. Graphics and gameplay have not been reached. Semaphore support
remains a separate integration unit. Existing logical CPU, direct-memory and Metal policies remain
unchanged.

## Process-private semaphore lifecycle

This source composition adds the published bounded `sceKernelCreateSema` and
`sceKernelDeleteSema` policy. Supported creation uses a readable non-NULL empty name, attribute
value 2, initial count 0, maximum count 1, NULL options and a writable eight-byte output slot.
It assigns process-private positive int32 IDs and publishes each in eight bytes; IDs are never
reused during the process lifetime. Delete accepts the full-width ID by value and changes no
borrowed caller slot. The observed early non-NULL-options EINVAL branch remains independent of
other argument access. Other unsupported modes and unknown errors stop by API name.

This is record creation/deletion, not a host semaphore primitive or console namespace/count
implementation. There is no wait, signal, guest worker, priority or scheduler support. See
[semaphore lifecycle](semaphore-lifecycle.md) for ownership, error and lifetime boundaries.
The implementation does not use sem_init, add a Darwin semaphore shim, or change guest TLS.
Unreleased records remain process-owned until exit; no new shutdown hook is installed.

The table mutex and SIGALRM deferral retain the exact published implementation. The old thread
mask is restored only after releasing the mutex and completing output publication or removed-
record free. A pending boot alarm may take its nonlocal exit at that restoration boundary.
This does not bound host POSIX calls, make arbitrary faults/cancellation recoverable or solve
cross-thread signal routing. Invalid output pointers can fault after record registration and
leave a process-owned record; no invented rollback or host errno translation is added.

The dedicated `test_semaphore` and `test_semaphore_failures` targets are registered on the
Darwin path as `semaphore_lifecycle` and `semaphore_failures`, each with its existing ten-second
CTest bound. Their exact published source checks concurrent distinct IDs, non-reuse, full-width
Delete, unaligned publication/canaries, ownership failures, the real int32 limit and pending
SIGALRM restoration. All eight prior startup, logical CPU and rwlock CTests remain registered;
the proposed ordinary Darwin gate now consists of those eight plus these two. The broad legacy
syslib test remains outside the selected Darwin path.

A normal whole-x86-64 Darwin Release build passed all ten selected CTests, with zero skips.
Linux GCC and Clang each passed the three scoped syslib and semaphore CTests. All source
hashes stayed unchanged. One subsequent original macOS game boot passed semaphore startup and
stopped at `libScePosix!gettimeofday`, returning to `eboot+0x20e2708`, with no fault and owned
TLS/signal state released. Graphics and gameplay have not been reached. Existing CPU,
direct-memory, rwlock, TSD/signal, loader and Metal behavior is unchanged. This unit adds no
gettimeofday provider or other time policy.

## Bounded host realtime composition

This source composition adds the published `gettimeofday` adapter in `libScePosix` and
`libkernel`. Supported calls provide valid writable sixteen-byte output and NULL timezone.
The host query writes a local native timeval; explicit field conversion publishes two signed
64-bit fields, seconds then microseconds. Validation requires microseconds in 0..999999 before
exact sixteen-byte publication, including unaligned output. Native padding and Darwin's narrower
microsecond field are not copied. Host errno is preserved.

Host realtime is an explicit port policy, not a console-clock, precision or monotonicity claim.
NULL output, non-NULL timezone, failed host query and malformed microseconds stop by name before
publication, without guessing guest errors. See [bounded realtime service](own-runtime-gettimeofday.md)
for the consumed wire, public API basis and target-kernel limits. No timer, module-loading or
scheduler implementation is supplied by this adapter.

The exact published `test_timeval` and `test_timeval_failures` sources are registered as `timeval`
and `timeval_failures`, each with its existing ten-second CTest bound. They check both registry
aliases, unaligned output/canaries, full-width signed seconds, poisoned native padding,
microsecond endpoints, errno preservation and rejection before publication. They make no
wall-clock ordering assertion. The ten previous Darwin CTests are unchanged; the proposed gate
now contains those ten plus these two. The broad legacy syslib test remains outside this path.

A normal whole-x86-64 Darwin Release build passed all twelve selected CTests, with zero skips.
Linux GCC and Clang each passed the three scoped syslib/timeval CTests; the corrected failure
fixture then passed separately on both compilers. That fixture observes errno at the adapter's
fatal boundary because the measured test recovery interval changes it. Production code is
unchanged by the fixture correction, and all source hashes stayed unchanged within each run.

One subsequent original macOS game run reached the named unsupported
`sceSysmoduleLoadModule(0x00b4)` path and terminated with SIGABRT. No status JSON was emitted;
this run establishes no milestone or return site. The existing five sensor-module declarations
were retained. Existing Mach-O registry/startup, TSD/signal, CPU, direct-memory, synchronization
and Metal code are unchanged. Complete original startup, system-module behavior, graphics and
gameplay remain unestablished.
