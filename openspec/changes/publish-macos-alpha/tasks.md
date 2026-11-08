# Tasks

## 1. Privacy and release contract

- [ ] 1.1 Back up and replace only the affected merge metadata with coordinated noreply identity and guarded ref update; verify identical tree/message/parents and clean full-history publication scan.
- [x] 1.2 Add accepted D114 and normative experimental-alpha/compiler/installer/publication contracts; update bounded metadata, release notes and limitations and verify release-contract/spec/docs/roadmap gates without milestone promotion.

## 2. Portable native toolchain

- [x] 2.1 Implement explicit installed macOS system-Clang selection and exact version/family/target probing, preserving pinned defaults and exact runtime provenance; verify positive/negative/missing/limited probe tests and all reachable allocation-failure teardown indices.
- [x] 2.2 Use actual selected compiler identity/policy/target in doctor and native cache; verify changed-identity cache behavior, poisoned-environment exclusion, installed debug/release VM/native comparisons and zero allocation/byte counters.

## 3. macOS package

- [x] 3.1 Generate checksummed ARM64/Intel .pkg files from validated complete prefixes with collision-safe scripts, distinct version-specific receipts and atomic wrapper activation; verify focused inventory, malformed/tampered/link/foreign-command/prefix/receipt and error-path tests and unchanged source input.
- [ ] 3.2 Add real macOS package/layout and installation/upgrade/activation smoke checks plus user installation documentation; verify doctor, VM run/test, both native profiles and moved whole-prefix operation on both CI hosts.

## 4. Automated publication

- [x] 4.1 Implement tag/manual release workflow and exact-commit/version/main/CI/security gates with scoped write permission; verify actionlint, validation/error-case tests and PR build-only behavior.
- [x] 4.2 Validate artifact inventory/checksums, fill a draft and publish only after all gates, protecting existing public assets; verify mismatch/failure/rerun handling and document release operation.

## 5. Integrated acceptance and delivery

- [ ] 5.1 Pass strict build/focused tests and required architecture, roadmap, generated/spec/example/docs, memory, release, publication/history, secret-scan and OpenSpec gates; record exact commands/platform scope.
- [ ] 5.2 Obtain successful full current-head repository CI and both portable macOS package gates; integrate with noreply identity, publish the authorized alpha tag and follow release workflow to successful public prerelease.
- [ ] 5.3 Download the published assets and verify checksums/package contents against the tested revision; deliver the release URL and verified global Mac installation command, preserving explicit alpha limitations.

## Workflow follow-up

- Archive only after implementation review and publication evidence are complete.
- Product milestone verification remains independently evidence-scoped.
