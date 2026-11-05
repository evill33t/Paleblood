# Owned system-module startup

`pbboot` can map an explicitly configured group of AMD64 `ET_SCE_DYNAMIC`
modules and start their actual entry functions on the current guest stack:

```sh
pbboot executable.elf \
  --system-module /system/common/lib/libExample.sprx=example.elf \
  --start-system-module /system/common/lib/libExampleDependency.sprx=dependency.elf
```

The left side is the exact guest path passed to the private kernel loader.
The right side is an operator-supplied local ELF. There is no implicit filesystem
search, name substitution or alias between incompatible library namespaces.
The ordinary `--module` option retains its legacy eager startup behavior.

## Supported startup profile

All images, relocations, TLS blocks and the sealed catalogue are prepared before
entering guest code. Deferred modules remain `prepared`. An explicit eager
module starts before legacy modules and the main entry. Import bindings determine
the managed dependency graph. Acyclic dependencies start once, before their
consumer. A dependency already started is reused; a direct repeated load, a
reentrant load or a failed initializer stops by name. These are deliberately
bounded host policies, not a complete native load/unload/refcount implementation.
Managed imports from legacy eager modules are refused because their initialization
order would not be established by this graph.

The private `sceKernelLoadStartModuleForSysmodule` entry accepts zero argument
size, a null argument pointer and zero caller flags. It calls the real initializer
with `(0, NULL, module_start_or_NULL)`. A zero ELF entry address is valid when it
points into executable file-backed bytes. An actual `module_start` export is
passed as the third argument; the module entry decides how to use it. This entry
is not registered as the public `sceKernelLoadStartModule` API.

A non-null options pointer returns the observed early invalid-argument status
without loading or writing the result. On initializer return, its actual signed
32-bit result is written through the optional result pointer, exactly four bytes,
before testing for failure. A nonnegative result publishes a positive owned
handle. A negative result stops with a named diagnostic after that write; native
SDK-dependent rollback, unloading and compatibility returns remain unsupported.
Handles belong to this catalogue and are not native kernel handle identities.

Initialization stays inside the existing outer guest timeout and fault recovery
scope. It never resets the guest stack or overwrites recovery state by calling
`run_guest` recursively. A fault, timeout or unimplemented import abandons every
active initializer and marks its state `failed`. Previously completed dependencies
remain started. No rollback or recovery of partially mutated guest memory is
claimed. A successful imported call to a managed provider requires `started`;
the readiness gate preserves GP, vector, stack and variadic arguments. Direct
calls within the initializer's own image remain ordinary guest code.

## Typed and backed objects

The explicit profile binds imports by NID, library and exact `STT_FUNC` or
`STT_OBJECT` kind. Multiple matching providers, unsupported selected symbol kinds,
function extents outside executable file-backed bytes, object extents outside allocated segments,
zero or insufficient declared extents, negative or out-of-object data addends,
nonzero function-address addends (which cannot offset a trampoline),
and missing data backing fail admission before guest execution. Unused exports
of other kinds do not participate in binding. Raw data from a deferred managed
provider cannot be imported directly by a legacy module or executable; configure
an eager provider to establish its state first. Managed consumers use the
initialized dependency's actual object address, including shared mutations.

`RT_SYSLIB_DATA` is a separate registry for stable owned objects with explicit
sizes and a documented value/lifetime policy. It does not expose host libc
variables. No kernel data providers are invented by this change. The supplied
firmware's process variables, stack guard and signal mask need their own justified
initialization policies before those modules can execute. An export's zero-filled
file tail proves storage layout, not runtime values.

Legacy runs without either new system-module option retain their old unresolved
data-page fallback; that behavior is not evidence for a valid native object.
The explicit profile allocates no such pages. Unknown function calls continue to
stop by name. Status JSON distinguishes `module not ready` from `unimplemented`
and reports the managed lifecycle state.

## Evidence and validation boundary

The initializer ABI, early options rejection and four-byte result write come
from independent REA analysis of the operator-supplied kernel's private wrapper
and common load/start helper. Metadata from supplied modules establishes actual
symbol kinds, dependency namespaces and zero entry addresses. No implementation
from another PS4 runtime or emulator was consulted or adapted.

The owned C tests cover catalogue validation, lifetime, thread ownership,
dependency reuse, exact result writes, early failure and abandoned initialization.
Owned AMD64 ELF fixtures exercise real entry calls, deferred and eager startup,
third-argument identity, dependency data aliasing, TLS, readiness argument
preservation and outer fault/timeout/import recovery. They are host-profile tests,
not console recordings, native differential verification or evidence of gameplay.
This change does not establish firmware service output, native linker/DTV behavior,
child guest-thread loading, dynamic discovery, unloading or an ingame milestone.
