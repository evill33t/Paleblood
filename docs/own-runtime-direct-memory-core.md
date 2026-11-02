# Explicitly owned direct-memory backing pool

This core API owns a caller-sized temporary backing file, a sorted first-fit allocation ledger,
and non-clobbering aligned shared mappings. It validates full-width geometry, publishes outputs
only on success, permits aliases of live allocations, and retains ownership when cleanup fails.
The public API declares supported caller serialization, 16 KiB geometry, CPU read/write mappings,
and whole-allocation release. These are Paleblood host policies, not inferred console defaults.

This independently usable component does not replace upstream `kernel_memory.c`, select an
implicit capacity, change guest virtual-memory queries, register duplicate syscalls, or add a boot
option. Upstream now has broader direct-memory, flexible-memory and GPU-query behavior; adapting
its syscall wires to an opt-in pool requires separate compatibility work.

Owned core and injected host-failure tests exercise alias byte sharing, alignment, geometry,
allocation/release, malformed capacities, output preservation and retryable cleanup. The tests
use synthetic data and normal host temporary files, with no firmware or game data.
