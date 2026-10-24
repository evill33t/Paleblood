# Own-runtime CPU topology

Paleblood exposes its own CPU topology. It does not claim the CPU mask of a console. The native
allowed-ID policy never infers host affinity from a core count; the separate explicit logical-slot
policy below declares virtual labels without a host-ID mapping. The current implementation covers the registered calling main thread;
guest thread creation and affinity setters remain unimplemented.

The mapping core and provider interface are independent of Linux. The current native provider
is Linux-only; other hosts, including macOS, fail clearly for native queries. The explicit Darwin
logical-slot option is a different policy, not a native affinity provider.
A Darwin provider must define its own honest CPU/scheduling policy instead of pretending that
Linux hard-affinity APIs exist there. This unit supplies no macOS execution or graphics backend.

Before guest code runs, the Linux backend queries that host thread's actual allowed CPUs using
a dynamically sized CPU set. By default, all allowed host CPU IDs in ascending order become
guest bits 0 through N-1. There must be between 1 and 64 IDs. A host ID may be sparse or greater
than 63: it is mapped to a guest bit, never copied or truncated. The boot prints the complete
mapping and labels it as port policy.

`BB_RUNTIME_CPU_MAP` optionally specifies an ordered comma-separated list of decimal host IDs.
It must be a permutation of the same entire allowed set. For example, a host allowed set
`5,130,4097` normally maps guest bits `0,1,2` to those IDs. An explicit `4097,130,5` reverses
that order. No setting silently discards CPUs or changes host affinity. If more than 64 CPUs
are allowed, narrow the host process's affinity externally before starting Paleblood. Invalid
settings and unsupported host queries stop before guest execution.

`scePthreadAttrGet` re-queries the actual calling registered main thread and snapshots its
translated mask. A later subset retains its original guest bit positions. A newly allowed
host ID outside the original map stops with a named diagnostic; it is not ignored. Attribute
getters then read the saved snapshot, not the live host state. The provider/context API used
by embedding hosts and unit tests has the same ownership and mapping rules.

The supplied plaintext libkernel image has SHA256
`b3e13917da667d01a353ccf49025d6b1308a32cf86f3f351f96b3a13cd5c7c2f`.
Static analysis of that exact image establishes the two-pointer SCE affinity getter, one
eight-byte store on success, low32 error translation, null/empty attribute EINVAL, and the
difference between a fresh unrestricted attribute and one filled by a thread-affinity query.
Its build/firmware compatibility with the game's bundled library has not been established.
No target library or raw decompiler output is included in the repository.

Paleblood chooses an internal 16-byte CPU-set policy with at most 64 mapped guest CPUs. A fresh
attribute remains unrestricted, so its public low64 affinity is all ones. A copied main-thread
attribute contains the actual mapped snapshot. This internal width and topology are declared
port choices; the target kernel's CPU-set size and default numeric mask remain unmeasured.

The public getter publishes exactly eight bytes with `memcpy`, permitting unaligned or aliased
valid output storage. A null/empty attribute returns `0x80020016` and leaves output unchanged.
The observed target does not check output nullness on success, so no new null-output EINVAL is
invented. Backend query failures or an unsupported thread produce named diagnostics instead
of invented BSD errors or a fabricated mask. Other attribute behavior, including the unresolved
detach-state policy, remains as documented previously.

For the native-allowed policy, future create/set/current-CPU/scheduler interfaces must preserve
this same guest-bit mapping and apply the corresponding host CPU set. A fixed game mask must retain its logical meaning.
Zero-mask validation and the supplied nested setter's errors require their own contract review;
this unit does not register a setter or pretend to support guest worker threads.

`test_affinity` uses only mocked providers. It covers sparse/high host IDs, explicit mapping,
bit63, rejected truncation, fresh versus captured masks, live subset changes, unknown CPU
expansion, eight-byte canaries, alias output, invalid-attribute errors and provider failure.
Actual Linux backend/build/boot validation is separate from these planned unit tests.

## Explicit Darwin logical slots

Darwin boot may opt into `--guest-cpu-count N` for decimal N in 1..64, or
`--guest-cpu-count host` to read and validate `hw.logicalcpu` once at startup. The default remains
identity-only registration and a named unsupported diagnostic at `scePthreadAttrGet`. Linux's
native query, allowed-ID map, environment permutation and default boot behavior are unchanged;
the new command-line option is accepted only on Darwin.

This option supplies immutable, unpinned logical slots 0 through N-1. Slot numbers are not native
processor IDs, an allowed-CPU set, affinity tags, performance-core classes or a console topology.
Explicit N is a user-declared guest topology and may oversubscribe the host. `host` supplies an
enabled logical-core count snapshot, not per-thread eligibility or a pinning guarantee. It fails
on query failure, malformed data or a count outside 1..64; there is no clipping or fallback.
The selected domain stays fixed for the main-thread lifetime even if host availability changes.

Resolution occurs after the import-listing early return, before backing/image/stack allocation.
`--imports` does not query the host count or register a CPU policy. Normal execution rejects
`BB_RUNTIME_CPU_MAP` when the logical option is selected, including an empty environment value;
its native host-ID syntax is never reinterpreted as logical slots. Invalid, duplicate or missing
command-line values fail before startup. The boot log identifies the count, its source and the
absence of native affinity.

The embedding entry point `rt_thread_register_main_logical` checks a separate immutable slot
value before publishing main-thread identity and policy. It does not call a hardware provider.
Only the calling registered main thread may capture it through `scePthreadAttrGet`. Native
embedding providers retain their existing separate domain, including on Darwin. A logical
snapshot contains low N bits set; a fresh unrestricted attribute still returns all 64 low bits,
not a mask clamped to N. Attribute publication follows the existing exact eight-byte unaligned
and alias-safe getter contract. All policy reads succeed before any AttrGet field is replaced.
Unknown detach state, native affinity setters, thread creation, current-CPU queries and scheduler
semantics remain unresolved. No new guest imports are registered.

The independent [slot component](own-runtime-logical-cpu.md) supplies count validation and
read-only host count errors without changing native CPU types. New Release-active registry
fixtures cover 1/3/63/64, failed/duplicate registration, immutable snapshots, fresh attributes,
output canaries, unaligned/aliased publication and the existing null-output fault. Existing native
provider tests run unchanged. Owned Darwin boot fixtures check all four counts through normal
public imports. A test-only wrapper of the exact boot translation unit supplies a deterministic
count/failure so `host` resolution, no query for explicit values/import listing, and conflict/error
paths are measurable without production injection APIs. Normal Release validation passed all six
macOS CTests, including the nine existing and seven new owned boot test methods. Linux GCC 14
and Clang 19 each passed seven selected CTests and 34 existing Python methods, with zero skips.
All 276 source hashes remained unchanged. These tests cover owned fixtures; complete original
startup, guest worker threads, scheduling and gameplay remain unestablished.
