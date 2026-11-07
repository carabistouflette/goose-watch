# Explicit directory sets

Alpha.2 adds a bounded, immutable set of 1..256 directories. Linux 4.19+ and
headers defining IN_MASK_CREATE are required for sets; single Watcher behavior
and its header requirements remain unchanged. Qualified platform: Linux
x86-64/glibc only. Membership is nonrecursive and stays fixed until close.

```goose
import watch from "./goose-watch";
import std;
fn main() {
    var owner = watch::WatchSet { .. };
    let opened = watch::open_set(owner, ["/absolute/src", "/absolute/tests"]);
    if opened.status.error != watch::Error.None {
        print("open failed at request index ", opened.failed_directory); return;
    }
    var out = watch::SetEvent { .. };
    let status = watch::next(owner, 1000, out);
    if status.error == watch::Error.None && status.ready {
        if watch::has(out.event, watch::OVERFLOW) {
            print("coverage lost: close/reopen the complete set and rescan");
        } else {
            print("directory index=", out.directory, " mask=", out.event.mask);
            // out.event.name is untrusted bytes. Escape before displaying.
        }
    }
    let closed = watch::close(owner);
    if closed.error != watch::Error.None { print("close errno=", closed.os_error); }
}
```

`WatchSet` is a root owner: declare it locally/globally, pass by reference, never
copy or aggregate it. `SetEvent` composes the existing Event in `.event` and adds
`.directory`. `open_set` returns `OpenSet { status, failed_directory }`.
The existing `next`/`close` names overload on the owner type. Existing Watcher,
Event, Status and `has(Event, mask)` semantics are unchanged.

Input paths must use the same normalized absolute byte-path rules as Watcher:
1..4095 bytes, no NUL, empty/dot/dot-dot components or trailing separators except
root `/`. Reject duplicate strings before acquiring resources and inode aliases
with IN_MASK_CREATE. Intermediate symlinks may still be followed; this is not
confinement or authorization. No path lookup or cleanup wall-clock deadline is
promised. Poll timeout 0..60000 bounds the requested poll wait only.

Open validates count and complete packed inputs before creating a NONBLOCK,
CLOEXEC instance. On any registration failure, close the whole new instance once
and remain inactive. Preserve primary errno and cleanup_error separately.
Invalid paths/duplicate text report InvalidInput; duplicate text reports the later
index. Kernel alias rejection reports SystemError/EEXIST. Global argument/init
errors and success use failed_directory=-1; per-path/kernel errors identify the
request involved. Validation stages can report a later overlong path before an
earlier lexical/alias error. A live owner rejects every replacement request as
InvalidState without changing ownership, mapping or queued data.

Paths are borrowed only during open. The temporary packed bytes are at most
1 MiB, with at most 4096 offset-metadata bytes. The retained owner has one 64 KiB
read buffer, fixed 4 KiB descriptor/index map and small state. Names retain the
255-byte bound and use caller-owned Goose storage. No global registry, threads,
native heap ownership, per-event stat/path lookup or per-directory fd is used.

Ready ordinary events use stable zero-based indices in caller request order,
independent of the kernel's descriptor values. Callers retain their own path table.
Names are basenames; an empty name concerns that watched directory. Global
OVERFLOW has index -1, empty name and zero cookie. Handle it before indexing the
path table. Idle/error clears the index to -1 and every nested Event field.
Old returned event indices refer to their original session even after reopening
with a different path order. Returned names survive owner close.

Deliver IGNORED with its original index, then immediately return stable End on
subsequent next calls. Deliver global OVERFLOW, then the same stable End. Discard
remaining buffered records and perform no further reads after either terminal
event. Keep ownership until explicit close. Overflow requires complete close/
reopen plus application rescan/full command; it can hide individual removals.
MOVE_SELF/DELETE_SELF/UNMOUNT are indexed events; consumers such as Honk stop on
them instead of assuming pathname identity. Corruption, unknown descriptors and
inconsistent global-overflow records poison further reads as CorruptEvent.

Rename halves and cookies remain independent hints, including cross-directory
moves. No snapshot, exactly-once count, implicit recursion or subset continuation
is promised. Fresh fd per open and immutable membership avoid reusing identities
inside a session. One operation at a time per owner; signal/fork rules match
[the original contract](API.md).

Compatibility includes legacy buffered delivery after IGNORED and legacy
continuation after OVERFLOW. Those existing single-Watcher behaviors are not
changed to the stricter set policy. Header builds without the set flag return
SystemError/ENOTSUP for open_set; older kernels rejecting the flag propagate their
EINVAL and rollback. No silent flag downgrade is performed.

The kernel floor follows the actual
[Linux 4.19 header](https://github.com/torvalds/linux/blob/v4.19/include/uapi/linux/inotify.h):
4.18's header lacks IN_MASK_CREATE, despite the manual's earlier version note.

An additive [borrowed descriptor interface](DESCRIPTORS.md) binds registration
to already-open directory identities. It has explicit caller-lifetime and procfs
requirements; path-based registration remains unchanged.
