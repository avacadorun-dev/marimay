# Spec Delta

## Purpose

Deliver the owner's explicitly authorized experimental macOS alpha with a
working installed command, preserving the normative toolchain and privacy
contracts in `docs/spec/toolchain.md` and accepted D114.

## ADDED Requirements

### Requirement: Bounded experimental publication
The release SHALL identify `0.1.0-alpha.1` as `experimental-alpha` with the
`local-checker-backed-slice` claim and `publicationReady:false`. This readiness
field SHALL remain scoped to CLI0/CP40 acceptance. Publication SHALL NOT promote
roadmap milestones or claim signing, notarization, N0 or production readiness.

#### Scenario: Inspect a downloaded alpha
- **WHEN** a user inspects installed version metadata and release notes
- **THEN** both identify the experimental scope, supported macOS hosts,
  prerequisites and unavailable capabilities consistently

### Requirement: Portable local Apple toolchain
An explicitly enabled macOS distribution SHALL use fixed system Apple developer
tool shims in a cleared environment, an explicit host architecture and macOS 15
deployment target. It SHALL accept AppleClang 16 or newer after a bounded
identity/target probe. Project inputs, `CC` and user `PATH` SHALL NOT select a
compiler. Runtime-producer provenance checks SHALL remain exact.

#### Scenario: Use the package on another supported Mac
- **WHEN** the installed package runs doctor or builds with compatible local CLT
- **THEN** it uses the local compiler identity rather than the CI installation
  path, and native cache identity includes the actual selected toolchain

#### Scenario: Reject an unhealthy local toolchain
- **WHEN** tools are absent, foreign, malformed, too old or target-incompatible
- **THEN** doctor/native build fails with bounded existing diagnostics and does
  not fall back to an environment-selected compiler

### Requirement: Global macOS installer
The `.pkg` SHALL install the complete versioned prefix and activate
`/usr/local/bin/mmay` through a wrapper executing its real binary. It SHALL
reject unrelated or modified command/prefix/receipt collisions. A new version SHALL
remain inactive until validation succeeds; activation SHALL be atomic. Installer
failure SHALL preserve the previously active command. Shell profiles SHALL NOT
be edited.

#### Scenario: Install and use the global command
- **WHEN** a user installs the matching architecture package on macOS 15+
- **THEN** `mmay` works from another directory with doctor, VM execution and
  debug/release native builds, with all runtime/header/data files present

#### Scenario: Preserve existing installations and foreign files
- **WHEN** a foreign/modified command or prefix conflicts, or activation fails
- **THEN** installation fails without replacing the active command or deleting
  unrelated data; incomplete inactive payloads remain installer-managed

#### Scenario: Reject same-version overwrite
- **WHEN** the candidate version prefix already exists, including a reinstall
- **THEN** the installer rejects it unchanged and requires a fresh version slot
  or explicit user recovery before installing again

#### Scenario: Preserve Installer receipt ownership
- **WHEN** an existing receipt is foreign, partial, corrupt or inconsistent
  with the verified old active prefix and target volume
- **THEN** installation fails without replacing its receipt or active command

#### Scenario: Match canonical macOS receipt location
- **WHEN** the existing owned version is `V` on a validated target volume
- **THEN** receipt `install-location` must exactly equal the volume-relative
  `usr/local/lib/marimay/V`; absolute or alternate spellings are rejected

### Requirement: Version-specific receipt ownership
Each version SHALL have a distinct `org.marimay.mmay.V` Installer receipt.
Candidate and retired constant receipts SHALL be absent before installation.
An active previous packaged version SHALL require its verified matching receipt;
its receipt and BOM-owned files SHALL remain unchanged.

#### Scenario: Preserve a previous packaged version
- **WHEN** a new version is installed beside a verified active previous version
- **THEN** Installer retains the previous receipt and all its BOM-owned files,
  and the new wrapper activates only after complete candidate validation

### Requirement: Verified immutable release publication
Tag/manual publication SHALL validate tag/version/commit/main ancestry, complete
repository CI and security, both macOS full test suites, installed smoke tests,
archive/package inventory and SHA-256 checksums before publishing a prerelease.
Only the publishing job SHALL have repository write permission. Existing
published release assets SHALL NOT be overwritten.

#### Scenario: Publish the first alpha
- **WHEN** the authorized alpha tag targets a verified main commit
- **THEN** both architecture packages and archives with checksums are uploaded
  to a draft, and the complete validated draft becomes a GitHub prerelease

#### Scenario: Block a defective or mismatched release
- **WHEN** any test, security check, identity or checksum fails
- **THEN** no public release is published from that attempt

### Requirement: Preserve public-history privacy
Reachable public release history SHALL exclude private author emails and retired
payloads. Metadata repair SHALL preserve source trees and parents, keep a private
backup and coordinate a guarded ref update. Publication/security rules SHALL NOT
be weakened to accept the detected private metadata.

#### Scenario: Repair the post-merge finding
- **WHEN** the affected merge is replaced with GitHub noreply metadata
- **THEN** its tree/parents remain identical and the full-history guard passes
  without a new allowlist or scanner suppression
