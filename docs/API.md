# API and contract

Experimental Linux directory watcher. Initial consumer: a standalone command
rerunner example, composing watching and a separately pinned goose-process.
Honk separately consumes the pinned alpha.1 single-directory interface;
[WatchSet](SETS.md) is the additive alpha.2 interface with a stricter terminal policy.

- `root struct Watcher`: compiler-enforced noncopyable owner, fixed 64 KiB pending
  kernel event buffer and private integer state; zero initialization is inactive.
  No global registry, threads or native heap allocation. Event names use Goose
  growable storage; all allocations are caller-owned. No hidden environment I/O.
- `open(watcher&, absolute_path) -> Status`: one nonrecursive directory watch,
  nonblocking close-on-exec inotify descriptor. Reject already-open owners,
  empty/relative/overlong/NUL paths and final symlinks. Paths are at most 4095
  bytes: reject trailing separators, empty components and dot/dot-dot components
  (root `/` is allowed). Intermediate symlinks may
  be followed: this is notification, not directory confinement or authorization.
- `next(watcher&, timeout_ms, event&) -> Status`: one event. Timeout 0..60000;
  poll once then read at most 64 KiB once, retaining unused complete records.
  Buffered events need no syscall. `Status.ready` distinguishes events from
  successful idle/timeout. A read EAGAIN after poll is idle; poll error/hangup
  is SystemError; zero-byte read is CorruptEvent. EINTR is explicit; no unbounded retry loops.
- `Event`: raw Linux event mask, unsigned rename cookie widened to i64, owned
  byte name of at most 255 bytes (no UTF-8 assumption). Output is cleared on idle/error. Events never
  contain absolute paths. Empty names concern the watched directory itself.
- `close(watcher&) -> Status`: explicit, idempotent for inactive owners; invalidate
  ownership before one close syscall, never retry close on Linux EINTR.
- Error classes: None, InvalidInput, InvalidState, Interrupted, SystemError,
  End (after the final Ignored event), CorruptEvent (fatal decoder failure).
  OS errno is separate. Status includes `error`, `os_error`, `cleanup_error`, and `ready`. Failed
  open on an inactive owner preserves inactivity; opening a live owner rejects
  the request and preserves it. Primary and cleanup errors remain separate; close invalidates even on error.

Watch all create/modify/close-write/attribute/delete/move/self/unmount/ignored
notifications; no access/open/close-read events to cause feedback from builds.
`OVERFLOW` and `IGNORED` are explicit masks. Overflow means events were lost:
close/reopen and rescan; no claim of a complete history. Rename halves are independent
and may span reads or be missing. No automatic pairing, recursion, debounce,
ignore rules, traversal, subprocesses, file opening or background work in core.

After delivering Ignored, return End on subsequent calls once the current userspace buffer is consumed; no further kernel reads occur. Overflow may hide Ignored, so observable End is not guaranteed.
End and CorruptEvent retain fd ownership: explicit close is still required.
CorruptEvent permanently poisons event consumption until close/reopen. A moved watched directory remains watched by inode; paths can become stale.
Do not use notifications as authorization to open files. Names are untrusted
bytes: escape before terminal/log rendering. Kernel coalescing, queue overflow,
remote/mmap changes and races preclude exactly-once or snapshot semantics.

Only one operation may access a given watcher at a time. Independent watchers
have independent storage. No use from signal handlers, and no child use after
fork before exec. Explicit close is required on every owning path; no destructor.
Native functions are private; mutating private state or raw-closing owned fds
is outside the contract.

Validation gates: JIT, optimized GCC/Clang and debug Goose consumers; ownership
compile failures; synthetic malformed/truncated/unaligned native events;
real filesystem create/write/rename/delete/self-delete; final symlink and bad
paths; nonblocking/timeout/EINTR; retained-buffer order; close-on-exec and fd
leaks; native ASan/UBSan and bounded decoder fuzzing; reproducible idle/burst
benchmarks with provenance and no unsupported comparison or speed claims.

`timeout_ms` bounds the requested poll wait, not total scheduling time or
filesystem pathname resolution. Root owners are locals/globals, not elements
of ordinary arrays. Name outputs reuse their existing capacity.

`has(event, mask)` tests whether any supplied bit is present (zero matches none).
The mask constants are the Linux values; unknown future bits are retained.
Native entry points and fields prefixed with `_` or `detail_` are private API.

Kernel behavior reference: [inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html)
and [inotify_add_watch(2)](https://man7.org/linux/man-pages/man2/inotify_add_watch.2.html).
