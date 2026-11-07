# goose-watch

Bounded Linux directory notifications for Goose. Experimental, Apache-2.0.
One nonrecursive directory per compiler-enforced noncopyable owner. Explicit
close, owned byte names, bounded waits, and visible event loss. No background
threads, global registry, recursive traversal, or command execution in core.

See [the API contract](docs/API.md) and [contribution standards](CONTRIBUTING.md).
Initial consumer is the standalone command rerun example; Honk integration is
future work. This checkout is not yet published or registered in the catalog.
