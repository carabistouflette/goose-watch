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
close-EINTR. No test changes the host's sysctls. Overflow recovery is synthetic;
a real kernel-overflow stress campaign is not claimed. Real-native tests check
repeat success/failure fd counts, independent owners, nonblocking/CLOEXEC flags,
exec inheritance and a delivered interrupt signal. Native GCC/Clang and complete
Goose consumers run under ASan/UBSan. Five compilation failures enforce resource
ownership. Fuzzing targets the decoder and path validator with bounded input
size and fixed seed; it is finite, not proof of absence of defects.

CI is configured for Linux GCC/Clang and full-commit pinned actions. Hosted CI
has not run for this unpublished checkout. Local evidence is not hosted release
acceptance. macOS, Windows, ARM64 and alternate libc are unqualified.
