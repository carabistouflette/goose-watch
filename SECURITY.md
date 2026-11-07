# Security

This experimental library has no stable release. Use an exact reviewed commit.
Treat all event names as untrusted bytes and escape them before displaying.
Notifications are hints, never authorization or a complete history. Watch paths
are not confined; intermediate symlinks may be followed. See docs/API.md.

Do not post exploit details or secrets in public issues. Use the repository
host's private vulnerability reporting feature if enabled; otherwise contact
the maintainer privately before sharing a reproducer. No security audit or
response-time guarantee is claimed.

Before the first public release, configure and verify a concrete private
reporting endpoint and link it here. This local unpublished checkout has no
hosted reporting endpoint yet.
