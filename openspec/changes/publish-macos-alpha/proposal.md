# Proposal

## Why

The owner requested the first downloadable alpha with a working global `mmay`
installation on macOS. Current CI produces temporary archives, pins the CI C
compiler path/version, and does not publish a GitHub Release; a subsequent merge
also reintroduced a private author email into reachable commit metadata.

## What Changes

- Repair only the affected merge metadata using GitHub noreply identity, with a
  private backup, identical tree/parents, coordinated refs and a guarded lease.
  Preserve full-history publication and secret checks without exceptions.
- Introduce accepted D114 for an owner-authorized `experimental-alpha` macOS
  prerelease, keeping the bounded claim and incomplete CLI0/CP40 readiness.
- Add an explicit portable macOS distribution compiler policy; keep exact
  shipped-runtime provenance validation and use the actual selected compiler
  identity in native cache keys and diagnostics.
- Package complete ARM64 and Intel installations as checksummed `.pkg` and
  `.tar.gz` files. The package installs a versioned prefix and a global command
  wrapper; no source build, shell-profile edits or symlink-dependent lookup.
- Automate tag-scoped validation, building, installed VM/native smoke tests,
  draft asset upload and prerelease publication after all release gates pass.

## Capabilities

### New Capabilities

- `macos-alpha-distribution`: Bounded experimental release identity, portable
  local Apple toolchain selection, whole-prefix installer and verified automated
  publication, linking normative `docs/spec/toolchain.md` sections 6.4, 9 and 10.1
  and accepted D030, D064, D083, D112 and D114.

### Modified Capabilities

None: no capabilities are registered in the current OpenSpec inventory.
Normative authority remains in `docs/spec/`, not this planning record.

## Impact

Native toolchain/build/doctor owners, build configuration, distribution tools,
release metadata/tests, GitHub workflows, installation/release/security docs and
focused allocator/platform/package tests. No source-language syntax or semantics
change; no formatter syntax fixtures are required. VM/native installed smoke
tests retain the same observable program behavior. macOS 15+ ARM64 and Intel are
the first release scope; other existing CI coverage remains required. No N0,
CLI0, CP39, CP40, CP41 or CP42 milestone is marked verified by this publication.
