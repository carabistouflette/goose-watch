# goose-watch

Bounded Linux directory notifications for the [Goose language](https://github.com/aardappel/goose).
Experimental alpha API, Apache-2.0; qualified on Linux x86-64/glibc only.

Watch one directory with a compiler-enforced noncopyable owner. `next` returns
one event, retaining the rest of each 64 KiB kernel read. Buffered events need
no syscall. Names are owned bytes, waits are bounded, and event loss is explicit.

## Try it

Requires Git, Make, CMake 3.27+, C/C++20 compilers,
Python 3.11+, Linux headers and glibc:

```sh
git clone https://github.com/carabistouflette/goose-watch.git
cd goose-watch
git checkout v0.1.0-alpha.1
make setup
.toolchain/goose/build/goose run examples/events.goose -- /absolute/directory
make check test test-debug
```

Setup builds the full compiler commit in `GOOSE_REVISION`. With a compatible
existing compiler, pass `GOOSE=/absolute/path/to/goose` to Make. Validation also
uses GCC, Clang and sanitizers; see [validation](docs/VALIDATION.md).

For an application, declare the pinned package in its root source file:

```goose
import watch from "carabistouflette/goose-watch@v0.1.0-alpha.1";
import std;
fn main() {
    var owner = watch::Watcher { .. };
    let opened = watch::open(owner, "/tmp");
    if opened.error != watch::Error.None { return; }
    var event = watch::Event { .. };
    let status = watch::next(owner, 1000, event);
    if status.error == watch::Error.None && status.ready {
        // Do not display raw event.name: filenames are untrusted byte strings.
        print("mask=", event.mask, " cookie=", event.cookie);
        if watch::has(event, watch::OVERFLOW) {
            print("events lost: close/reopen and rescan");
        }
    }
    let closed = watch::close(owner);
    if closed.error != watch::Error.None { print("close error: ", closed.os_error); }
}
```

For a neighboring local checkout use `import watch from "./goose-watch";` instead.

`Status` separates `error`, `os_error`, `cleanup_error` and `ready`. A successful
idle/timeout has `ready = false`; output is cleared on idle/error. `has` tests
whether any supplied mask bit is set. Rename halves have separate events and
an unsigned cookie, not a guaranteed pair. See the [complete API](docs/API.md).

## Command rerun example

```sh
.toolchain/goose/build/goose run examples/rerun.goose -- /absolute/src 10 -- /absolute/program arg
```

Runs the command initially, then on relevant `.goose` file events until ten
runs complete. The directory is nonrecursive. No debounce or event-per-operation
count is promised; with no further changes the example keeps waiting. Child
argv is passed directly, output is inherited, and each direct child has a
30-second deadline. A child failure is reported and later changes can retry;
the final command result determines success. Descendant supervision is outside
the pinned process library's contract. SIGINT terminates the example; this is
not an interactive job controller.

The example alone imports `goose-process` at full published commit
`4cf1f13302ad5b42f3ca94f3c7a829c44b7ee60e`; first compilation needs network access.
The watcher core and basic example have no external package dependency. On
overflow the example closes/reopens before a rerun; it stops if the watched
directory moves, disappears or unmounts. Honk integration remains future work.

## Limits

- One directory per owner, 64 KiB retained buffer, names <=255 bytes, paths
  <=4095 bytes and requested poll waits of 0..60000 ms. Kernel limits also apply.
- Absolute normalized paths only. Final symlinks are rejected; intermediate
  symlinks may be followed. Path lookup has no hard deadline or confinement.
- Close explicitly, including after End or CorruptEvent. No destructor or
  hidden threads. One operation per owner at a time; independent owners do not
  share library state. Root owners cannot be copied or placed in ordinary arrays.
- Notifications are hints. Kernel coalescing, overflow, rename races, remote
  filesystems and mmap writes prevent a complete or exactly-once change history.
  After overflow, close/reopen and rescan. Events never authorize file access.
- No recursive traversal, ignore rules, automatic retries, rename pairing or
  background command execution in the core. Other platforms are unqualified.

[Benchmarks](bench/README.md) preserve raw measurements and provenance; no
comparative speed claim is made. [Contributing](CONTRIBUTING.md),
[security](SECURITY.md), [license](LICENSE) and [notices](NOTICE).
