# Changelog

## 0.1.0-alpha.2

- Add immutable WatchSet ownership for 1..256 explicit directories with stable
  origin indices, all-or-inactive setup and whole-set terminal loss semantics.
- Preserve alpha.1 behavior. Sets alone need Linux 4.19/IN_MASK_CREATE support.

## 0.1.0-alpha.1

- Experimental Linux directory watcher with a noncopyable owner, 64 KiB retained
  event buffer, bounded poll waits, and explicit cleanup/loss/error semantics.
- Real filesystem and fault tests, native and Goose sanitizer tests, bounded
  decoder fuzzing, command rerun example and reproducible benchmark drivers.
- Qualified on Linux x86-64/glibc only; no stable API compatibility promise.
