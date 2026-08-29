# Marimay product roadmap

Status: authoritative product gate state and release sequencing.
Audit date: 2026-08-29.
Audit base revision: `aeb2baf8644963a8e84cf816c352b4c286dc0f8a`.

This file answers what the product can claim and what blocks the next release.
The [checkpoint ledger](checkpoints.md) is the only execution queue, the
[TSV register](roadmap.tsv) is the compact machine source of truth for IDs and
states, and the [evidence index](evidence/README.md) owns direct report links.
Historical delivery does not silently raise a product gate.

## Current

### Implemented surface

The portable C17 Stage 0 builds `mmay`. For the checker-backed local/private
preview surface it provides help/version, lossless parsing, canonical
formatting, authoritative `mmay check`, verified bytecode-VM `mmay run`, native
`mmay build`, and deterministic `mmay test --engine both`. Executable slices
cover scalar control flow, checked integer behavior, deterministic `f64`,
strings and explicit D046 `println`, nominal records/enums/interfaces,
`Option`/`Result`/`?`, lexical `defer`, local packages and D043 dot calls, and
managed `[]T` arrays through VM/native differential paths. CP13–CP28 added
profile-coherent native artifacts, cross-package generic data, bounded array
mutation/resources, and verified performance-oriented MIR/runtime changes.
CP29 added the accepted bounded pre-beta release contract, exact executable
versus staged command metadata, install/relocation checks, and a 42-row CLI0
closure matrix without changing any product-gate state or publishing artifacts.
CP30 now implements closed primitive interpolation, boolean/all-width integer
formatting, integer parsing, and bounded dynamic string construction across the
checker, verified MIR, bytecode VM, and native C17. Its local sustained fuzz,
sanitizer, allocation, differential, and performance gates are verified.
CP31 adds bounded UTF-8 program arguments, test injection, and cleanup-safe
portable statuses across VM and native execution with local differential,
allocation-failure, and sanitizer evidence.
CP32 adds bounded typed stdin, independent stdout/stderr, explicit per-stream
flush, exact capabilities, bounded partial/retry handling, and binary process
fixtures across the MIR oracle, verified bytecode VM, and native C17. Its
local differential, closed-pipe, allocation-failure, and sanitizer gates are
verified.
CP33 adds owned counted-UTF-8 text operations, nominal mutable `Bytes`, bounded
text/byte builders, explicit UTF-8 conversion, and binary standard-stream I/O.
Its local randomized, hostile-verifier, allocation-failure, sustained-fuzz,
sanitizer, and VM/native differential gates are verified.
CP34 adds typed lexical paths, separately granted read/write roots, bounded
whole-file and explicit-handle I/O, atomic replacement, metadata, and sorted
directory enumeration. Its local path/capability, symlink, hostile-verifier,
allocation-failure, sanitizer, and VM/native process gates are verified.
CP35 adds capability-confined exact environment lookup, explicit
`--allow-env` grants, and normalized absolute current-directory/executable
paths across injected MIR, verified bytecode VM, and native C17 execution. Its
local hostile-verifier, redaction, allocation-failure, sanitizer, relocation,
cache-independence, and VM/native process gates are verified.

### Current limitations

- The implemented source surface is not the complete 1.0 specification; I1–I5
  remain `in-progress` and unsupported accepted constructs fail closed.
- `std/cli` and typed JSON remain CP37–CP38 work.
- Native `mmay run`, `mmay init`, `mmay doctor`, relocatable install lifecycle,
  and publication-ready artifacts remain CP39–CP40 work.
- Portable public `.myb`, untrusted `mmay exec`, dependency resolution,
  registry operations, production LSP/test tooling, C FFI, and cross
  compilation are not part of CLI0.
- I6 task suspension, scheduler, cancellation, non-blocking I/O, DNS/TCP/HTTP,
  channels, race/select, and isolates are not implemented.
- N0 lacks external legal/control/PATH evidence. No public identity, package,
  namespace, signing key, installer, tag, or release is authorized here.
- Full Tier 1, sanitizer, installer, sustained fuzz, compatibility, bootstrap,
  external-pilot, and production-service evidence required by later gates is
  not complete.

## Now / Next / Later

- **Now:** [CP37 — first-party CLI parser](checkpoints.md#cp37-first-party-cli-parser).
- **Next:** [CP38 — bounded typed JSON](checkpoints.md#cp38-bounded-typed-json).
- **Later:** CP38 finishes the bounded language/core surface; CP41 then
  closes the measured x86_64 native-code pressure gap before CP39–CP40 finish
  installation and the CLI0 audit. I6 next proves one real direct-style
  concurrent I/O vertical slice. E0 tests the semantic hypotheses externally,
  I7–I9 productize the ecosystem/platform, and only then do the broader alpha,
  self-hosting, beta, reference-service, RC, and 1.0 gates close.

No new array-only micro-optimization enters CP31–CP40 unless a correctness or
security blocker requires it. CP41 is the separately evidenced cross-target
native-code checkpoint; it does not reopen the earlier array-only sequence.

## Critical path

```text
D0 + bounded N0/I0/A0/I1-I5 prerequisites
  -> CP29-CP38: bounded local CLI language/core surface
  -> CP41: I4 x86_64 generated-C live-state and spill/branch pressure
  -> CP39-CP40: installable artifact and CLI0 audit
  -> I6: typed JSON/config -> concurrent HTTP fan-out -> timeout/race/cancel
         -> lexical defer cleanup -> deterministic VM/native trace
  -> E0: external ergonomics pilot
  -> I7 / I8 / I9 productization
  -> P0 broader ecosystem/service alpha
  -> I10 self-hosting after verified E0
  -> P1 beta -> P2 production service -> P3 RC -> P4 1.0
```

I7–I9 risk-reduction work may overlap after CLI0 where its dependencies permit,
but it cannot bypass N0 or substitute for I6/E0. I10 is not a prerequisite for
CLI0 or P0; its hard prerequisite is verified E0.

## Product gate dashboard

`implemented locally` means a named slice works at a recorded revision.
`verified` means the row's complete exit criterion and evidence scope are
closed. `publicly shippable` additionally requires every preceding release
dependency, controlled identity, release audit, and explicit publication
authorization. These are never synonyms.

<!-- roadmap-register:begin -->
| ID | Gate | State | Evidence scope | Concrete next blocker | Testable exit criterion / execution link |
|---|---|---|---|---|---|
| D0 | Research and design baseline | `verified` | `local` | — | Preserve the accepted decision/spec baseline; [evidence](evidence/README.md#d0). |
| N0 | Naming and distribution identity | `blocked` | `local` | External legal search plus control of domains/accounts/namespaces/signing identity and `mmay` PATH evidence are missing. | Close every naming-audit row, accept the follow-up identity decision, and link direct control/install evidence; [evidence](evidence/README.md#n0). |
| I0 | Repository and C17 bootstrap | `in-progress` | `pushed-ci` | The complete Tier 1 bootstrap/compiler/memory-tool matrix is not linked. | Debug/release/compiler/sanitizer/failure-injection checks pass on every claimed Tier 1 host at one revision; [evidence](evidence/README.md#i0). |
| A0 | Bounded compiler architecture | `in-progress` | `tier-1-partial` | Architecture evidence is missing on part of Tier 1. | `./tools/check-architecture` and focused ownership review pass on the full Tier 1 matrix; [CP0](checkpoints.md#cp0-bounded-compiler-architecture). |
| I1 | Lossless frontend and formatter | `in-progress` | `pushed-ci` | Remaining production fixtures and the accepted sustained fuzz gate are not closed on Tier 1. | Every grammar production has positive/negative/recovery/formatter evidence and parser/formatter campaigns meet the test-strategy budget; [evidence](evidence/README.md#i1). |
| I2 | Typed core and MIR oracle | `in-progress` | `pushed-ci` | The CP29 matrix assigns the remaining baseline-language gaps, but those criteria are not yet closed. | Execute the CP29 closure assignments; complete conformance, verifier, evaluator, completion, and invalidation gates pass; [matrix](cli0-closure-matrix.md). |
| I3 | Typed bytecode VM | `in-progress` | `pushed-ci` | Portable public bundles and the full hostile-verifier/fuzz/limit gate remain open. | Every opcode and required bundle boundary passes verifier/execution/fuzz/limit tests and MIR/VM differential on Tier 1; [evidence](evidence/README.md#i3). |
| I4 | C17 AOT and equivalence | `in-progress` | `pushed-ci` | Native run/install/doctor, the full compiler/platform differential matrix, and the measured x86_64 code-pressure gap remain open. | CP39 and CP41 plus complete O0/O2 Clang/GCC/MSVC sanitizer and engine-equivalence evidence meet the I4 gate; [CP41](checkpoints.md#cp41-x86_64-native-code-generation-pressure). |
| I5 | Managed data and standard core | `in-progress` | `pushed-ci` | CLI text/bytes/files/collections plus D007/D013 workload budgets are incomplete. | CP30–CP38 and managed-heap/property/GC/workload gates close with VM/native evidence; [queue](checkpoints.md#active-sequence-cp29-cp41). |
| CLI0 | Bounded public CLI alpha | `planned` | `none` | CP36–CP40, the CP41 I4 prerequisite, and the CP29 matrix remain open. | CP29–CP41 are verified, N0 and all claimed platform gates are closed, and the CP40 release audit supports only the bounded CLI claim. |
| I6 | Direct-style tasks, scheduler, and I/O | `planned` | `none` | CLI0 and CP41 precede the executable I6 prototype. | Suspension, scope/task lifetime, cancellation, race/select, non-blocking I/O, and deterministic VM/native traces pass the defined vertical scenario. |
| E0 | Pre-self-host ergonomics pilot | `planned` | `none` | A working I6 vertical slice is unavailable. | At least five external developers on two Tier 1 host families complete the defined tasks and every semantic finding is resolved or accepted. |
| I7 | Packages, cache, and supply chain | `planned` | `none` | N0 and external semantic feedback are not verified. | Deterministic offline lock/cache, hostile archive, provenance, and registry-prototype gates pass without dependency code execution. |
| I8 | Complete first-party developer loop | `planned` | `none` | The bounded CLI/package foundations are incomplete. | A fresh user completes init/fmt/check/test/run/build/doc/dependency workflows; test/LSP/doc/fuzz event contracts pass end to end. |
| I9 | FFI, capabilities, and platform layer | `planned` | `none` | I8 and the production platform contracts are incomplete. | Tier 1 FFI/capability/TLS/HTTP/installer/unsafe audits pass with no untracked pointer or resource escape. |
| P0 | Broader ecosystem and service alpha | `planned` | `none` | E0, N0, and I7–I9 productization are incomplete. | Public examples/packages and the five-platform D057 registry pass only through public APIs and direct evidence. |
| I10 | Self-hosting and reproducibility | `planned` | `none` | E0 is not verified. | Stage 0 builds Stage 1, Stage 1 builds Stage 2, normalized artifacts and conformance agree on two independent Tier 1 environments. |
| P1 | Beta compatibility and security | `planned` | `none` | P0 and reproducible self-hosting are incomplete. | Compatibility, sustained fuzz, supply-chain, signing, performance, sanitizer, and security gates are continuously green. |
| P2 | Production reference service | `planned` | `none` | Beta public APIs and operational contracts are unavailable. | The required HTTP/TLS/database/task service meets load, failure-injection, shutdown, upgrade, and rollback SLOs. |
| P3 | 1.0 release candidate | `planned` | `none` | Production-service and all release matrices are incomplete. | Complete platform, sanitizer, fuzz, bootstrap, compatibility, installer, package, service, and performance audit is green. |
| P4 | Production 1.0 | `planned` | `none` | No verified RC soak and independent completion audit exist. | Every normative must, decision gate, platform claim, and operational claim maps to direct evidence after the RC soak. |
<!-- roadmap-register:end -->

The state/evidence projection above is validated against `roadmap.tsv` by
`./tools/check-roadmap`. Long report lists live in the evidence index, not in
dashboard cells.

## Release claims and ladder

| Stage | Allowed claim | Additional release gate |
|---|---|---|
| Private preview | Checker-backed local/private C17 bootstrap slices work at named revisions. | No public identity or compatibility claim. |
| Bounded CLI alpha (`CLI0`) | Small local CLI programs using only the CP29–CP40 surface on explicitly verified hosts. | N0, CP41, claimed Tier 1 matrices, CP40 audit, and user authorization. No I6/network/registry/FFI/public-bytecode/self-hosting claim. |
| Concurrency/service pilot (`I6` + `E0`) | Working direct-style concurrent I/O prototype evaluated by external developers. | I6 deterministic prototype plus verified E0 findings. |
| Ecosystem alpha (`P0`) | Broader packages/platform/service-oriented alpha using public APIs. | N0, E0, I7–I9, public package/workload evidence. |
| Beta (`P1`) | Compatibility and security hardening with a frozen surface except blockers. | Self-hosting, compatibility, fuzz, supply-chain and security gates. |
| RC (`P3`, after `P2`) | 1.0 candidate backed by the production reference service. | P2 load/operations plus complete release audit. |
| 1.0 (`P4`) | Production 1.0 on the explicitly published platform tiers. | RC soak and independent requirement-to-evidence audit. |

Prepared artifacts are not published by completing a documentation or
checkpoint audit. Tagging, signing, uploading, reserving a namespace, or
announcing a release always requires explicit user authorization.

## CLI0 boundary

CLI0 combines CP29–CP40 and is deliberately narrower than P0. It includes
runtime strings/formatting, arguments and exit status, bounded standard
streams, UTF-8/bytes/builders, local paths/files/environment, `Map`/`Set`, a
first-party CLI parser, typed JSON, native run/init/doctor, installable
artifacts, and a publication-ready audit. It does not include networking,
direct-style tasks, registry/dependency workflows, production LSP/test tools,
C FFI, portable public bytecode, cross compilation, desktop support, or
self-hosting.

CP28 is the fixed algorithm/performance integration boundary. CP29 established
the release contract and closure matrix for N0, A0, I0, I1, and the applicable
I2–I5 slices. That matrix assigns missing evidence; it does not mark those gates
verified. CP36 is verified, and CP37 is now the sole ready card.

## CLI0 CP41 boundary and post-CLI0 I6/E0

After CP38 and before CP39, execute
[CP41](checkpoints.md#cp41-x86_64-native-code-generation-pressure) against a
clean D056 Linux x86_64 baseline. It closes the already observed generated-C
live-state/spill gap without weakening typed MIR, runtime checks, or VM/native
semantics. This ordering prevents a cycle between CLI0 and its I4 prerequisite
and ensures the installed/audited artifact contains the repair. Cross-language
ratios remain diagnostic and the full D057 Tier 1 registry remains a separate
release gate.

After CP41, CP39, and CP40 are verified and CLI0 is closed, audit D010–D012,
D029, D041, the execution prototype gates, and the current runtime owners
before assigning CP42+. The first I6 plan must prove an atomic executable
scenario:

```text
typed JSON/config
  -> concurrent HTTP fan-out
  -> timeout/race/cancellation
  -> lexical defer cleanup
  -> VM/native deterministic trace comparison
```

E0 follows the working slice and measures multi-layer `mapError`, D043 dot-call
discoverability under named type/function/namespace imports, inferred
suspension without `async` color, `scope`/`spawn`/`wait` plus cancellation and
race/select cleanup, native-toolchain setup steps, and time to complete. These
semantic hypotheses cannot freeze from internal tests alone.

## Gate contracts

The dashboard carries the one current blocker and exit test. The stable scope
of each gate remains:

- **D0:** research evidence, constitution, decisions, normative specs,
  consistency audit, implementation brief, and test strategy.
- **N0:** legal/search/confusable audit; controlled domains, accounts,
  namespace and signing identity; clean-image `mmay` collision lifecycle; and
  an accepted residual-risk decision.
- **I0/A0:** portable offline C17 build and CI foundation, central allocation
  discipline, stable diagnostics, bounded independently compiled owners, and
  focused tests under the architecture policy.
- **I1:** lossless bounded lexer/parser/CST, canonical non-configurable
  formatter, overlays, complete fixture axes, idempotence/round-trip, and
  sustained fuzzing.
- **I2:** deterministic packages/names/types/generics/nominals/interfaces,
  complete baseline control/error/pattern semantics, verified typed MIR oracle,
  authoritative check, and checker-owned compiler-service facts/completion.
- **I3:** versioned typed bytecode, hostile decoder/verifier, VM execution and
  limits, source traces, VM-backed tests, and sanitizer-clean bundle fuzzing.
- **I4:** portable safe C17 emission, profile/runtime/toolchain ownership,
  native run/build/test, O0/O2 compiler matrix, source traces, cache baselines,
  and exact MIR/VM/native comparison.
- **I5:** precise managed heaps, identity/mutation, strings/bytes/collections,
  deterministic standard core, equality/hash/iterator properties, and measured
  D007/D013 allocation/generic trade-offs.
- **I6:** inferred suspension, structured tasks and cancellation, scheduler,
  timers/channels/pollers/blocking pool, race/select, direct file/network/HTTP,
  isolates, diagnostics/tracing, fairness and memory/load evidence.
- **E0:** external install, JSON/DTO, immutable-tree, package-boundary and
  concurrent-HTTP tasks with quantitative transcripts and decision impact.
- **I7:** deterministic manifest/workspace/lock resolution, immutable offline
  cache, hostile archive/provenance security, and package lifecycle commands.
- **I8:** production test/doc/vet/fix/bench/fuzz and LSP/editor workflows with
  stable diagnostics/events and fresh-user CLI usability.
- **I9:** explicit unsafe C FFI, bounded blocking bridge, capabilities,
  production platform adapters, TLS/security/installer and graceful shutdown.
- **I10:** maintained C17 root, Stage 0→1→2 bootstrap, normalized reproducible
  artifacts, conformance parity, and offline independent rebuilds.
- **P0–P4:** public ecosystem workloads; beta compatibility/security; operated
  production reference service; complete RC evidence; and independent 1.0
  completion audit.

Normative detail remains in `docs/spec/`, accepted decisions, requirements, and
the test strategy. This roadmap does not amend language or runtime semantics.
