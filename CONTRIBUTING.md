# Contributing

Use focused branches (`feat/...`, `fix/...`, `docs/...`) and Conventional Commits.
One commit describes one coherent change. Keep generated binaries, caches,
credentials and private coordination notes out of commits.

Use the full compiler commit in GOOSE_REVISION. Run `make format check test
 test-debug`, native GCC/Clang ASan/UBSan tests and bounded decoder fuzzing for
native changes. Add positive, malformed, boundary and ownership regressions.
Independent review is required for behavioral changes; fix actionable findings
and keep exact-source CI passing. Compiler updates need independent review.

Keep single Watcher behavior compatible. WatchSet adds at most 256 explicit
directories under one owner; preserve all-or-inactive open and whole-set loss
semantics. No hidden I/O, threads,
recursion, retries or process management. Preserve explicit cleanup and loss
semantics. Profile before optimizing; retain raw samples and slower results,
source/compiler/platform provenance and comparison limitations. CI verifies
benchmark execution, never speed thresholds. Never move a published tag.
