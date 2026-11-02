# Owned module TLS

The boot harness assigns a TLS module ID to each mapped ELF in object order, starting
at one. It now prepares a separate TLS block for every nonempty template before any
module entry runs and implements `libkernel:__tls_get_addr` over that thread's blocks.
The executable's block borrows the already initialized static TLS allocation, so a
lookup and the existing thread-pointer access see the same bytes. The base is the
thread pointer minus the **rounded allocation size**, rather than minus logical
`p_memsz` when those sizes differ.

Each other block captures its initialization bytes from the mapped, relocated
image, zero-fills the remaining logical size, and satisfies the template alignment.
An empty template has no addressable block. All allocations happen during context
preparation; lookup performs no allocation, locking, guest calls or binding changes.
Contexts belong to their creating host thread. Binding seals their layout; unbinding
and rebinding preserve values, and destruction never releases borrowed storage.
New guest threads must prepare their own context before they can use this provider.
This change does not implement guest thread creation.

## Evidence and supported boundary

The supplied `libkernel.sprx` has SHA-256
`b3e13917da667d01a353ccf49025d6b1308a32cf86f3f351f96b3a13cd5c7c2f`.
Its `__tls_get_addr` export has NID `vNe1w4diLCs` at ELF virtual address `0x289c0`.
REA/Ghidra analysis retained the complete observed body: 1,751 bytes, 396
instructions, in ranges `0x289c0..0x28ad5`, `0x28ae0..0x28be8` and
`0x28bf0..0x290a7` (ends inclusive). The larger 1,768-byte unwind span includes
17 bytes outside that body. The original contiguous-body expectation failed;
the successful provider output and complete observed ranges were retained without
another query. Raw evidence digest:
`56fbc13a84a551cd5f7a1172fc0fafcd9475e39f49d8b273eb37846941ff47c8`.
No supplied firmware was executed.

That function reads two consecutive 64-bit words from its first argument: module
word and offset. The observed native implementation consults its TCB/DTV and linker
state, uses low bits for some indexing, and forwards the full module word in one
mode. Its return calculation masks a slot word to 48 bits and adds the full offset;
it does not check that offset against template size. Invalid native states lead
to diagnostics and `INT 0x44`, rather than an established ordinary error return.
Its generation updates, lazy allocation and linker helpers are not reconstructed.

Two adjacent native routines were also analyzed: `0x2a9a0` (767 bytes, 194
instructions) and `0x2aca0` (582 bytes, 145 instructions). Neither directly derives
a template-residue offset or adjusts the allocator's returned base. The allocation
arms explicitly prepare the logical size; register consumption and side effects of
the lower selector, helper and callback remain unproved. These observations do not
establish their final allocation alignment or universally rule out a residue policy.
The supplied title libc has `p_vaddr % p_align == 8`, while the supplied firmware
LibcInternal has residue zero. This host profile uses a declared-alignment block
base and does not derive a residue from the initialization image's virtual address.
Compatibility with all native allocation modes remains unresolved.

Paleblood provides an **owned eager allocation policy** using the established
module/offset request, rather than reproducing that internal PS4 linker layout.
IDs and offsets stay full-width. A known ID and offset up to logical `p_memsz`
return the block address; equality returns a one-past pointer without dereferencing
it. Unknown IDs, missing thread contexts and larger offsets stop by the name
`__tls_get_addr`. Those bounds and diagnostics are explicit host support limits,
not claims about native error codes or native bounds checks. No PS4 DTV generations,
lazy loading, unloading, alternate native modes, or allocator ABI are implemented.

The generic initialization model is also described in the original
[TLS ABI paper](https://www.akkadia.org/drepper/tls.pdf) and the
[Oracle linker documentation](https://docs.oracle.com/cd/E19120-01/open.solaris/819-0690/chapter8-70/index.html).
No other PS4 runtime implementation was used.

## Input checks and owned verification

The loader rejects duplicate TLS headers, file sizes exceeding logical size,
out-of-file initialization data, non-power-of-two alignment, and templates whose
initialized bytes do not match an admitted segment's file mapping. Conflicting
file mappings over those bytes are also rejected. Zero alignment normalizes to
one. The loader does not require a readable initialization image for TBSS.
The bounded boot profile still requires its template virtual address to lie within
the mapped image, including a zero-byte template. Boot separately checks
allocation arithmetic and uses aligned storage even when logical size is not an
alignment multiple.

`test_module_tls` exercises the actual registry entry, initialization, alignment,
zero-fill, stable writes, borrowed storage, thread separation, rejected contexts,
full-width requests and named stops. `test_module_tls_boot.py` executes only owned
AMD64 fixtures through `pbboot`: static/dynamic main TLS aliasing with logical size
17 and rounded size 64, module ID relocations, a relocated template pointer,
constructor persistence, and an unsupported high module ID. The loader tests
check malformed metadata before mapping. These are host implementation checks,
not differential execution of the firmware function or gameplay captures.

This prepares a prerequisite for admitting system modules such as the supplied
`libSceLibcInternal`. It does not yet load that library or resolve AppContent's
external service responses, and establishes no new original-game boot milestone.
