# WatchSet local records

Both records measure clean commit `3a6cee3`, with five rounds for each of
1, 16, 64 and 256 directories and 16 files per directory. Every raw sample is
retained. Each workload checks 16 CREATE events for each original directory
index, rejects overflow, and checks input/binary stability across measurement.

These shared-host Linux x86-64/glibc records establish workload execution and
provenance. They make no competitor ranking, sustained-load or latency-percentile
claim. See [methodology](../../README.md).
