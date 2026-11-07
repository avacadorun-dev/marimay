# Security policy

Marimay is experimental. No version is currently offered as a production
security-supported release. Fixes target `main`; historical preview snapshots
may remain unpatched.

## Report a vulnerability privately

Use **Security → Report a vulnerability** on
[this repository](https://github.com/avacadorun-dev/marimay/security/advisories/new).
Include the revision, platform, a minimal reproducer with synthetic data,
impact, and any proposed mitigation. Do not open public issues with exploits,
credentials, private URLs, or personal data. No response-time commitment is
made while the project is experimental.

## Data and trust boundaries

- The reviewed compiler/runtime source contains no telemetry client, analytics
  endpoint, automatic crash uploader, or update checker. Default CMake builds
  use local inputs and do not fetch dependencies. This is a scoped source-review
  finding, not a proof about every tool on a contributor's machine.
- Filesystem and named environment access use explicit runtime capabilities.
  CMake, Python development scripts, the native C compiler/linker, and the OS
  are trusted host tools outside that model. They may inherit the host
  environment. Use a clean environment when building untrusted input.
- `mmay run` executes code. VM verification is not an OS sandbox. Native code
  and unsafe FFI need OS isolation for hostile workloads. Local execution has
  no default instruction-count or elapsed-time cutoff.
- Networking and a complete package ecosystem remain staged. Their future
  specification is not an implemented security guarantee.
- Optional JIT builds require explicit local SLJIT sources verified against
  pinned hashes. CI explicitly fetches that public dependency for its JIT job.

## Public CI and logs

GitHub-hosted runners receive the public checkout. Workflows use read-only
repository permissions, do not persist checkout credentials, use no repository
secrets, and use no private/self-hosted runners. They contact GitHub and public
toolchain/package providers. Selected build archives, synthetic fuzz reproducers,
and test diagnostics may be uploaded as GitHub artifacts; job logs are public.

Do not run artifact-upload workflows against private projects or secret-bearing
environments. Diagnostic collectors retain generated source and logs; they are
not universal secret scrubbers. The local benchmark monitor records process
arguments, so inspect its output before sharing it.

See [publication policy](docs/security/publication.md) and the
[security review](docs/security/review-2026-11-08.md) for scope and limitations.
If a real credential is committed, revoke or rotate it first. Deleting a file
does not remove its history or already downloaded copies.
