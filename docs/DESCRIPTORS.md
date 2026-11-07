# Borrowed directory descriptor sets

The additive `open_set_fds(owner: WatchSet&, directories: const i64[:]) -> OpenSet`
interface watches 1..256 already-open directory descriptors. It returns the same
caller-order indices, event/status types and immutable membership as
[the path-based set](SETS.md). Existing Watcher and path-based APIs are unchanged.

This interface is intended for callers that already own directory descriptors,
such as a descriptor-relative no-follow tree scanner. Acquisition policy belongs
to the caller: a descriptor may itself have been opened through a symlink.
Registration binds to the opened directory identity rather than re-resolving its
original pathname. This does not establish filesystem confinement, a snapshot,
or recursive watching.

```goose
import watch from "./goose-watch";
import std;
fn monitor(opened_directories: const i64[:]) {
    var owner = watch::WatchSet { .. };
    let opened = watch::open_set_fds(owner, opened_directories);
    if opened.status.error != watch::Error.None {
        print("registration failed at index ", opened.failed_directory); return;
    }
    var event = watch::SetEvent { .. };
    let status = watch::next(owner, 1000, event);
    if status.ready { print("directory index=", event.directory); }
    let closed = watch::close(owner);
    if closed.error != watch::Error.None { print("close errno=", closed.os_error); }
}
```

Caller descriptors and their numbers must remain open and unchanged throughout
the call, including against concurrent close/dup2 and signal-handler operations.
The library neither transfers, duplicates nor closes caller descriptors, and
leaves their flags unchanged. After successful registration, callers may close
them: the owned inotify instance retains its own watches. Closing a WatchSet
never closes the supplied descriptors. A live owner rejects replacement before
validating input, preserving its mapping and buffered records.

Input numbers must be 0..INT_MAX. Empty/over-limit inputs, out-of-range values
and repeated numeric descriptors return InvalidInput before instance acquisition;
a numeric duplicate reports its later index. Every descriptor is checked with
fstat and must identify a directory. Invalid/closed descriptors report their
system error; a non-directory reports ENOTDIR. Descriptor aliases for the same
inode are atomically rejected with IN_MASK_CREATE/EEXIST. Installation failure
rolls back the complete new instance once, preserving primary and cleanup errors.
Global errors use failed_directory=-1; per-descriptor errors identify the input.

Registration requires genuine, accessible `/proc/self/fd` in the process mount
namespace. The library formats only private numeric aliases and deliberately
follows them; it never accepts caller-supplied procfd path text through this
interface. IN_ONLYDIR and IN_MASK_CREATE remain required. Missing/inaccessible
procfs fails explicitly; no path fallback is attempted. Privileged replacement
of procfs is outside this contract. The existing path interface retains its
IN_DONT_FOLLOW flag. Sets retain the Linux 4.19/header requirements; builds
without IN_MASK_CREATE return ENOTSUP without descriptor acquisition or fstat.

External open directory descriptors can delay DELETE_SELF/IGNORED delivery after
rmdir. Retaining a directory fd is not a directory-liveness test. A recursive
consumer must also observe parent namespace/binding changes and validate links
and identities when coverage is uncertain. The native deletion-anchor test
records the observed behavior; it does not assert a particular kernel sequence.

Setup uses bounded native stack aliases (256 times32 bytes), a256-pointer table,
and shared installation scratch. Retained WatchSet storage and per-event work
are unchanged. There is no new native heap ownership or per-event path lookup.
Open and cleanup have no hard wall-clock deadline. General signal/fork and
single-operation ownership rules match the existing contract.
