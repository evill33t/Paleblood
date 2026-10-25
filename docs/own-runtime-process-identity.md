# Owned process identity

`pbboot --owned-process-data` publishes the real host process identity with the
existing owned argument, environment, program name, guard, and signal preference
storage. This explicit port profile maps one guest process to its host process.
The default runtime does not invent a guest PID. Calling `getpid` without this
profile stops by name and identifies the required option.

The registered function is `getpid` in the exact `libkernel` and `libScePosix`
namespaces. Both return the same positive 32-bit identity, zero extended in RAX.
This is the owned profile's return policy; the native wrapper alone establishes
neither the PID value nor its namespace. No success value in RDX is asserted.
`getppid`, `_getpid`, and `sceKernelGetProcessType` remain unimplemented.

Preparation captures the host PID privately, checks its positive signed 32-bit
range, and publishes it only after every member of the existing process-data
group succeeds. A preparation failure publishes nothing and permits retry.
The first successful publication is permanent. Queries before publication leave
the caller's output untouched. The identity belongs to the process and remains
the same when queried by another host thread. Identity queries preserve host
errno, and the registered provider does not modify guest errno.

A host child inheriting a prepared profile refuses readiness, parameter lookup,
PID lookup, the registered provider, and further preparation. The original
parent remains ready. This check does not revoke previously cached raw pointers.
Fork during preparation and guest fork/exec are unsupported. There is no reset
or inherited-state repair API.

The supplied native library's canonical wrapper has syscall selector 20 and the
same carry-controlled return/error pattern already recovered from its syscall
wrappers. Its success value is supplied by the kernel. The shared error path
stores the error through `__error` and returns an all-ones register pair. The
supplied libc caller consumes only EAX before calling `sceKernelGetProcessType`;
that use supplies no process type value or PID numbering policy.

The [FreeBSD `getpid(2)` contract](https://man.freebsd.org/cgi/man.cgi?format=html&query=getpid&sektion=2)
describes calling-process identity and no error return. It supports the API
shape, while the explicit owned profile defines the host mapping used here.

`test_process_identity` checks publication across threads, unchanged outputs on
failure, retry boundaries, both registry entries, host errno, and inherited
profile rejection. A child prepared from an unready parent reports its result
through a real pipe; the parent's `fork` return independently identifies it.
Release assertions remain active. The owned AMD64 boot fixtures call both
namespaces, preserve all four guest errno bytes, and emit the complete 64-bit
result through the owned console. The launching Python parent's `Popen.pid` is
the oracle for both the executable and an eager module. No shipped guest code
or gameplay recordings are used by these tests.
