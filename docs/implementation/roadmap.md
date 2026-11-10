# Marimay product roadmap

Status: authoritative product gate state. Release ordering and dates live in
GitHub milestones; see [ROADMAP.md](../../ROADMAP.md).
Audit date: 2026-11-10.
Audit base revision: `68e15227226d9ac76edd329319c009e85a8246c5`.

This file answers what the product can claim and what blocks the next release.
It does not schedule work: the dated release slots are GitHub milestones, and
the work queue is their issues. A gate state here and a due date on a milestone
are different facts and must not be derived from one another.
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
- Bounded explicit typed JSON is verified; reflection, derive syntax, an
  unbounded DOM, and dynamic `any` remain outside the language contract.
- Native `mmay run`, `mmay init`, `mmay doctor`, and distribution lifecycle
  implementations are present in the experimental preview. Their complete CP39
  acceptance and the CP40 publication audit remain open; command availability
  must not be read as checkpoint verification.
- Portable public `.myb`, untrusted `mmay exec`, dependency resolution,
  registry operations, production LSP/test tooling, C FFI, and cross
  compilation are not part of CLI0.
- I6 task suspension, scheduler, cancellation, non-blocking I/O, DNS/TCP/HTTP,
  channels, race/select, and isolates are not implemented.
- N0 lacks external legal/control/PATH evidence. D123 excludes N0 from the CLI0
  claim and records the accepted residual risk; the gate returns as a hard
  prerequisite for I7 registry namespaces, any signing or notarization claim,
  and the P4 1.0 audit. This roadmap still does not authorize public identities,
  registries, namespaces, signing keys or supported releases. D114 separately
  authorizes the experimental macOS prerelease and installer; its publication
  does not close N0, CLI0 or CP39–CP42.
- Full Tier 1, sanitizer, installer, sustained fuzz, compatibility, bootstrap,
  external-pilot, and production-service evidence required by later gates is
  not complete.

## Now / Next / Later

Release slots are GitHub milestones; this section names the slot each checkpoint
belongs to. The milestone description carries the exit criteria and the date.

- **Now — `0.2.0-alpha.4`, due 2027-02-28:**
  [CP42 — notes-driven CLI ergonomics](checkpoints.md#cp42-notes-driven-cli-ergonomics)
  closes. All ten portfolio applications are implemented; what remains is
  acceptance work, not features: the combined coverage matrix with a disposition
  for every finding (#24), startup budgets met or re-budgeted (#25), one bounded
  perf timebox for `frequency-map` / `unique-set` / `rune-frequency` and #3 (#26),
  the canonical notes program in CI (#27), then the release (#28). The stale
  sentence about finishing the mutable-capture warning and the delimited /
  source-lint applications was already delivered on 2026-09-09 (D103,
  `cp42-stateful-2026-09-09.md`) and no longer describes the next step.
- **Next — `0.3.0-alpha.5`, due 2027-06-14:** CP41 x86_64 native code-generation
  pressure (#29), CP39 clean-host installation (#30), CP40 publication audit
  (#31), plus the N0 identity decision (#32) and the E0 gate decision (#33).
  CLI0 cannot close by carrying an open-ended N0 or an unachievable E0.
- **Later — `0.4.0-alpha.6`, provisional 2028-02-29:** the I6 vertical slice.
  Cards #14-#22 stay in the PARKED milestone until CLI0 closes; they are
  specified and reviewed, waiting on entry rather than on design.
- **Then:** I7-I9 productize the ecosystem and platform, P0 opens the broader
  alpha, and self-hosting (I10) follows a verified E0. Beta, reference service,
  RC and 1.0 are not scheduled and are not implied by any date above.

### Deferred backlog

Two standard-library gaps are specified or needed but sit in no release slot. They
are GitHub issues in the Backlog milestone with `parked`, which means
"explicitly deferred, not in any release" — not "forgotten":

- **`std/time` (#40).** §7 of the standard library already specifies `Instant`,
  `WallTime`, `Duration`, `now`, `wallNow` and `sleep`; no package exists, so the
  reference slice cannot timestamp anything and `std/task`'s `withTimeout` has no
  duration type. The issue splits the surface on purpose: the clock-free core is I5
  work and unblocks `Duration`, while suspending `sleep` waits for I6 rather than
  shipping a blocking call that contradicts §7. Two decisions come first — whether
  clock reads are a capability, and whether blocking `sleep` may ship outside I6.
- **`std/math` (#41).** `f64` arithmetic is specified by LANG-036, but no function
  exists: no `sqrt`, `abs`, rounding, `min`/`max` or constants. The blocking problem
  is not the function list but determinism: C `libm` is not bit-identical across
  Tier 1 platforms, so a decision must choose, per function group, between a
  self-implemented bit-exact algorithm and an explicit platform-dependence
  exception. Until then the specification stays silent and nothing is promised.

Each issue states its own entry condition, so neither can be pulled into a slot by
preference: the trigger is a slot application that needs it, or the gate that needs
the type.

A perf item enters the alpha line only inside a dated timebox with a recorded
exit: either a paired improvement or an accepted debt with the next hypothesis.
CP41 remains the separately evidenced cross-target native-code checkpoint.

## Critical path

```text
D0 + bounded I0/A0/I1-I5 prerequisites (N0 deferred by D123)
  -> CP29-CP38: bounded local CLI language/core surface
  -> CP42: JSON/collections/control-flow/API ergonomics + ten external CLI apps
  -> CP41: I4 x86_64 generated-C live-state and spill/branch pressure
  -> CP39-CP40: installable artifact and CLI0 audit
  -> I6: typed JSON/config -> concurrent HTTP fan-out -> timeout/race/cancel
         -> lexical defer cleanup -> deterministic VM/native trace
  -> E0: agent pilot under D124 (3 sessions, 2 platforms)
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
| N0 | Naming and distribution identity | `blocked` | `local` | Deferred by D123: the evidence is genuinely absent, but no release claim depends on it until I7, signing, or the 1.0 audit. | When it returns, close every naming-audit row and link dated control/install evidence; [evidence](evidence/README.md#n0). |
| I0 | Repository and C17 bootstrap | `in-progress` | `pushed-ci` | The complete Tier 1 bootstrap/compiler/memory-tool matrix is not linked. | Debug/release/compiler/sanitizer/failure-injection checks pass on every claimed Tier 1 host at one revision; [evidence](evidence/README.md#i0). |
| A0 | Bounded compiler architecture | `in-progress` | `tier-1-partial` | Architecture evidence is missing on part of Tier 1. | `./tools/check-architecture` and focused ownership review pass on the full Tier 1 matrix; [CP0](checkpoints.md#cp0-bounded-compiler-architecture). |
| I1 | Lossless frontend and formatter | `in-progress` | `pushed-ci` | Remaining production fixtures and the accepted sustained fuzz gate are not closed on Tier 1. | Every grammar production has positive/negative/recovery/formatter evidence and parser/formatter campaigns meet the test-strategy budget; [evidence](evidence/README.md#i1). |
| I2 | Typed core and MIR oracle | `in-progress` | `pushed-ci` | The CP29 matrix assigns the remaining baseline-language gaps, but those criteria are not yet closed. | Execute the CP29 closure assignments; complete conformance, verifier, evaluator, completion, and invalidation gates pass; [matrix](cli0-closure-matrix.md). |
| I3 | Typed bytecode VM | `in-progress` | `pushed-ci` | Portable public bundles and the full hostile-verifier/fuzz/limit gate remain open. | Every opcode and required bundle boundary passes verifier/execution/fuzz/limit tests and MIR/VM differential on Tier 1; [evidence](evidence/README.md#i3). |
| I4 | C17 AOT and equivalence | `in-progress` | `pushed-ci` | Native run/install/doctor, the full compiler/platform differential matrix, and the measured x86_64 code-pressure gap remain open. | CP39 and CP41 plus complete O0/O2 Clang/GCC/MSVC sanitizer and engine-equivalence evidence meet the I4 gate; [CP41](checkpoints.md#cp41-x86_64-native-code-generation-pressure). |
| I5 | Managed data and standard core | `in-progress` | `pushed-ci` | The CP30–CP38 bounded CLI data surface is verified, but remaining managed-heap/property/GC/workload and claimed-platform gates are open. | Those remaining gates close with VM/native and claimed-platform evidence; [CP38 evidence](evidence/cp38-bounded-typed-json-2026-09-01.md). |
| CLI0 | Bounded public CLI alpha | `planned` | `none` | CP42 startup budgets, perf timebox, canonical program and release, then CP41 and CP39–CP40, remain open. N0 is excluded from this claim by D123. | CP29–CP42 are verified, the claimed platform gates are closed, the CP40 audit supports only the bounded CLI claim, and the contract states the D123 residual risk. |
| I6 | Direct-style tasks, scheduler, and I/O | `planned` | `none` | CLI0 and CP41 precede the executable I6 prototype. | Suspension, scope/task lifetime, cancellation, race/select, non-blocking I/O, and deterministic VM/native traces pass the defined vertical scenario. |
| E0 | Pre-self-host ergonomics pilot | `planned` | `none` | A working I6 vertical slice is unavailable. | Under D124: three independent agent sessions on at least two platforms follow [the pilot protocol](pilot-protocol.md) with zero undocumented steps, and the gate report separates what was proven from what was not. |
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
| Experimental macOS alpha (`D114`) | A checksummed download of the checker-backed local slice for macOS 15+ ARM64/Intel, with explicit prerequisites and exclusions. | Owner authorization plus exact-revision repository CI/security, both full macOS suites, installed VM/native smoke, and artifact validation. `publicationReady: false`; no CLI0/CP40/N0 promotion, signing or notarization claim. |
| Bounded CLI alpha (`CLI0`) | Small local CLI programs using the bounded CLI surface on explicitly verified hosts. | CP42 coverage and budgets, CP41, claimed Tier 1 matrices, CP40 audit, user authorization, and the D123 residual-risk statement. N0 is excluded from this claim until I7 or signing. No I6/network/registry/FFI/public-bytecode/self-hosting claim. |
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
verified. CP38 is verified. The user's 2026-09-05 priority change inserts CP42
before CP41; CP42 is the sole in-progress card. CP42 must not be called verified
on the strength of its initial automatic-JSON slice alone.
The user's 2026-09-06 extension requires the complete
[external CLI portfolio](cli-ergonomics-suite.md) before CP42/CLI0 closure.
Useful applications, combined coverage and resolved ergonomics findings are
required alongside conformance; merely creating repositories is not acceptance.

On 2026-09-07 the user accepted the
[five-cycle execution route](checkpoints.md#accepted-cli0-execution-route):
reliable baseline, shared CLI contracts, application families, measured
performance, then candidate audit. Application waves replace numeric order;
affected failures block their acceptance and release while independent host,
install and N0 preparation can proceed. CP42 remains the sole active checkpoint
and verification order remains CP42 → CP41 → CP39 → CP40. Existing numerical
budgets, all ten applications, the D057 five-platform obligation and N0 are
unchanged. Scope/threshold research does not itself amend these contracts.

## CLI0 CP41 boundary and post-CLI0 I6/E0

After CP38 and the user-prioritized CP42 ergonomics acceptance, before CP39 execute
[CP41](checkpoints.md#cp41-x86_64-native-code-generation-pressure) against a
clean D056 Linux x86_64 baseline. It closes the already observed generated-C
live-state/spill gap without weakening typed MIR, runtime checks, or VM/native
semantics. This ordering prevents a cycle between CLI0 and its I4 prerequisite
and ensures the installed/audited artifact contains the repair. Cross-language
ratios remain diagnostic and the full D057 Tier 1 registry remains a separate
release gate.

After CP41, CP39, and CP40 are verified and CLI0 is closed, audit D010–D012,
D029, D041, the execution prototype gates, and the current runtime owners
before assigning CP43+. CP42 now belongs to CLI ergonomics, not I6.
The first I6 plan must prove an atomic executable
scenario:

```text
typed JSON/config
  -> concurrent HTTP fan-out
  -> timeout/race/cancellation
  -> lexical defer cleanup
  -> VM/native deterministic trace comparison
```

E0 follows the working slice and runs the agent pilot of D124 and
[`pilot-protocol.md`](pilot-protocol.md): clean-environment installation,
documented-command completeness, two-platform reproducibility, diagnostics
quality, and end-to-end use of the bounded surface. The human hypotheses —
multi-layer `mapError`, D043 dot-call discoverability under named imports,
inferred suspension without `async` color, and time to complete — are not
measured by an agent pilot and stay open as optional written human reports with
no deadline. No gate may present the pilot as evidence for them.

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
