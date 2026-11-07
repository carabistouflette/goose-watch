# Security

Report suspected vulnerabilities through [GitHub private vulnerability reporting](https://github.com/carabistouflette/goose-watch/security/advisories/new).
Include the affected commit, Goose/compiler versions, platform and a small
reproducer. Keep exploit details and secrets out of public issues until a fix
is available. No security audit or response-time guarantee is claimed.

This experimental library has no stable release. Use an exact reviewed commit.
Treat all event names as untrusted bytes and escape them before displaying.
Notifications are hints, never authorization or a complete history. Watch paths
are not confined; intermediate symlinks may be followed. See [the API](docs/API.md)
for ownership, resource limits and notification-loss behavior.
