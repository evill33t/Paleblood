# Default unnamed rwlock lifecycle, readers and writers

`runtime/syslib/kernel_rwlock.c` registers `libkernel!scePthreadRwlockInit`,
`scePthreadRwlockDestroy`, `scePthreadRwlockRdlock`, `scePthreadRwlockWrlock`
and `scePthreadRwlockUnlock`. This is a bounded lifecycle and nonrecursive
own-reader/writer adapter. Attributes, named kernel objects, try/timed operations, and console scheduling
and fairness remain outside it.

The imported wire is an eight-byte handle stored in caller-owned memory.
Init takes `(handle_slot, attribute_slot_or_NULL, name_or_NULL)` and returns a
low-32-bit status. With NULL attributes and NULL name, it allocates a private
host object containing a real `pthread_rwlock_t` and initializes it with default
host attributes. Only the pointer is published in the guest slot; neither the
host inline lock nor a guessed console object layout is written there.
Default host attributes and host cleanup are explicit port choices.

Init does not inspect the previous slot value. Allocation failure returns
`0x8002000c` and leaves the slot untouched, including when its address is NULL.
A successful initialization writes exactly eight bytes, using `memcpy` so a
valid slot need not be aligned. The caller must supply writable slot memory on
success; no fabricated NULL-slot `EINVAL` is returned. A host initialization
failure frees the unpublished allocation and stops through `rt_fatal`, naming
`scePthreadRwlockInit`; its guest error translation is not established.
Non-NULL attributes or names stop by name before allocation or output access.

Destroy accepts only a valid initialized object created by this adapter, after
all users, waiters and API calls have finished. A recorded writer or nonzero reader total stops by name
before host destruction. These held-object checks are port policy and cannot
establish that no acquisition, waiter or API call is in flight. A slot containing
zero returns success without a write. For a live object, the adapter first checks
host destruction. On success it writes the exact eight-byte tombstone value
`1` and then frees owned storage. On an unexpected host error it keeps both the
original slot and allocation reachable, then stops by name. That error-path
ordering is an ownership policy, not a recovered console busy/error rule.
A slot containing tombstone `1` stops by name because the original diagnostic
helper's behavior is unresolved. Outer NULL/inaccessible slot addresses,
foreign/freed handles, concurrent destruction, and direct host access to the
opaque object are outside the supported contract. There is no guessed errno
for those cases.

## Writer operations

Wrlock and Unlock each take one borrowed eight-byte handle slot and leave every
byte of that slot unchanged. They require a previously initialized owned object.
Wrlock uses the host's blocking `pthread_rwlock_wrlock`; normal host contention
and exclusion are port policy, not recovered console fairness or kernel behavior.
A private atomic record holds the address of a host `_Thread_local` byte as an
owner token. This does not use a guest TLS layout or a `pthread_t` encoding.
Wrlock records the token only after host success; an already recorded writer
on the same host thread stops by name rather than guessing recursive behavior.
The writer Unlock branch supports only that recorded writer. A host failure stops by name; its
error branch writes neither the record nor the guest slot. No continuing state or retry is promised.
After host success, compare-and-exchange clears only the old token, preserving
a next writer that may already have acquired and published its own record.

The host thread must unlock before it exits. Calls must not reenter on the same
host thread, including through a signal handler; token lifetime and ownership
rely on those preconditions. Foreign handles, unlocked/wrong-owner
unlock, stored-NULL lazy initialization and tombstones stop by name or remain
outside the valid-object contract. No guessed EPERM/EINVAL return is supplied:
the recovered invalid paths invoke diagnostic behavior whose return is unknown.
This adapter does not implement guest thread creation or full guest TLS.

## Own-reader operations

Rdlock takes the same borrowed eight-byte slot and leaves it unchanged. It requires
an initialized owned object and uses host pthread_rwlock_rdlock with default host
contention policy. Before any host acquisition it rejects this thread's writer
(write-to-read conversion) or an existing read hold for the same object (recursive
read). Wrlock rejects a same-object own-reader hold (read-to-write conversion).
No recursive, upgrade/downgrade or console deadlock error policy is invented.

Each host thread has a private linked ledger keyed by the owned object pointer,
not the borrowed slot address. One heap node records one balanced read acquisition;
different objects may be read-held together. The candidate node is allocated before
host acquisition. Allocation failure stops by name before host calls or metadata
change. A host Rdlock failure frees that unpublished node, then stops by name,
without a guessed guest errno, rollback unlock or continuing-state guarantee.
Success links the node, increments an atomic per-object reader total and returns
zero, without a fallible allocation after acquisition.

If Unlock has no own-writer token, it requires this host thread's reader node.
A missing node stops by name before host unlock. Host unlock failure retains the
node, total and guest slot, then stops by name; no continuing use or retry is
promised. Success removes just that node, atomically decrements the total and frees
it. This branch never clears a writer token: another writer may have acquired and
published after host unlock. Concurrent reader increments are preserved too.

The total conservatively counts acquisitions until their successful unlock
bookkeeping finishes. It is not the console low29 reader count, admission masks or
aggregate guest TCB field. One allocated node per hold bounds the count below
size_t capacity. Destroy still requires external quiescence: no active operations,
waiters or possible users. Concurrent destruction cannot be made safe by a zero
reader total. Every thread must release all holds before exit; there is no implicit
thread-exit unlock or reader-ledger cleanup.

No signal mask or shared metadata mutex spans blocking host acquisition. Like the
writer path, a process-terminal boot timeout/nonlocal exit or cancellation can
abandon a candidate or host lock before bookkeeping completes. Continuing runtime
use or cleanup after such abandonment is unsupported; process exit reclaims owned
memory. This is not signal-safe recovery or guest TLS/thread-creation support.

## Evidence and limits

The supplied plaintext `libkernel.sprx` has SHA-256
`b3e13917da667d01a353ccf49025d6b1308a32cf86f3f351f96b3a13cd5c7c2f`.
Static instruction analysis establishes default unnamed Init's success-only
pointer publication and allocation error, and default Destroy's stored-zero
no-op and tombstone-before-free success behavior. Additional static analysis
establishes the initialized writer and own-writer unlock fast paths: one handle
slot argument, unchanged slot bytes, and low32 success status zero. Further static
analysis establishes the initialized reader increment and ordinary reader-release
decrement success paths. The reader core also updates an aggregate guest thread
field; this host ledger is a separate explicit ownership policy and does not copy
that layout or claim equivalent recursive/contended behavior. The artifact does not identify the
module's firmware/build or prove compatibility with every other shipped
module. No target library code or raw decompiler output is included here.

The primary custom
[OpenOrbis pthread header](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/b458dfd9d2f5c40aa249b127c9b9b4488afdc686/include/orbis/_types/pthread.h)
uses platform pthread typedefs. Its
[libkernel header](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/b458dfd9d2f5c40aa249b127c9b9b4488afdc686/include/orbis/libkernel.h)
declares the three Init arguments and Destroy handle pointer.
Those homebrew declarations corroborate the named interface. Its platform
typedefs and placeholder declarations do not establish the eight-byte wire,
console object layout, or all API behavior; the byte widths here come from the
supplied binary's instructions. Named registration, non-NULL attributes,
held-lock destruction, priority, fairness, recursive/timed operations and complete
computed callers remain
unproved and unimplemented.

## Owned tests

The normal optimized build adds two bounded CTests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target test_rwlock test_rwlock_failures --parallel 2
ctest --test-dir build -R '^rwlock_(lifecycle|failures)$' --output-on-failure
```

The lifecycle test calls the actual registry entries, keeps two distinct owned
handles alive together, checks repeated write/unlock/lifetime operations and
unaligned borrowed slots with canaries, and verifies the stored-zero Destroy
no-op. Main and one joined host worker serialize 128 shared updates under the
writer lock, with atomic overlap checks and no timing/fairness assertion. Additional
owned workers demonstrate simultaneous reader admission, balanced reader release
and writer exclusion; different read-held objects exercise non-LIFO ledger unlink.
No sleep, timing or fairness assertion is used. The
failure test uses aliases only in its own translation unit to inject allocation
and host failures, check ownership and unchanged slots, prove tombstone-before-
free destruction, and check named unsupported paths. An injected next-writer
publication proves the old unlock's CAS preserves the new record. Reader aliases
check candidate-allocation and host failures, retained ownership, unsupported
paths before host calls, and independent next-reader/next-writer handoffs. All checks
remain active under Release. These synthetic owned tests do not execute guest
machine code or establish console synchronization semantics.
