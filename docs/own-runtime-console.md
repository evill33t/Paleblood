# Owned console output

`pbboot --owned-console` prepares an output endpoint backed by an owned duplicate
of the process's standard output. `_open("/dev/console", 1, ...)` creates another
owned backing descriptor and returns the lowest available guest descriptor from
3 upward. Guest IDs and host descriptors occupy separate namespaces. Only
registered descriptors can be written or closed; guest IDs 0, 1 and 2 do not
expose the host's standard streams.

This is an explicit output-stream port policy. It does not establish a native
terminal device, its controls or its firmware-dependent state. The initial
firmware stream configuration derives to a write-only console open, but the
runtime argument registers have not been sampled. Unknown paths and access flags
remain named diagnostics. `open` and `sceKernelOpen` are distinct native bodies
and are not registered as aliases of `_open`.

## Native wire and owned state

Independent analysis binds the exact libkernel `_open` entry and its common
error handler. The failure handler stores four bytes through `__error`, then
sets both RAX and RDX to all ones. The `_write` and `_close` stubs have the same
complete instruction template with different syscall selectors. The actual
kernel results and console device behavior remain external to these bodies.
Native `__error` can select a global fallback as well as thread-dependent
storage; it is not proven to be a thread-local-only getter.

The port supplies a separate, writable 32-bit errno cell for each host thread.
Its initial zero, lifetime and thread selection are owned policy. It is separate
from the host C library's errno. Successful operations preserve the guest cell;
failed supported operations publish its four-byte BSD compatibility value.
Two-word provider results preserve the observed failure register pair. The
secondary word on success is explicitly zero in this port; native success does
not establish that value.

The compatibility values come from FreeBSD's published
[errno definitions](https://github.com/freebsd/freebsd-src/blob/main/sys/sys/errno.h).
This defines the chosen port error schema; it is not a claim that console kernel
services were observed returning those values. Unknown host errors remain
explicit failures. No general host-errno passthrough or catch-all error exists.

## Descriptor and recovery boundaries

An owned duplicate shares the source's open-file description, offset, status
flags and underlying access mode. The registry enforces guest write-only rights;
`dup` does not narrow an O_RDWR source. The backend does not change source status
flags. Preparation and registry mutation belong to the preparing host thread;
other threads may use their own errno cell, but guest I/O on them is unsupported.
Worker-thread I/O needs a separate lifetime and synchronization extension.

Writes make one host call and preserve its actual partial count or error. They
do not retry or loop until all bytes are written. Counts above the host
`SSIZE_MAX` fail with the owned EINVAL value before inspecting the buffer. See the
[POSIX write contract](https://pubs.opengroup.org/onlinepubs/9690949599/functions/write.html).
A pipe endpoint retains its inherited SIGPIPE behavior; the backend installs no
global signal handler and does not claim native console signal semantics.

Short publication and close transitions preserve the host signal mask and block
SIGALRM while bookkeeping is incomplete. Potentially blocking writes run with
the normal mask and no registry lock. A guest timeout can unwind a blocked write
without stranding a lock or hiding its backing descriptor. After the guest scope
has settled, the quiescent owner closes all remaining owned backing and the
anchor. It never closes the borrowed output descriptor. Preparation has one
process lifetime, and shutdown is idempotent; there is no live reset or rebind.

A host close error can leave descriptor state uncertain under the
[POSIX close contract](https://pubs.opengroup.org/onlinepubs/009604499/functions/close.html).
The backend must retire that descriptor and stop rather than blindly retry or
close a possibly reused number. Unsupported device controls and general file
namespaces remain separate work.

Owned C and AMD64 fixtures check actual bytes, descriptor isolation and cleanup,
errno and both error result registers, initializer/main sharing, and the opt-in
boundary. They do not replace genuine gameplay captures or establish a rendered
frame.
