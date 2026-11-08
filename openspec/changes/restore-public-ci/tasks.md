# Tasks

This checklist records implementation and local verification. Acceptance of
the published integrated revision follows the CI and review steps below.

## 1. Portable storage and Windows execution

- [x] 1.1 Centralize fundamental alignment and preserve both branches' arena/map tests; verify strict and sanitizer-backed focused tests, complete failure sweeps, and zero live allocation/byte counters.
- [x] 1.2 Repair native descriptors, compiler arguments, object ownership, runtime profiles, and binary streams; verify CLI/profile, relocation/concurrent-build, argument, and exact-byte regressions, with historical Windows results linked in the PR.

## 2. Parser and formatter contracts

- [x] 2.1 Preserve parser limit status and test boundary/allocation failures; verify the focused frontend suite and retained fuzz reproducer.
- [x] 2.2 Reconcile accepted D071 separator wording, grammar, and AST oracles; verify positive/negative comparisons, formatter fixtures, and raw/formatted VM/native differential tests.

## 3. JIT memory

- [x] 3.1 Own and account for readable entry padding while preserving W^X and cleanup; verify prefix/budget/failure-index tests and record the historical Linux JIT ASan+UBSan result of 105/105 tests with its revision scope in the PR.

## 4. CI and security orchestration

- [x] 4.1 Preserve full platform/sanitizer/fuzz suites, native preflight, and failed-test diagnostics; verify workflow lint and focused compiler configuration tests.
- [x] 4.2 Bound seven IPO probes individually and in aggregate; verify the registered timeout and successful Windows runs exceeding the former 60-second total.
- [x] 4.3 Scan complete submitted history and retain publication rules; verify source-security results for the pre-cleanup source revision and the local publication gate.

## 5. Integration and documentation

- [x] 5.1 Integrate concurrent `main` changes without duplicate alignment or RTC implementations; verify the strict combined build and focused CTest families.
- [x] 5.2 Connect OpenSpec, contributor guidance, normative contracts, decisions, owners, and evidence; verify the CLI resolves this project and all documentation links.
- [x] 5.3 Validate the completed OpenSpec record and integrated repository gates; run OpenSpec strict validation plus architecture, roadmap, generated, specification/example, memory-policy, release-contract, documentation, and publication checks.

## Workflow follow-up

- Require all jobs in both full CI matrices and both source-security runs to
  pass for the final PR head; record its exact revision and job links in
  [PR #1](https://github.com/marimay-lang/marimay/pull/1).
- Preserve revision/platform scope when reporting earlier results; a historical
  result does not establish acceptance of the integrated head.
- Complete review and merge before archiving this change. Roadmap milestone
  status continues to require its own documented evidence.
