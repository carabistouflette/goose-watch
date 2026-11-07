# Benchmarks

`make bench GOOSE=/absolute/path/to/goose CC=gcc` builds at `-O2` and writes
`.build/benchmark.json` with five rounds and every raw sample. Use `make
bench-smoke` for one round of both single-owner and set workloads. Requires
Linux, Python 3.11+, GCC or Clang, and an inotify queue capacity of at least 16384 events (no competing filesystem writer
in the temporary watched directory). No speed assertion is part of CI.

`make bench` alone requires a queue capacity of at least 4096 events.

The single-owner Goose driver measures 10,000 empty nonblocking `next` calls, then drains
notifications for 1, 64, and 1024 freshly written files. File creation is outside
the drain timer. Drain includes syscall refill, native decoding, Goose ownership
copying and correctness checks; event counts are measured, not assumed to equal
filesystem operation counts. Timeout scheduling latency is not measured.
The C-only decoder loop uses one million synthetic records; it excludes kernel
I/O and Goose name copying, and uses a separate translation unit without LTO to prevent caller folding.
It is a diagnostic microbenchmark, not an end-to-end speed claim.

The script snapshots source/Git/compiler state before building and rejects changes
during measurement. It also checksums both measured binaries before and after.
The report records source hashes, Git identity/dirty state, Goose revision and
binary hash, compiler version, flags, platform, affinity and queue limit. Retain
all samples, including slower ones. A dirty run is exploratory; publication needs
an exact reviewed source identity. No competitor, sustained load, cross-platform
ranking or latency-percentile claim follows from these local samples.

Deterministic fault tests separately assert exactly one poll/read per refill
and no extra syscalls for retained events. Profile a real consumer before
changing the hot path.

`make bench-set CC=gcc` writes `.build/set-benchmark.json` with five rounds at
1, 16, 64 and 256 directories, each receiving 16 newly written files.
`make bench-set-smoke` retains one round. These targets require queue capacity
16384. Each sample measures full open setup, 1,000 idle calls and draining the
queued burst. Generation is outside the drain timer; open includes packing,
validation, watch registration and map sorting. All returned origin indices are
checked, and global overflow rejects the sample. Set reports retain the same
before/after source, compiler and binary identity checks as single-owner reports.
