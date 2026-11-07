# Changelog

## 0.1.0-alpha.1

- Experimental Linux directory watcher with a noncopyable owner, 64 KiB retained
  event buffer, bounded poll waits, and explicit cleanup/loss/error semantics.
- Real filesystem and fault tests, native and Goose sanitizer tests, bounded
  decoder fuzzing, command rerun example and reproducible benchmark drivers.
- Qualified on Linux x86-64/glibc only; no stable API compatibility promise.
