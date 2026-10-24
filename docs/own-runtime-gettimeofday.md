# Bounded realtime service

The runtime registers `gettimeofday` in `libScePosix` and `libkernel`. The supported
call supplies a valid writable sixteen-byte output and NULL timezone. Success writes
two signed 64-bit fields, seconds followed by microseconds, and returns zero.
The output may be unaligned. Native host realtime is an explicit port choice; clock
adjustment, resolution and precision are not console-clock or monotonicity claims.

The host query writes a local `struct timeval`. Explicit field conversion avoids
copying native padding or Darwin's narrower microsecond field. Only validated
success publishes the sixteen output bytes. The host thread's errno is preserved.
NULL output, non-NULL timezone, a failed host query or malformed microseconds stop
by name before output publication. No guest errno or invalid-pointer behavior is
invented. Inaccessible or overlapping storage is outside the writable-output domain.

## Basis and limits

The reached game's import passes NULL timezone and consumes two eight-byte fields
as seconds times one million plus microseconds. Canonical import/export binding
also establishes the two library aliases. The supplied libkernel wrapper dispatches
syscall 116; it does not contain kernel output stores. Kernel behavior, its error
helper and the supplied module's firmware match remain unknown. This service uses
the public API and the consumed wire without copying any console object layout or
another runtime implementation. No target code or raw decompiler output is included.

The historical [POSIX gettimeofday specification](https://pubs.opengroup.org/onlinepubs/009604599/functions/gettimeofday.html)
describes epoch seconds and microseconds for NULL timezone. Apple's public
[gettimeofday manual](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/gettimeofday.2.html)
documents the host service; the installed SDK provides its native field types.
These API descriptions do not establish console-specific error translation.

## Owned checks

`timeval` calls both actual registry aliases with an unaligned output and canaries.
`timeval_failures` substitutes a native provider in its test translation unit to
check full-width positive and negative seconds, exact microsecond endpoints,
poisoned host padding, errno preservation, and failure before output publication.
Checks remain active in Release. They do not assert realtime ordering, execute the
console kernel or verify a decompiled game function.

Normal Linux Release builds passed the selected syslib and two timeval CTests on
GCC and Clang, both for this standalone unit and the full runtime-progress source
composition. Zero skips; all source hashes stayed unchanged. Darwin build and
startup validation require a separate composition retaining its registry and
startup prerequisites.
