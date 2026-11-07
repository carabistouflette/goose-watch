# Validation

Use `make setup` for the exact compiler revision. With an existing verified
compiler set `GOOSE=/absolute/path/to/goose` on every Make invocation.

```sh
make format check test test-debug test-examples CC=gcc
make test test-debug test-examples CC=clang
make test-native CC=gcc
make test-native CC=clang
make test-sanitize SANITIZER_CC=gcc
make test-sanitize SANITIZER_CC=clang
make fuzz FUZZ_RUNS=50000
make bench-smoke
```

The filesystem harness exchanges bounded commands with the driver: real create,
write, close-write, rename-cookie, delete, byte/255-byte names, directory flags,
nonrecursive behavior, moved/deleted watch, End, reopen and output clearing.
The large-burst check runs only if max_queued_events >=8192; otherwise the smaller
burst remains and the skip is explicit. Synthetic full-buffer/refill tests run
regardless. Tests use isolated local temporary directories; remote filesystems
and concurrent changes to inotify sysctls are outside this qualification.

Native fault tests inject errors deterministically, including initial open,
failed add plus failed cleanup, fd zero, poll errors/interruption, read
EAGAIN/interruption/error/EOF, malformed events, overflow, poison and
close-EINTR. No test changes the host's sysctls. Overflow delivery is synthetic; the example recovery branch is source-reviewed,
a real kernel-overflow stress campaign is not claimed. Real-native tests check
repeat success/failure fd counts, independent owners, nonblocking/CLOEXEC flags,
exec inheritance and a delivered interrupt signal. Native GCC/Clang and complete
Goose consumers run under ASan/UBSan. Twelve compilation failures enforce resource ownership and event-name lifetimes
for both owner types. Fuzzing targets the decoder and path validator with bounded input
size and fixed seed; it is finite, not proof of absence of defects.

CI runs Linux GCC/Clang with full-commit pinned actions. Release acceptance
requires successful [hosted CI](https://github.com/carabistouflette/goose-watch/actions/workflows/ci.yml)
for the exact candidate commit (or an explicitly verified identical merge tree).
Local evidence alone does not establish hosted release acceptance. macOS, Windows, ARM64 and alternate libc are unqualified.

## WatchSet qualification

Set tests cover 1–256 explicit directories, input-order indices, cross-directory
rename cookies, non-UTF-8 and 255-byte names, rejected live reopen, reverse-order
reopen, terminal ignored/overflow fencing, and output clearing. Native fixtures
cover malformed packed offsets and terminators, alias/duplicate rejection,
rollback at all 256 positions, unsorted descriptor IDs, unknown origins,
legacy terminal compatibility and unsupported headers without acquiring an fd.
Real lifecycle tests check successful and failed set opens for leaks, descriptor
flags and exec inheritance. The mixed decoder/path/set fuzz campaign is finite.

`make bench-set` measures open, 1,000 idle calls and draining 16 generated files
per directory at counts 1, 16, 64 and 256. Generation is outside the drain timer.
Measurements require a queue limit of at least 16,384; shared-host samples do not
establish comparative performance. Source, compiler and binary hashes are
checked before and after measurement.

## Alpha.1 local qualification receipt

Runtime implementation was independently reviewed at `e6272a7`; final test and
benchmark-tool fixes are in `8d3cda6`. The runtime sources are unchanged between
those commits. Three independent reviewers found no blocking runtime issue.
Repeated design, implementation and test reviews preceded the final candidate.

On Linux x86-64/glibc with compiler commit
`eadca409f68210893f366e60d0c2f0dc29a339be`, local qualification passed:

- Goose JIT, optimized GCC/Clang, and debug/bounds-check builds.
- Six compiler ownership/lifetime rejection cases.
- Filesystem, lifecycle, byte-name, buffering, timeout and command example tests.
- Native GCC/Clang ASan/UBSan and complete Goose consumer ASan/UBSan runs.
- Fixed-seed decoder/path libFuzzer campaign: 50,000 inputs, ASan/UBSan enabled.
- Fresh standalone local clone: check, JIT/optimized/debug tests and command example pass.
- Clean-source GCC/Clang benchmark runs, five rounds each, all samples retained
  under `bench/results/`. Binary/source stability is checked during measurement.

This receipt covers local checks only. It does not establish hosted CI,
a public release, a security audit, or qualification on other platforms.
