# Unresolved weak symbols

The loader exposes raw symbol binding, type, st_other and section metadata.
References with different metadata retain separate identities, including weak
and required declarations of the same name. Repeated identical declarations
aggregate size; each relocation retains its own addend. Metadata pointers live
until the ELF is closed. The export catalogue includes plain global/weak names
and special sections so callers can distinguish absence from unsupported presence.

The [ELF symbol specification](https://gabi.xinuos.com/elf/05-symtab.html)
allows an unresolved weak symbol to have zero value. Our bounded profile admits
zero only for undefined STB_WEAK references with st_other exactly zero and raw
type NOTYPE, OBJECT or FUNC. TLS, IFUNC, other symbol types and nondefault weak
visibility need separate provider policies. A map callback returning zero still
fails for a required reference. This changes the callback's interpretation for
eligible weak declarations; it does not provide a general dynamic linker.

Boot proves absence within its admitted image and runtime-provider catalogue.
Plain names use whole-string matching across every admitted image, including
main. Only known INTERNAL/HIDDEN definitions are excluded; unknown metadata
remains a candidate. Present plain providers are currently unsupported and stop
by name. Encoded typed imports retain exact NID/library lookup, backing bounds
and readiness. Wrong-kind, ambiguous, special-section or unprepared candidates
cannot become absent. Existing conservative ambiguity handling remains in place;
full interposition and general plain-provider binding are unfinished.

An absent import receives S=0 and no trampoline or anonymous object. For
R_X86_64_64 the stored value is S+A modulo 64 bits; the addend remains effective
when S=0. GLOB_DAT and JUMP_SLOT write S. See the
[ELF relocation model](https://gabi.xinuos.com/elf/06-reloc.html).
Boot function trampolines cannot apply a bound function-address addend. Both boot
profiles reject these addends; absent weak references still preserve S=0+A.
An absent function is not callable: guest code must check it before use, and an
unconditional call retains its real fault through the existing recovery scope.

Listings label absence `weak-absent`. These imports remain unimplemented in boot
counts and generated coverage and never become module-function provenance.
Pending runtime data remains pending, including weak references; legacy weak
references cannot obtain an invented anonymous object for an unprepared provider.
Legacy required-data behavior is unchanged.

Owned loader and AMD64 fixtures cover metadata, separate weak/required bindings,
relocation arithmetic, real provider calls, visible/hidden/special definitions,
wrong-kind and unprepared providers, short plain names and coverage, and actual
faults on unconditional absent calls. This enables optional-reference admission;
it does not implement firmware services or establish rendered gameplay.
