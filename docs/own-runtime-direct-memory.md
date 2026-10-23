# Configured direct memory

`pbboot --direct-memory-size BYTES` opts into a Paleblood-owned pool. The argument is one
positive decimal integer, a multiple of 16384 and representable as a POSIX file length. There
is no implicit console capacity. Reaching any of these four imports without configuration stops
by its name and explains the option. Import listing does not create backing, even when the
option is supplied. Initialization errors stop before guest entry.

The capacity is the logical length of one anonymous temporary file (`tmpfile`, `fileno`,
`ftruncate`). It is not a resident-RAM reservation or a promise that later page faults can always
be satisfied. Host backing limits, storage exhaustion, signal delivery, and GPU coherency remain
host concerns; no graphics policy is provided. The core supports POSIX hosts whose page size is
a power-of-two divisor of 16 KiB. This does not establish the console's `getpagesize` result.
The composed boot harness still has its existing Linux/x86 dependencies.

## Supported policy

| Import | Wire and Paleblood behavior |
|---|---|
| `sceKernelGetDirectMemorySize(void)` | Full 64-bit capacity of the configured file. No returning status and no invented console constant. |
| `sceKernelAllocateDirectMemory(start,end,length,alignment,type,out_offset)` | Full-width byte offsets and sizes, end-exclusive `[start,end)`. Sorted first-fit physical ledger; no overlap. Type 0 only. Writes exactly eight bytes to a valid, possibly unaligned output slot only after success. |
| `sceKernelMapDirectMemory(inout_address,length,protection,flags,offset,alignment)` | Valid eight-byte input/output slot, full-width offset and size. CPU read/write protection 3, nonfixed flags 0 only. One map must fit inside one live allocation. Shared aliases refer to the same file bytes. The input address is only a hint; no console default hint is reproduced. |
| `sceKernelReleaseDirectMemory(offset,length)` | Release one exact unmapped allocation. Partial release and release with a live mapping stop by name. No guest unmap import or `GetDirectMemoryType` is added. |

Allocation ranges, offsets and lengths are multiples of 16 KiB. Zero alignment means 16 KiB;
other alignments must be powers of two at least 16 KiB. Mapping alignment is at most 1 GiB.
All additions and range endpoints are checked before use. Allocation alignment may exceed the
pool size; that either finds a valid aligned gap or returns exhaustion. A released allocation's
backing bytes persist on reuse; console reuse zeroing is unknown.

A map first reserves a fresh `PROT_NONE` anonymous range using a nonfixed hint. It chooses an
aligned subrange and replaces only that owned reservation with `MAP_SHARED|MAP_FIXED` file
pages. Existing mappings are never replaced. Prefix/suffix guards stay owned until cleanup.
Every successful map has its own ledger, including aliases. On failure the output slot is
unchanged. A failed cleanup keeps the owned reservation recorded for destruction rather than
losing it. A theoretical host allocation at address zero is cleaned and rejected as unsupported.

Pool critical sections defer only SIGALRM in the calling thread. They save its exact prior mask
before acquiring the mutex, unlock before restoring it, and may then deliver a pending timeout
that returns to the boot handler. This prevents that timeout from stranding the mutex. It does
not bound POSIX call duration or fix the boot harness's global/cross-thread signal routing.

The core has an exact-map unmap operation for owned consumers and tests. It does not register
an additional guest API. Pool destruction unmaps all maps, closes the file and frees ledgers;
failed unmaps retain the pool for a later cleanup attempt. A final `fclose` error is reported
with the pool consumed. The boot harness registers cleanup before mapping/entering guest code.
The registry serializes pool operations and teardown; direct core consumers serialize their own
calls. Configure before guest entry and destroy after its consumers have quiesced. Arbitrary
invalid guest pointers are outside this valid-slot contract and may fault; borrowed slot reads
and writes occur outside the registry mutex so a caught guest memory fault does not strand it.

## Errors and evidence limits

Invalid geometry and nonallocated map ranges return own-policy `0x80020016`
(EINVAL). A full physical ledger, failed ledger allocation, or failed host map returns
own-policy `0x8002000c` (ENOMEM). Backing creation/cleanup failures and unsupported types,
protections, flags or release shapes stop by the operation name. Host errno values are not
blindly mapped to target status numbers. Error precedence for multiply invalid requests is
Paleblood policy and has not been measured on the console. Core-owned null handle checks are not
added to the guest wires: Map unconditionally reads its borrowed address slot before checking
mapping modes or geometry; Allocate touches its borrowed output only after successful allocation.
A null/inaccessible guest slot therefore faults at that access, including a successful allocation
whose result cannot be stored. Failed allocations leave even a null output untouched.

The supplied plaintext libkernel wrappers establish these four argument/return wires, their
success-only stores, a cached size obtained from `/dev/dmem` via ioctl, and the normal versus
alternate mapping paths. They do not establish kernel validation, allocation placement, range
endpoint rules, total capacity, alias-release semantics, other memory types, GPU access, target
mapping defaults or the feature/configuration-dependent alternate map mode. Those remain
explicitly unsupported. A retained title call site first requests 116 MiB aligned to 2 MiB,
then maps CPU read/write, flags zero, and releases on map failure. That first request does not
establish the capacity needed by the whole game. These sources contain no title-specific rule.

## Owned tests

The four C test targets use checks that remain active in Release. They cover strict parsing,
sparse full-width offsets, first fit/alignment, exhaustion and overflow, unaligned output
canaries, shared aliases, occupied hints, exact release/reuse, destruction and real registry
calls, including child-isolated borrowed-slot faults and failed-allocation no-store behavior.
The fault target compiles the same core with test-only libc aliases to inject allocation,
backing/map and cleanup failures and check ownership rollback. It adds no production hooks.
A child-isolated timeout regression raises SIGALRM during allocation-ledger creation and
recovers through sigsetjmp, then checks query/shutdown, ledger cleanup, unchanged output and
signal masks with a short watchdog. The Python test uses an owned synthetic ELF to query/allocate/map or release through real import
bindings, and checks missing/invalid/duplicate configuration and import-only behavior.

Owned test inputs do not establish console differential equivalence, gameplay or the capacity
required by the whole title.
