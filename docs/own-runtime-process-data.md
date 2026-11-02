# Owned kernel process data

`pbboot --owned-process-data` opts into a process-lifetime owned profile before
any guest initializer or entry runs. It supplies four typed `libkernel` objects:
`environ` (8-byte pointer cell), `__progname` (8-byte pointer cell),
`__stack_chk_guard` (64 writable bytes), and `_sigintr` (four 32-bit words).
These are exported object addresses, not values copied into each relocation.

The raw entry block holds a 64-bit argc followed by argv pointers, a NULL,
environment pointers and another NULL. The default boot profile copies the
single argument `eboot.bin` and an empty environment. It never inherits host
`environ`. The internal preparation API accepts explicit terminated vectors,
at most 64 arguments, 128 environment entries, and 1 MiB of terminated strings
in total. Callers must keep input vectors/strings stable for the call.

All copied vectors and strings are writable. `environ` points into that same
entry block after the argv terminator. With an argument, `__progname` borrows
the suffix of copied argv[0] after its final forward slash. A trailing slash
therefore yields an empty name; backslash is ordinary data. With no arguments,
the owned profile publishes a NULL program-name cell. Preparation does not
invoke the console's kernel startup routine.

The supplied kernel exposes a 64-byte guard and initializes it through a
system service, checking the first eight bytes as its readiness sentinel.
This port instead obtains all 64 bytes in one host `getentropy` call. Entropy
failure or a zero first word rejects preparation; there is no fixed fallback.
The guard remains stable for the process lifetime. It is private guest storage
and does not alias a host compiler's stack protector object. GCC describes the
runtime's role in providing its guard in its [stack protector documentation](https://gcc.gnu.org/onlinedocs/gccint/Stack-Smashing-Protection.html).

The supplied signal membership helper establishes a four-word, 128-bit layout.
An initially empty `_sigintr` is an explicit owned policy, not a measured
console default. It is independent of the host signal mask; this change does
not implement signal actions or install host handlers for guest preferences.
The representation is also consistent with the original
[FreeBSD 9 signal-set operations](https://raw.githubusercontent.com/freebsd/freebsd-src/releng/9.0/lib/libc/gen/sigsetops.c).

Preparation stages allocations and entropy before atomically publishing the
group. Failure leaves it unavailable. Concurrent preparation reports busy;
a second successful preparation is forbidden. No reset or teardown is offered:
guest constructors and frames can retain the addresses. This owns process data,
not another guest process, and cannot be used to recycle a boot in place.

The typed registry has an optional readiness callback. Unprepared objects are
never bound or counted as implemented. `--imports` performs no preparation or
entropy acquisition and marks them `runtime-data-pending`, even with the flag.
The explicit system-module profile rejects such pending data before execution
and names the opt-in. An unflagged legacy run retains its existing anonymous
data pages and unresolved coverage; registration alone does not promote them.
Declared extents, addends, NIDs and library identity remain checked.

Owned C and AMD64 guest fixtures check failure before publication, concurrent
preparation, writable copies, aliasing, all 64 guard bytes and lifetime,
constructor/main sharing, readiness listings and coverage, and backing bounds.
They do not establish console initialization defaults, original-game execution,
AppContent service results or rendered gameplay.
