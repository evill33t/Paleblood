# Process-private semaphore creation and deletion

Paleblood implements a deliberately narrow own-port policy for `sceKernelCreateSema`
and `sceKernelDeleteSema`. The supported creation call has a readable non-NULL empty
name, attribute tag 2, initial count 0, maximum count 1, NULL options and an eight-byte
writable output slot. Each object is process-private and has no waiters. This unit
creates and deletes records; it provides no wait, signal, priority ordering or host
semaphore primitive. Guest code must not use an object through any other operation.
An empty name is accepted only as this explicit port policy, without claiming the
console kernel treats it as unnamed or has the same namespace or ownership rules.

The supplied libkernel artifact (SHA-256
`b3e13917da667d01a353ccf49025d6b1308a32cf86f3f351f96b3a13cd5c7c2f`)
shows an early non-NULL-options return of `0x80020016`, before other argument access.
With NULL options it delegates creation to the kernel. On success its wrapper stores
a sign-extended 32-bit kernel result into exactly eight bytes and returns zero.
Delete forwards the full 64-bit handle by value, without accessing a caller slot.
The reached original caller supplies the narrow creation values above. These static
observations establish the wire, not kernel name/count validation, ID allocation,
resource cleanup, scheduling, firmware identity or equivalence with this host policy.
The supplied artifact's firmware/build match to the game is not established.

This port assigns positive int32 IDs from 1 through INT32_MAX, stores each as a
64-bit value, and never reuses an ID during the process lifetime. It never publishes
an allocation pointer. All records share a mutex; full-width Delete lookup prevents
an ID with different upper bits from matching. Successful Delete removes and frees
one record without mutating any borrowed guest slot. There is no shutdown hook:
unreleased records remain process-owned until process exit. Concurrent lifecycle
calls are serialized at table mutation; callers must coordinate ownership so no
consumer can use an object after deletion. No cross-process or async-signal calls
are supported, and no waiter/read-side lifetime protocol is claimed.

The exact observed non-NULL-options branch returns EINVAL without reading the name
or output. All other unsupported paths stop by operation name, including nonempty
or NULL names, other attributes/counts, exhausted IDs, allocation failure, stale or
unknown IDs, and synchronization failures. No arbitrary host errno or guessed
kernel error mapping is returned. These diagnostics do not promise console behavior
for invalid inputs; the only supported status path besides non-NULL options is zero
for a completed supported operation.

Publication occurs only after complete record registration, using an unaligned-safe
8-byte memcpy with no previous-slot read. The caller owns its output memory and must
keep it writable until publication finishes. Invalid output pointers are outside
this contract; a synchronous fault can leave a registered process-owned record,
as the recovered wrapper has no visible rollback. Publication and removed-record
free occur outside the table mutex. SIGALRM is blocked from before acquiring the
mutex until after publication/free and mutex release, then the prior mask is
restored. A pending boot timeout can take its nonlocal exit at that restoration
boundary without stranding the shared mutex or interrupting owned cleanup. This is
not protection against arbitrary synchronous faults, cancellation, other signals,
or process termination.

Owned tests exercise the real registry, concurrent distinct IDs, non-reuse,
unaligned eight-byte publication, surrounding canaries and by-value deletion. A
separate test-only translation unit injects allocation failure, reaches the actual
int32 exhaustion limit, rejects unsupported/unknown/full-width-mismatched IDs, and
raises pending SIGALRM during allocation/free to check restoration after completion.
Checks remain active in Release. They do not execute console code or establish
console scheduling behavior.

Build the ordinary Linux Release configuration, then run the selected tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pbboot test_syslib test_semaphore test_semaphore_failures
ctest --test-dir build -R '^(syslib|semaphore_lifecycle|semaphore_failures)$' --output-on-failure
```

Run those gates with GCC and Clang before a bounded actual boot. This standalone
unit is based on upstream 45f6f962; composition into a later progress branch must
preserve its existing runtime sources, Threads linkage and unrelated owned tests.
