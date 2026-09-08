# Marimay checkpoint ledger

Status: authoritative execution queue.
Audit date: 2026-08-24.

This ledger contains every checkpoint CP0–CP42 exactly once. Product gate
state and release sequencing live in the [product roadmap](roadmap.md); direct
reports live in the [evidence index](evidence/README.md); compact IDs, states,
dependencies, evidence scope, and next actions live in
[`roadmap.tsv`](roadmap.tsv). Historical cards record delivered outcomes and
remaining limitations, not reusable implementation instructions.

## Status and evidence model

The closed state vocabulary is:

| State | Meaning |
|---|---|
| `planned` | The outcome is defined but at least one predecessor is not verified. |
| `ready` | Every predecessor is verified; this is the next executable checkpoint. |
| `in-progress` | Work has actually started and exact-current-revision evidence exists. |
| `blocked` | A concrete external or prerequisite condition prevents progress; the unblock condition is named. |
| `verified` | The checkpoint's exact acceptance contract and required evidence scope are closed. |
| `superseded` | Another ID replaced the plan; the replacement and reason are named without erasing history. |

`implemented`, `locally complete`, `closed`, `done`, and `green` may describe
evidence but are not states. Evidence scopes are `none`, `local`, `pushed-ci`,
`tier-1-partial`, `tier-1-complete`, `external-pilot`, and `release-audit`.
Checkpoint verification at a deliberately bounded scope does not verify its
parent product gate. In particular, the historical CP0–CP28 delivery chain is
closed while A0 and I1–I5 still lack their full product-gate evidence.

## Common execution contract

Every active checkpoint uses these rules once; cards do not repeat them.

1. Start from a green main revision and a verified predecessor. Re-read
   `AGENTS.md`, relevant normative specs/decisions, the architecture contract,
   focused production owners, and focused tests before editing.
2. A semantic or public API change includes an accepted decision or existing
   decision link, normative spec, diagnostics, parser/checker/runtime coverage,
   VM/native differential, formatter fixtures, and public examples in the same
   change. Backends do not repair source semantics.
3. A runtime intrinsic or compiler-known standard API crosses typed MIR, MIR
   verification, bytecode encode/decode/verification, VM, generated C17, and
   native execution. Signature-only extern declarations are not delivery.
4. Respect owner boundaries and line ceilings. Do not add feature logic to an
   excepted, over-limit, or stable coordinator; create a narrow independently
   compiled owner and focused test owner when required.
5. Every added or modified allocation-capable C API covers success, material
   errors, and each reachable deterministic failure index, ending with
   `live_allocations == 0` and `live_bytes == 0` after teardown.
6. Host I/O uses bounded platform adapters, never a shell. Cover partial
   operations, interruption, invalid UTF-8, Windows paths/console behavior,
   capability denial, cleanup, and exact diagnostics where applicable.
7. Blocking Stage 0 I/O before I6 is an internal adapter with an explicit I6
   parking removal gate; it creates no public task-aware claim.
8. Parser/formatter/bytecode/JSON changes run the applicable sustained fuzz
   campaign. Runtime scheduling/shared capture changes run TSan. Linux release
   evidence includes LSan; allocator counters do not replace sanitizers or
   scheduled/manual Valgrind evidence.
9. Evidence records exact revision, commands/exits, platform/toolchain,
   sanitizer/differential/allocation results, limitations, and artifact hashes.
   Missing claimed-platform evidence forbids `verified`.
10. Do not publish, sign, tag, upload, reserve a namespace, commit, or push
    unless separately authorized by the user. When checkpoint commits are
    authorized, use one reviewable commit and push only after its full evidence
    gate is green. Prepared release artifacts are not a release.

After focused acceptance, run the applicable common tail:

```sh
./tools/check-roadmap
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-generated
./tools/check-doc-links
./tools/check-spec-terms
./tools/check-example-syntax
./tools/check-syntax-coverage
cmake --build --preset debug --parallel
ctest --preset debug --output-on-failure
cmake --build --preset release --parallel
ctest --preset release --output-on-failure
cmake --build --preset asan-ubsan --parallel
ctest --preset asan-ubsan --output-on-failure
git diff --check
```

## Active checkpoint card contract

Every CP29–CP41 card has exactly these sections: `Outcome`, `Why now /
dependencies`, `Authority and decisions`, `In scope`, `Out of scope`, `Owners
to inspect`, `Required vertical coverage`, `Focused tests and acceptance
commands`, `Evidence and completion rule`, `Removal gates / remaining
limitations`, and `Handoff packet for the next agent`. The owners list is a
context packet to inspect, not permission to enlarge coordinators.

<!-- checkpoint-register:begin -->
## Historical checkpoints 0-12

### CP0 - Bounded compiler architecture

State: `verified`. Parent gate: A0. Predecessor: none. Evidence scope: `tier-1-partial`.

Outcome: decomposed Stage 0 and its tests into bounded independently compiled
owners and added the architecture gate. Parent A0 remains `in-progress` because
the full Tier 1 scope is missing. Evidence:
[A0 checkpoint 0](evidence/a0-bounded-architecture-checkpoint-0-2026-08-14.md).

### CP1 - Nominal record enum and interface engines

State: `verified`. Parent gate: I2. Predecessor: CP0. Evidence scope: `pushed-ci`.

Outcome: records, enums, patterns, identity, D040 construction order, and
non-generic structural interfaces reached MIR, encoded VM, native C17, and
differential execution. Broader I2/I5 semantics remain open. Evidence:
[V0 nominal engines](evidence/v0-nominal-engines-checkpoint-1-2026-08-15.md).

### CP2 - Local packages imports and D043

State: `verified`. Parent gate: I2. Predecessor: CP1. Evidence scope: `local`.

Outcome: deterministic local packages, visibility, import forms, and the
owning-package dot-call set reached the shared resolver and both engines.
External dependency resolution remains I7. Evidence:
[CP2](evidence/i2-local-packages-checkpoint-2-2026-08-16.md).

### CP3 - Compiler service facts and completion

State: `verified`. Parent gate: I2. Predecessor: CP2. Evidence scope: `local`.

Outcome: bounded occurrence/definition/search facts and exact resolver-owned
dot-call completion became available through snapshot-safe internal APIs.
Production LSP transport remains I8. Evidence:
[CP3](evidence/i2-compiler-service-queries-checkpoint-3-2026-08-17.md).

### CP4 - Generic Option Result and question

State: `verified`. Parent gate: I2. Predecessor: CP3. Evidence scope: `local`.

Outcome: a bounded generic `Option`/`Result`/`?`/`mapError` vertical slice
reached checker, MIR, bytecode, VM, and native C17. Full generic and filesystem
runtime coverage remains open. Evidence:
[CP4](evidence/i2-generics-checkpoint-4-2026-08-17.md).

### CP5 - Generic managed arrays

State: `verified`. Parent gate: I5. Predecessor: CP4. Evidence scope: `local`.

Outcome: generic managed arrays, construction, mutation, bounds, iteration,
portable type tables, precise roots, and sorting/searching crossed both
engines. Other managed collections remained open. Evidence:
[CP5](evidence/i5-arrays-checkpoint-5-2026-08-17.md).

### CP6 - Numeric widths conversions and formatters

State: `verified`. Parent gate: I2. Predecessor: CP5. Evidence scope: `local`.

Outcome: the bounded numeric/text preview slice, D035/D042/D044 behavior,
explicit conversions, and D046 numeric formatters reached VM/native parity.
Runtime interpolation remained open. Evidence:
[CP6](evidence/cp6-numeric-widths-2026-08-17.md).

### CP7 - Lexical defer

State: `verified`. Parent gate: I2. Predecessor: CP6. Evidence scope: `local`.

Outcome: D041 lexical cleanup across all pre-suspension exits reached checker,
verified MIR, encoded VM, native C17, and differential tests. Cancellation and
suspension cleanup remain I6. Evidence: [CP7](evidence/cp7-lexical-defer-2026-08-18.md).

### CP8 - Authoritative batch check

State: `verified`. Parent gate: I2. Predecessor: CP7. Evidence scope: `local`.

Outcome: deterministic local discovery and fail-closed checker-backed
`mmay check` became public for the preview surface. Dependency/workspace and
complete baseline checking remain I7/I2. Evidence:
[CP8](evidence/cp8-authoritative-check-2026-08-18.md).

### CP9 - Verified VM run and stdout

State: `verified`. Parent gate: I3. Predecessor: CP8. Evidence scope: `local`.

Outcome: local `mmay run` selected a canonical entry, encoded/decoded/verified
bytecode, and implemented exact D046 stdout and closed process outcomes.
Source-visible arguments and full standard streams remained open. Evidence:
[CP9](evidence/cp9-ruda-run-stdout-2026-08-18.md).

### CP10 - Native build

State: `verified`. Parent gate: I4. Predecessor: CP9. Evidence scope: `pushed-ci`.

Outcome: content-addressed strict-C17 `mmay build` produced atomic native
artifacts with VM/native outcome parity. Native run/install/doctor remained
open. Evidence: [CP10](evidence/cp10-native-build-2026-08-18.md).

### CP11 - Built-in test loop

State: `verified`. Parent gate: I8. Predecessor: CP10. Evidence scope: `local`.

Outcome: deterministic isolated VM/native record/replay tests and exact output
capture became available. Filters, watch, cache, concurrent scheduling, and the
production event protocol remain I8. Evidence:
[CP11](evidence/cp11-ruda-test-2026-08-18.md).

### CP12 - Private preview integration

State: `verified`. Parent gate: I4. Predecessor: CP11. Evidence scope: `local`.

Outcome: a clean local/private preview exercised fmt/check/run/test/build and
the native artifact over the accepted corpus. It did not close I2–I5 or
authorize public 0.1.0. Evidence:
[CP12](evidence/cp12-final-preview-2026-08-18.md).

## Historical checkpoints 13-28

### CP13 - Profile-coherent native runtime

State: `verified`. Parent gate: I4. Predecessor: CP12. Evidence scope: `local`.

Outcome: requested output profile selects and validates a relocatable,
version-matched runtime artifact whose bytes enter the cache identity. Full
Tier 1 installation remains I4. Evidence:
[CP13](evidence/cp13-release-runtime-2026-08-19.md).

### CP14 - Cross-package managed arrays

State: `verified`. Parent gate: I5. Predecessor: CP13. Evidence scope: `local`.

Outcome: canonical instantiated managed arrays cross package arguments,
returns, aliases, nesting, bytecode, VM, and native execution. Evidence:
[CP14](evidence/cp14-imported-arrays-2026-08-19.md).

### CP15 - Imported generic enum results

State: `verified`. Parent gate: I2. Predecessor: CP14. Evidence scope: `local`.

Outcome: imported `Option`/`Result` constructors, values, patterns, `mapError`,
and `?` use canonical nominal identity through both engines. Evidence:
[CP15](evidence/cp15-imported-results-2026-08-20.md).

### CP16 - Receiver-directed collection names

State: `verified`. Parent gate: I2. Predecessor: CP15. Evidence scope: `local`.

Outcome: exact nominal receiver resolution makes ordinary free/dot collection
names coexist with compiler-known array operations across imports and
completion. Evidence:
[CP16](evidence/cp16-receiver-collection-names-2026-08-20.md).

### CP17 - Array mutation and reservation

State: `verified`. Parent gate: I5. Predecessor: CP16. Evidence scope: `local`.

Outcome: typed `pop`, `truncate`, `clear`, `reserve`, precise removal,
failure-atomic growth, and generation-checked iteration reached both engines.
Evidence: [CP17](evidence/cp17-array-mutation-2026-08-21.md).

### CP18 - Canonical managed-array spelling

State: `verified`. Parent gate: I1. Predecessor: CP17. Evidence scope: `local`.

Outcome: `[]T` replaced prelude `Array<T>` across source, formatter,
diagnostics, compiler service, and examples without changing runtime identity.
`Slice<T>` remains distinct. Evidence:
[CP18](evidence/cp18-array-spelling-2026-08-21.md).

### CP19 - Managed-heap resource policy

State: `verified`. Parent gate: I5. Predecessor: CP18. Evidence scope: `local`.

Outcome: `--max-heap-bytes` consistently distinguishes semantic length,
configured managed budget, and host allocation failure across VM/tests/native
artifacts. Evidence: [CP19](evidence/cp19-resource-limits-2026-08-21.md).

### CP20 - Reusable algorithm corpus

State: `verified`. Parent gate: I5. Predecessor: CP19. Evidence scope: `local`.

Outcome: reusable Marimay package boundaries and machine-validated workload
fidelity replaced inline/parallel-array benchmark workarounds. This is
correctness/workload evidence, not a speed claim. Evidence:
[CP20](evidence/cp20-reusable-algorithm-corpus-2026-08-21.md).

### CP21 - Linux reference performance baseline

State: `verified`. Parent gate: I4. Predecessor: CP20. Evidence scope: `pushed-ci`.

Outcome: D056/D057 established raw samples, metadata, dispersion, reviewed
Linux x86_64 budgets, and a separate still-open five-platform public-alpha
registry. Evidence:
[CP21](evidence/cp21-array-performance-baseline-2026-08-22.md).

### CP22 - Verified MIR range facts

State: `verified`. Parent gate: I4. Predecessor: CP21. Evidence scope: `pushed-ci`.

Outcome: verifier-reconstructable array bounds capabilities safely remove
proved checks with hostile-bytecode rejection and checked fallback. Evidence:
[CP22](evidence/cp22-mir-range-proofs-2026-08-22.md).

### CP23 - Native exact array-access ABI

State: `verified`. Parent gate: I4. Predecessor: CP22. Evidence scope: `local`.

Outcome: one versioned C17 runtime ABI replaced duplicate native handle/type/
length/bounds work; LTO was rejected as the permanent boundary. Evidence:
[CP23](evidence/cp23-native-array-access-2026-08-22.md).

### CP24 - Dense raw-scalar array storage

State: `verified`. Parent gate: I5. Predecessor: CP23. Evidence scope: `local`.

Outcome: descriptor-selected 1/2/4/8-byte dense raw storage preserved stable
identity, exact bits, bytecode identity, tracing, accounting, and VM/native
observations. Evidence:
[CP24](evidence/cp24-dense-array-storage-2026-08-22.md).

### CP25 - Exact and reserved array capacity

State: `verified`. Parent gate: I5. Predecessor: CP24. Evidence scope: `local`.

Outcome: exact known-count construction and one stride-aware geometric
growth/reserve planner preserved the D055 resource boundary. Evidence:
[CP25](evidence/cp25-array-capacity-2026-08-22.md).

### CP26 - Nested managed views

State: `verified`. Parent gate: I5. Predecessor: CP25. Evidence scope: `local`.

Outcome: activation-scoped array/record views and rooted replacement guards
improved nested traversal without granting bounds or escaping raw pointers.
Evidence: [CP26](evidence/cp26-nested-array-views-2026-08-22.md).

### CP27 - Verified integer range proofs

State: `verified`. Parent gate: I4. Predecessor: CP26. Evidence scope: `local`.

Outcome: reconstructed interval proofs permit selected non-panicking i64 MIR/
bytecode operations while analysis/resource uncertainty retains checked
semantics. Evidence: [CP27](evidence/cp27-integer-proofs-2026-08-22.md).

### CP28 - Algorithm integration boundary

State: `verified`. Parent gate: I4. Predecessor: CP27. Evidence scope: `local`.

Outcome: final algorithm/native integration, rebaseline, correctness hardening,
and sanitizer validation established the fixed boundary before product work.
The five-platform public-alpha registry and cross-language gaps remain explicit.
Evidence: [rebaseline](evidence/cp28-release-rebaseline-2026-08-22.md) and
[release hardening](evidence/cp28-release-hardening-2026-08-23.md).

## CLI0 boundary

CP29–CP40 deliver a bounded local CLI alpha, not P0. Networking, I6 tasks,
registry/dependency workflows, production LSP/test tooling, FFI, public
portable bytecode, cross compilation, desktop support, and self-hosting are
excluded. N0 and claimed platform evidence still block publication. CP28→CP29
is the explicit transition from the completed algorithm foundation to product
alpha work. CP41 changes no CLI0 source surface; it is the separate I4
performance prerequisite inserted between CP38 and CP39.

## Active sequence CP29-CP42

The verification sequence is linear with exactly one active checkpoint. CP29
through CP38 are `verified`; CP42 is `in-progress`, and CP41 plus CP39–CP40
remain `planned` behind their declared predecessors. The user's 2026-09-05
priority change makes execution order CP42, CP41, CP39, then CP40;
numeric order does not override those explicit dependencies.

### Accepted CLI0 execution route

On 2026-09-07 the user accepted the
[dependency-based route analysis](../research/cli0-route-analysis-2026-09-07.md).
The five cycles below organize work within the existing checkpoint owners;
they do not introduce another status register or new numbered checkpoints.

| Cycle | Result / exit condition |
|---|---|
| 1. Reliable baseline | Reconcile all 42 closure assignments with code, evidence and missing hosts; classify/reproduce open native CI failures; establish reusable application acceptance mechanics from the config harness. A failed shared backend or missing proof remains explicit. |
| 2. Shared CLI contracts | Design collection copy/search/removal, mutation results and Option bindings together using config and existing notes; implement each semantic change through its own decision/spec/owner/tests and migrate these consumers. |
| 3. Application families | Complete all ten external applications in dependency waves, using the portfolio contract and shared acceptance mechanics. Fix functional blockers and map each application to actual tests; performance/platform acceptance remains explicit. |
| 4. Measured performance | Complete CP42 performance and its full required application/Tier 1 acceptance on its revision, then execute CP41 with a fresh D056 baseline. Preserve separate runtime/startup/backend attribution and all existing budgets. |
| 5. Candidate audit | Verify the resulting candidate and portfolio, complete CP39 clean installed lifecycle, then CP40 with exact revision/artifact/platform evidence and N0. Publication requires separate user authorization. |

Start Linux PMU availability, required-host inventory, clean install-image
preparation and N0 evidence inventory in cycle 1. CP39 infrastructure may be
prepared before CP41; CP39 verification still requires the final artifact.
One semantic family is active at a time. Independent evidence/environment
preparation does not activate a second checkpoint.

Application implementation follows the
[family waves](cli-ergonomics-suite.md#execution-and-completion), replacing
the former numeric application order. A performance failure blocks acceptance
of affected workloads and release; investigate, repair and remeasure unchanged
inputs. Independent preparation may continue. A shared native correctness
failure blocks the affected native candidate across all applications; a passing
local run or unrelated fix cannot close it.

Use a fresh baseline to confirm the CP41 hypothesis before selecting an
optimization. Its 15%/10%/20% criteria, D057 five-platform obligation and N0
remain unchanged. Any proposed revision needs measured grounds and the normal
decision/spec protocol. No already accepted requirement is waived by this route.

## CP29 - Public release contract and closure matrix

State: `verified`. Parent gate: CLI0. Predecessor: CP28. Evidence scope:
`local`. Evidence: [CP29](evidence/cp29-release-contract-2026-08-23.md).

### Outcome

Version/help/README/reference material distinguishes the executable private
preview, the bounded CLI0 target, and the 1.0 target design, while one exact
closure matrix assigns every missing N0, A0, I0, I1, and applicable I2–I5
criterion to an owning checkpoint or gate.

### Why now / dependencies

CP28 is the stable algorithm foundation. Product work cannot make an honest
alpha claim until public metadata and docs stop mixing implemented slices with
target design. This checkpoint plans closure; it does not close those gates.

### Authority and decisions

Read D001, D008, D030, D048–D057, toolchain §§1, 2, 6, and 10, requirements,
the naming audit, product roadmap, and current evidence. Accept a release
contract decision covering `0.1.0-alpha.N`, pre-beta source compatibility,
diagnostic stability, bytecode non-compatibility, standard-library versioning,
release channels, and removal policy before changing public claims.

### In scope

- Human/JSON `mmay version`, help snapshots, installed metadata, known-
  limitations format, release channel names, and exact implemented/staged
  command inventory.
- Bytecode may honestly remain versioned `unavailable`; an implemented bounded
  standard-library slice must have an explicit version classification rather
  than being labelled wholly unavailable.
- A runnable quick start that passes fmt/check/VM run/native build/test-both;
  target-design examples remain clearly labelled and separately syntax-tested.
- A row-by-row closure matrix for N0, A0, I0, I1, I2, I3, I4, and I5 with
  current evidence scope, missing criterion, owning CP/gate, acceptance command,
  platform scope, and permitted release claim.
- Decide whether `mmay explain` becomes executable for alpha diagnostics or is
  removed from everyday help until its owner lands.
- Confirm CP18 left no managed-array compatibility window; otherwise exclude
  source compatibility from the alpha claim and assign an exact removal gate.

### Out of scope

No runtime feature, semantic change, gate verification, external identity
reservation, tag, package, installer, artifact publication, or stable 0.1.0
claim. Do not close N0 from repository rebranding evidence.

### Owners to inspect

`CMakeLists.txt` version metadata; `bootstrap/src/cli/` version/help dispatch;
focused CLI snapshot owners under `tests/c-unit/`; `README.md`, installed-doc
configuration, `docs/requirements.md`, `roadmap.md`, `roadmap.tsv`, and the
evidence index. Inspect line ceilings before choosing an implementation owner.

### Required vertical coverage

This is CLI/release metadata rather than language semantics: version/help human
and JSON schemas, relocation/no-network behavior, README extraction, installed
metadata consistency, documentation links, closure-matrix validation, and
exact command availability are required. No parser/checker/MIR/backend change
is justified by this card.

### Focused tests and acceptance commands

Cover human/JSON version snapshots, every implemented/staged help surface,
metadata/release-note mismatches, extracted README quick start, relocation,
and no-network execution. Run:

```sh
build/debug/mmay version
build/debug/mmay version --json
build/debug/mmay help
build/debug/mmay fmt tests/public-alpha/cp29-quick-start --check
build/debug/mmay check tests/public-alpha/cp29-quick-start
build/debug/mmay run tests/public-alpha/cp29-quick-start
build/debug/mmay test tests/public-alpha/cp29-quick-start --engine both
build/debug/mmay build tests/public-alpha/cp29-quick-start --output build/debug/cp29-quick-start
build/debug/cp29-quick-start
```

Then run the common validation tail.

### Evidence and completion rule

Add a CP29 evidence report with exact revision/commands and link the accepted
release decision and closure matrix. Completion requires binary metadata,
help, README, installed docs, roadmap, register, and executable evidence to
agree. `0.0.0-dev` in the binary while docs claim alpha is a failure. No
product gate changes state merely because CP29 is verified.

### Removal gates / remaining limitations

Remove or visibly stage every everyday-help command that is not executable.
Record each closure-matrix gap against CP30–CP40 or its product gate. N0 and
all external/publication blockers remain explicit.

### Handoff packet for the next agent

Pass only the accepted release decision, CP29 closure matrix/evidence, version
and help owners/tests, `roadmap.md`, `roadmap.tsv`, and the CP30 card. The next
agent should not reread CP0–CP28 instructions.

## CP30 - Dynamic strings interpolation and primitive formatting

State: `verified`. Parent gate: CLI0. Predecessor: CP29. Evidence scope:
`local`. Evidence: [CP30](evidence/cp30-dynamic-strings-2026-08-23.md).

### Outcome

Programs build bounded runtime strings from values and print more than literals
or separately formatted `f64`, with locale-independent VM/native bytes.

### Why now / dependencies

CP29 fixes the bounded claim; CP6 already supplies numeric and string
foundations. Dynamic text is the first application blocker before arguments,
streams, paths, and JSON can expose usable diagnostics and output.

### Authority and decisions

Read language interpolation/evaluation rules, standard-library text/conversion
contracts, D003, D013, D040, D044, D046, D047, and CP6 evidence. Accept a
decision enumerating supported primitive types, exact format/parse signatures,
user-defined formatting contract if any, left-to-right once-only evaluation,
locale independence, limits, allocation/resource failure, and interaction with
`println(string)`. It must not add `any`, implicit conversion graphs, general
overloads, or host `printf` semantics.

### In scope

Interpolation typing and lowering; bounded string construction; bool, every
integer width, and `f64` formatting; the complete accepted integer format/parse
family including radix/sign/padding policy; optional owned-nominal formatting
only if the decision defines exact visibility/coherence; diagnostics,
formatter behavior, and executable tour examples. D046 `formatF32` retains its
raw-binary32 boundary.

### Out of scope

No stdin/files, unrestricted reflection, locale/collation, `Display`-style
open world, implicit numeric conversion, or task-aware I/O.

### Owners to inspect

Focused interpolation expression/checker owners; string and number formatting
owners under `bootstrap/src/base/`; MIR string/I/O owners; bytecode codec,
verifier and VM string owners; native emitter/runtime string owners; formatter
expression fixtures; focused checker/MIR/VM/native/CLI tests. Do not put the
feature in a broad expression, VM, or emitter coordinator.

### Required vertical coverage

Decision/spec/diagnostics and formatter fixtures; checker typing and exact
evaluation order; typed MIR and verifier; hostile bytecode encode/decode/
verification; VM and generated C17 O0/O2; bounded managed roots/construction;
public examples; exact VM/native stdout/result/panic/resource comparison.

### Focused tests and acceptance commands

Cover all integer extrema/bases, bool, both zero signs, infinities, NaNs,
embedded NUL, Unicode, empty/nested interpolation, side-effect order, formatter
ambiguity/visibility, output limits, malformed bytecode, and every allocation
failure. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp30-formatting --check
build/debug/mmay check tests/public-alpha/cp30-formatting
build/debug/mmay run tests/public-alpha/cp30-formatting
build/debug/mmay test tests/public-alpha/cp30-formatting --engine both
build/debug/mmay build tests/public-alpha/cp30-formatting --release --output build/debug/cp30-formatting
build/debug/cp30-formatting
```

Then run the common validation tail and the applicable parser/formatter/
bytecode fuzz campaigns.

### Evidence and completion rule

Link the accepted decision, normative changes, fixture inventory, allocation
sweep, raw VM/native observations, O0/O2 and claimed Tier 1 results in CP30
evidence. Every accepted formatted type must traverse the complete path; a
host-locale or undefined-C dependency blocks verification.

### Removal gates / remaining limitations

Remove literal-only/intermediate formatting workarounds and any host formatter
authority. Full standard streams remain CP32; text/bytes operations remain
CP33. Record any deliberately deferred user-defined formatting surface.

### Handoff packet for the next agent

Pass the CP30 decision/spec, exact public signatures, focused owners/tests,
evidence, and CP31 card. Include the stable string construction and error APIs,
not unrelated compiler internals.

## CP31 - Program arguments and controlled exit

State: `verified`. Parent gate: CLI0. Predecessor: CP30. Evidence scope:
`local`. Evidence: [CP31](evidence/cp31-program-arguments-2026-08-24.md).

### Outcome

Marimay programs read arguments passed after `--` and return a caller-selected
portable status without bypassing lexical cleanup.

### Why now / dependencies

Dynamic strings from CP30 make real argument diagnostics possible; CP14 already
proves cross-package `[]string`. This boundary must be stable before stdio,
`std/cli`, native run, and installable CLI workflows.

### Authority and decisions

Read language entry/return/defer contracts, execution process outcomes,
toolchain run/build/test argument separation, D041, D049–D051, and CP14. Accept
one source contract: an allowed `main` signature or a `std/process`/`std/cli`
accessor. Define executable-name presence, host encoding/Windows quoting,
NUL rejection, count/byte limits, test injection, exit range/mapping, and
capability classification. Safe exit must run all cleanup; immediate process
termination is not the default.

### In scope

CLI `--` transport, VM execution options, native entry adapter, test injection,
typed argument representation, controlled portable status, validation, stable
diagnostics, and precedence with `Result`, panic, D041 cleanup, and I0002.

### Out of scope

No subprocess execution, signals, shell parsing/expansion, environment access,
or std/cli option grammar.

### Owners to inspect

CLI run/test/build option owners; checker entry selection; MIR/bytecode entry
metadata and verifier; VM execution options; native public entry adapter;
platform process argument conversion; focused entry/CLI/process/differential
tests. Preserve the existing tool/program `--` separation owner.

### Required vertical coverage

Decision plus language/execution/toolchain/diagnostic text; checker entry/API
typing; typed MIR/verified bytecode argument values; VM/native adapters;
test-record injection; D041 cleanup on every exit; formatter/examples where
the selected source contract changes syntax; exact cross-platform results.

### Focused tests and acceptance commands

Cover zero/many/empty/Unicode/NUL-rejected arguments, leading dashes, literal
`--`, spaces, the Windows quoting corpus, byte/count limits, invalid encoding,
invalid status values, and normal/Err/panic/defer outcomes. Run:

```sh
build/debug/mmay run tests/public-alpha/cp31-args -- alpha "two words"
build/debug/mmay test tests/public-alpha/cp31-args --engine both
build/debug/mmay build tests/public-alpha/cp31-args --release --output build/debug/cp31-args
build/debug/cp31-args alpha "two words"
```

Compare exact VM, native process, and test-isolate observations, then run the
common validation tail.

### Evidence and completion rule

CP31 evidence records argument bytes/decoding, exit classification, cleanup,
platform matrix, allocation failures, and exact VM/native/test observations.
Verification requires cleanup exactly once and no host-specific status or
quoting drift on every claimed Tier 1 host.

### Removal gates / remaining limitations

Remove the CP9 retained-but-source-invisible argument slice and any test-only
entry bypass. Subprocesses/signals remain I9; option parsing remains CP37.

### Handoff packet for the next agent

Pass the accepted entry/argument/exit contract, process adapters, focused tests,
evidence, and the CP32 card. Include exact encoding and limit constants.

## CP32 - Typed standard streams and flush

State: `verified`. Parent gate: CLI0. Predecessor: CP31. Evidence scope:
`local`. Evidence: [CP32](evidence/cp32-typed-standard-streams-2026-08-25.md).

### Outcome

Synchronous CLI programs read bounded stdin and independently write/flush
stdout and stderr through typed APIs with explicit EOF and errors.

### Why now / dependencies

CP30 supplies dynamic text and CP31 fixes process outcomes. Streams are the
minimum pipeline boundary and are required before the text/bytes, filesystem,
CLI-parser, and JSON workload cards can be exercised honestly.

### Authority and decisions

Extend D046 or accept a new decision defining exact `print`/`println`,
`eprint`/`eprintln`, read-line/read-to-limit, EOF, buffering, partial read/write,
LF/CRLF, invalid UTF-8, maximum bytes, flush, error precedence, and distinct
`io.stdin`, `io.stdout`, `io.stderr` capabilities. Runtime diagnostics remain
separate from program stderr. Record the blocking Stage 0 adapter and I6
parking removal gate.

### In scope

Physical/embedded `std/io`, capability grants, text input/output, explicit EOF,
bounded capture buffers, flush, checker ownership, MIR operations, bytecode
codec/verifier, VM sources/sinks, generated C17/native platform adapters, test
records, CLI process injection, stable typed errors, and exact stream ordering
records. Binary integration may use the CP33 `Bytes` owner when that card lands.

### Out of scope

No task-aware parking claim, networking, terminal UI, locale transcoding,
unbounded whole-input read, or diagnostic text mixed into program streams.

### Owners to inspect

`std/io`; checker I/O recognition; MIR I/O model/verifier/execution; bytecode
I/O codec/verifier/VM; runtime stdout and test-capture owners; CLI run/test
process capture; POSIX/Windows file/console adapters; focused I/O and
differential tests. Split shared capture/synchronization only behind a narrow
owner contract.

### Required vertical coverage

Decision/spec/capability/diagnostic updates; exact public signatures and
checker typing; MIR and hostile-bytecode verification; VM/native partial I/O
and flush behavior; test capture/record comparison; cleanup and allocation
failures; formatter/examples; Tier 1 console, pipe, redirect, and file capture.

### Focused tests and acceptance commands

Cover empty input, EOF before/after data, LF/CRLF, embedded NUL, invalid UTF-8,
partial host operations, interruption/retry, denied capabilities, broken pipe,
flush failure, independent stdout/stderr, no diagnostic pollution, allocation
failures, and VM/native parity. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp32-stdio --check
build/debug/mmay check tests/public-alpha/cp32-stdio
build/debug/mmay test tests/public-alpha/cp32-stdio --engine both
build/debug/mmay build tests/public-alpha/cp32-stdio --release --output build/debug/cp32-stdio
```

Process fixtures must inject exact stdin bytes and compare stdout, stderr, and
exit separately for VM and native. Run the common tail; run TSan if shared
capture/synchronization changes.

### Evidence and completion rule

CP32 evidence links decisions/specs, exact stream records, platform adapters,
partial/error fixtures, allocation sweeps, sanitizer/TSan where applicable,
and the claimed Tier 1 matrix. Host `errno`, locale spelling, or unbounded input
in a public result blocks verification.

### Removal gates / remaining limitations

Document and later remove the blocking adapter at I6 when stream operations
park only the task. Binary I/O remains coupled to CP33; networking remains I6.

### Handoff packet for the next agent

Pass the finalized `std/io` signatures/errors/capabilities, platform adapter
contracts, exact capture records, focused tests/evidence, and CP33 card.

## CP33 - UTF-8 text Bytes and bounded builders

State: `verified`. Parent gate: CLI0. Predecessor: CP32. Evidence scope:
`local`. Evidence: [CP33](evidence/cp33-text-bytes-2026-08-26.md).

### Outcome

Programs safely inspect, split, join, decode, encode, and incrementally build
bounded UTF-8 text and bytes without quadratic literal-only workarounds.

### Why now / dependencies

CP30 supplies runtime strings and CP32 supplies real streams. Text/bytes are
the data foundation for files, environment, collections, CLI parsing, and JSON.

### Authority and decisions

Read language primitive/managed identity and indexing rules, standard-library
string/collection/resource sections, D007, D013, D040, D045, D053–D055. Accept
the alpha `string` byte/rune/grapheme-view subset, `Bytes` identity/mutation,
UTF-8 conversion errors, slicing boundaries, builder limits/failures, and
iteration invalidation. String integer indexing is forbidden. Do not introduce
a permanent borrowed view before I6 lifetime/suspension constraints exist.

### In scope

Byte length, rune iteration, bounded substring/view-or-copy, contains/prefix/
suffix, split/join/trim with explicit Unicode policy; `Bytes` construction,
indexing, push/append/slice/copy; text/byte builders; UTF-8 encode/decode; binary
stdio integration; package/import behavior, managed layout/rooting, and exact
diagnostics.

### Out of scope

No regex, normalization/collation beyond the accepted subset, locale behavior,
unbounded builders, implicit bytes/string conversion, or escaping borrowed
views.

### Owners to inspect

String and managed-array checker owners; type interning/spelling; MIR managed
string/array operations; bytecode descriptors/codec/verifier/VM; runtime
managed string/array storage/mutation; native emitter managed owners; `std/io`
binary integration; focused text/bytes/builder/parser/property tests.

### Required vertical coverage

Decision/language/stdlib/execution/diagnostics; parser/formatter for public
forms; checker and package boundaries; typed MIR and verifier; portable
bytecode and malformed cases; VM/native managed storage and roots; binary
stdio; randomized UTF-8 round trips, exact panic/errors, examples, allocation
sweeps, and differential execution.

### Focused tests and acceptance commands

Cover ASCII, multibyte scalars, combining marks, invalid UTF-8, empty/maximum
inputs, boundary slices, alias mutation, iterator invalidation, builder growth/
overflow, nested roots, malformed bytecode, randomized round trips, and every
allocation failure. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp33-text-bytes --check
build/debug/mmay check tests/public-alpha/cp33-text-bytes
build/debug/mmay run tests/public-alpha/cp33-text-bytes
build/debug/mmay test tests/public-alpha/cp33-text-bytes --engine both
build/debug/mmay build tests/public-alpha/cp33-text-bytes --release --output build/debug/cp33-text-bytes
build/debug/cp33-text-bytes
```

Then run the common tail and applicable parser/bytecode fuzz campaigns.

### Evidence and completion rule

CP33 evidence records exact public API, UTF-8 policy, layout/rooting,
random/property/fuzz results, allocation failures, and VM/native observations
on claimed Tier 1 hosts. A builder sequence that bypasses the declared input/
output limit or any divergent UTF-8 result blocks verification.

### Removal gates / remaining limitations

Remove literal-only concatenation and unsafe host-string/byte bypasses. Record
any temporary copied view; borrowed suspension-safe views remain later work.

### Handoff packet for the next agent

Pass the text/Bytes/builder decision and APIs, managed descriptors, focused
tests/evidence, binary-stdio contract, and CP34 card.

## CP34 - Paths and filesystem slice

State: `verified`. Parent gate: CLI0. Predecessor: CP33. Evidence scope:
`local`. Evidence: [CP34](evidence/cp34-paths-filesystem-2026-08-29.md).

### Outcome

CLI programs use portable typed paths to boundedly read files, atomically write
files, inspect metadata, and deterministically enumerate directories under
explicit capabilities.

### Why now / dependencies

CP32/CP33 provide bounded text and bytes; D041 provides cleanup. Filesystem
behavior is the first real local-resource workload and is required by JSON and
installable CLI examples.

### Authority and decisions

Read standard-library path/fs/resource contracts, execution capabilities,
toolchain sandbox flags, diagnostics, D006, D041, and platform tier rules.
Accept exact alpha `std/path`/`std/fs` signatures: representation, relative/
absolute behavior, lexical normalization, symlink policy, open/read/write/
close, bounded whole-input APIs, atomic writes, metadata, deterministic
directory order, stable errors, and operation-appropriate canonicalization
before capability matching.

### In scope

`--allow-read`/`--allow-write`; VM capability matcher; native POSIX/Windows
adapters; path construction/normalization; bounded `readText`/`readBytes`;
atomic `writeText`/`writeBytes`; open/close, metadata, directory entries,
temporary test filesystem, cleanup/error precedence, diagnostics, and real
replacement of the CP4 signature-only `std/fs` boundary where applicable.

### Out of scope

No watcher, mmap, permission management, network-filesystem guarantee,
subprocess, signals, ambient unrestricted filesystem access, or automatic
exposure of toolchain-internal path helpers.

### Owners to inspect

`std/path`, `std/fs`; CLI capability options; checker external/standard API
recognition; MIR/bytecode/VM capability and I/O owners; runtime resource
cleanup; `runtime/platform/posix` and `runtime/platform/windows` file/directory
owners; focused path/fs/capability/CLI/differential tests.

### Required vertical coverage

Decisions/specs/diagnostics and examples; exact public APIs and checker typing;
MIR/bytecode capability operands and verifier; VM virtual/test filesystem and
native adapters; path/capability normalization; partial I/O, atomic
publication, explicit cleanup, allocation failures, formatter fixtures, and
exact engine/platform result comparison.

### Focused tests and acceptance commands

Cover Unicode/spaces/long paths, relative roots, `..`, symlinks, missing/
denied/not-file/not-directory/read-only targets, short operations,
interruption, replacement failure, input limits, invalid UTF-8, directory
order, close failure, D041 cleanup panic precedence, capability escapes, and
allocation failures. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp34-files --check
build/debug/mmay check tests/public-alpha/cp34-files
build/debug/mmay test tests/public-alpha/cp34-files --engine both
build/debug/mmay run tests/public-alpha/cp34-files --allow-read tests/public-alpha/cp34-files/fixtures
build/debug/mmay build tests/public-alpha/cp34-files --release --output build/debug/cp34-files
```

Compare temporary-filesystem records across VM/native and run the common tail.

### Evidence and completion rule

CP34 evidence links the decisions/specs, path and capability cases, temporary
filesystem records, resource/allocation cleanup, sanitizer results, and claimed
Tier 1 adapter matrix. A denied operation that reaches the host or an opened
resource that survives any exit blocks verification.

### Removal gates / remaining limitations

Replace the CP4 signature-only filesystem fixture with real conformance or keep
it explicitly isolated as a negative fixture. Blocking file adapters acquire
the I6 parking removal gate; watchers/mmap remain later.

### Handoff packet for the next agent

Pass exact `std/path`/`std/fs` APIs, capability matching and error contracts,
platform adapters, focused tests/evidence, and CP35 card.

## CP35 - Environment and process context

State: `verified`. Parent gate: CLI0. Predecessor: CP34. Evidence scope:
`local`. Evidence: [CP35](evidence/cp35-environment-process-context-2026-08-29.md).

### Outcome

Programs read cwd, executable identity, and only explicitly allowed environment
variables with portable encoding, redaction, and error behavior.

### Why now / dependencies

CP31 fixes program process boundaries, CP33 supplies text/bytes, and CP34 fixes
portable paths. Environment/process context completes local CLI context before
collection-backed option parsing and install lifecycle.

### Authority and decisions

Read standard-library env/process contracts, execution capability rules,
toolchain option/cache rules, D030, D055, and platform policies. Accept exact
lookup, missing-versus-invalid-encoding, optional enumeration as a separate
grant, cwd/executable guarantees, Windows case folding, test injection,
redacted diagnostics, and cache/nondeterminism behavior. Hidden environment
overrides of runtime semantics remain forbidden.

### In scope

`--allow-env NAME`; a separately named enumeration grant only if accepted;
`std/env` and process-context APIs; cwd/executable path; VM/native capability
transport; deterministic test environment; Windows case rules; redaction;
allocation/error behavior and cache nondeterminism guards.

### Out of scope

No subprocess execution, signals, dynamic libraries, ambient enumeration,
secret logging, or environment-driven compiler/runtime policy.

### Owners to inspect

CLI capability/argument owners; `std/env` and process-context packages;
VM execution options; native public entry adapter; POSIX/Windows process/path
owners; diagnostic redaction; build-cache inputs; focused env/process/
capability/relocation tests.

### Required vertical coverage

Decision/stdlib/execution/toolchain/diagnostics; checker and package behavior;
typed MIR/bytecode capability access if compiler-known; VM injected context;
native platform adapters; deterministic tests; relocation/cwd paths; redaction,
allocation failure, and exact VM/native result comparison.

### Focused tests and acceptance commands

Cover missing/empty/Unicode/invalid encoding, duplicates/case variants, denied
lookup/enumeration, changed cwd, relocated executable, spaces/Unicode,
deterministic injection, no leakage, cache independence, and allocation
failures. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp35-environment --check
build/debug/mmay check tests/public-alpha/cp35-environment
build/debug/mmay test tests/public-alpha/cp35-environment --engine both
build/debug/mmay run tests/public-alpha/cp35-environment --allow-env MMAY_ALPHA_TEST
```

Then run the common validation tail on every claimed platform family.

### Evidence and completion rule

CP35 evidence records the accepted grants, exact encodings/errors/redaction,
relocation/cwd outcomes, deterministic VM/native tests, cache probes,
allocation failures, and Tier 1 results. Undeclared environment affecting a
program result or semantic build cache blocks verification.

### Removal gates / remaining limitations

Remove hidden ambient reads and test-only native bypasses. Subprocesses and
signals remain I9; environment enumeration remains absent unless separately
accepted and granted.

### Handoff packet for the next agent

Pass the env/process APIs and capability flags, platform behavior matrix,
focused tests/evidence, and CP36 card.

## CP36 - Map Set and iterator core

State: `verified`. Parent gate: CLI0. Predecessor: CP35. Evidence scope:
`local`. Evidence: [CP36](evidence/cp36-map-set-iterators-2026-08-30.md).

### Outcome

CLI programs use first-party ordered `Map` and `Set` collections and
production iterators instead of parallel-array workarounds.

### Why now / dependencies

CP33 supplies text/bytes keys and CP17 established mutation/generation
precedent. Environment and files are already executable, so ordered maps/sets
now unlock reusable CLI parsing and typed JSON.

### Authority and decisions

Read language generics/interfaces/identity, standard-library collections,
execution managed layout, D003, D007, D013, D043, D053–D055. Accept complete
Map/Set operations, equality/hash eligibility and laws, insertion order,
update/delete/reinsert position, mutation generation, iterator invalidation,
growth errors, non-observable capacity, and deterministic test seeding. Hash
randomization must not change iteration order.

### In scope

Generic type checking and structural constraints; exact free/dot calls;
primitive/string/accepted nominal keys; managed Map/Set layouts; public
operations as one closed surface; insertion-ordered iteration; mutation and
errors; precise tracing, package boundaries, diagnostics, formatter/examples,
typed MIR, bytecode/VM, and native execution.

### Out of scope

No concurrent collections, weak references, custom allocators, arbitrary
user hashing without an accepted coherence contract, or capacity observation.

### Owners to inspect

Generic/interface/resolution checker owners; type/layout tables; MIR managed
collection model/verifier/execution; bytecode tables/opcodes/codec/verifier/VM;
runtime managed storage/tracing/mutation; native descriptors/emitter; focused
Map/Set/iterator/property/GC/package/differential tests.

### Required vertical coverage

Decision/language/stdlib/execution/diagnostics; parser/formatter/examples;
checker constraints/imports and compiler-service spelling; typed MIR/oracle;
portable hostile bytecode; VM/native layout/tracing/mutation; collect-at-every-
allocation and randomized heap graphs; equality/hash/order properties;
malformed artifacts, allocation sweeps, and exact engine results.

### Focused tests and acceptance commands

Cover supported key families, collisions, update/remove/reinsert, empty/large
collections, mutation during iteration, equality/hash laws, insertion order,
nested values/cycles, package boundaries, malformed bytecode, randomized heap
graphs, collect-every-allocation, and every failure index. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp36-map-set --check
build/debug/mmay check tests/public-alpha/cp36-map-set
build/debug/mmay run tests/public-alpha/cp36-map-set
build/debug/mmay test tests/public-alpha/cp36-map-set --engine both
build/debug/mmay build tests/public-alpha/cp36-map-set --release --output build/debug/cp36-map-set
build/debug/cp36-map-set
```

Then run the common tail and applicable bytecode/heap fuzz/property campaigns.

### Evidence and completion rule

CP36 evidence links accepted decisions, exact API inventory, law/order
properties, managed-root graphs, allocation/fuzz/sanitizer results, and
VM/native/Tier 1 observations. Any unstable iteration order, hash/equality-law
violation, or lost managed root blocks verification.

### Removal gates / remaining limitations

Remove parallel-array lookup/counter/set workarounds in the alpha corpus.
Concurrent collections remain outside CLI0; D007/D013 product-gate workload
evidence remains separately visible.

### Handoff packet for the next agent

Pass the complete Map/Set/iterator public contracts, layout and law-test
owners, evidence, and CP37 card. Do not pass broad managed-runtime internals.

## CP37 - First-party CLI parser

State: `verified`. Parent gate: CLI0. Predecessor: CP36. Evidence scope:
`local`. Evidence: [CP37](evidence/cp37-cli-parser-2026-08-30.md).

### Outcome

Applications declaratively parse bounded flags, options, and positionals,
render deterministic help, and return typed usage errors through `std/cli`.

### Why now / dependencies

CP31 provides exact raw arguments, CP33 provides builders, and CP36 provides
ordered collections. The parser can therefore be an ordinary first-party
library rather than host-C or compiler-specific bypass logic.

### Authority and decisions

Read D003/D043, language records/enums/generics, standard-library error and
collection contracts, toolchain argument separation, and CP31. Accept the
bounded `std/cli` model: long/short names, boolean flags, required/optional
values, repetition, positionals, `--`, subcommands only if justified, help
order/width, duplicate declarations, unknown/missing options, Unicode, limits,
and recommended usage exit. The library performs no shell expansion/quoting
and uses no `any` or unrestricted reflection.

### In scope

The complete accepted `std/cli` API and implementation using public records,
enums, strings/builders, arrays/maps/sets where possible; typed errors;
deterministic help; parser limits; examples; package/docs tests; only the
already accepted intrinsic needed to obtain CP31 arguments.

### Out of scope

No compiler syntax, shell parser, environment configuration, interactive
prompting, plugin system, hidden host-C parser, or unaccepted subcommand model.

### Owners to inspect

`std/cli` package layout; standard-library build/embed ownership; CP31 argument
transport; string/builder/Map/Set APIs; formatter/doc examples; focused
standard-package, CLI process, property, allocation, and differential tests.
Compiler owners should change only if an existing public contract cannot
express the library.

### Required vertical coverage

Decision/stdlib/toolchain/docs; public package checker/import behavior;
ordinary typed MIR/bytecode/VM/native execution through shipped APIs;
deterministic help/error bytes; property-generated argument cases; package
limits and allocation failures; no compiler-specific semantic fast path.

### Focused tests and acceptance commands

Cover empty arguments, accepted clusters, `--name=value`, separated values,
negative numeric values, repeated/unknown/missing options, `--`, Unicode,
help order/width, limits/OOM, and generated cases. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp37-cli-parser --check
build/debug/mmay check tests/public-alpha/cp37-cli-parser
build/debug/mmay test tests/public-alpha/cp37-cli-parser --engine both
build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --help
build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --count 3 input.txt
```

Compare exact VM/native help and errors, then run the common tail.

### Evidence and completion rule

CP37 evidence records the accepted API, public implementation dependency
surface, help/error snapshots, property/allocation tests, and VM/native/Tier 1
observations. Verification requires the parser package itself to use only
shipped alpha contracts with no host-C parsing bypass.

### Removal gates / remaining limitations

Remove benchmark/example-local option parsers. Any deferred subcommand or
advanced completion feature remains absent and documented, not stubbed.

### Handoff packet for the next agent

Pass the `std/cli` API/implementation, exact help and error contracts, focused
tests/evidence, and CP38 card.

## CP38 - Bounded typed JSON

State: `verified`. Parent gate: CLI0. Predecessor: CP37. Evidence scope:
`pushed-ci`. Evidence:
[CP38](evidence/cp38-bounded-typed-json-2026-09-01.md).

### Outcome

The alpha ships a real data utility that boundedly decodes typed JSON into
records/enums/collections, transforms it, and deterministically encodes JSON.

### Why now / dependencies

CP32–CP36 provide streams/files, text/bytes, and managed collections; CP37
provides a real application boundary. JSON now proves a nontrivial data-
transform workload before installation and final release audit.

### Authority and decisions

Read standard-library JSON/security/resource contracts, D006/D007/D013,
language visibility/derives/attributes, and test-strategy parser/fuzz rules.
Accept a bounded typed codec model: explicit codec values or compiler derives,
field naming, unknown/missing/duplicate fields, enum representation, number
ranges, Unicode escapes, deterministic output order, nesting/byte limits, and
error spans. Private fields are inaccessible through unrestricted reflection.
If derives add syntax/attributes, use the complete semantic-change protocol.

### In scope

Tokenizer/parser/encoder packages; typed codec construction/generation;
records/enums/arrays/maps; text/bytes and stream/file integration; exact errors
and limits; deterministic encoding; one shipped JSON transformer; security
guidance; fuzz, allocation, and VM/native coverage.

### Out of scope

No schema registry, arbitrary/private reflection, unbounded DOM, streaming
network bodies, host JSON library authority, or dynamic `any` tree.

### Owners to inspect

`std/json`; source lexer only if new derive syntax is accepted; checker derive/
visibility owners; standard-library embedding; text/Bytes/Map owners; MIR/
bytecode/native only for any approved compiler-known codec metadata; focused
JSON tokenizer/codec/derive/fuzz/CLI/differential tests.

### Required vertical coverage

Decision/stdlib/security/diagnostics; parser/formatter/checker/generated
metadata if syntax changes; typed codec privacy and package boundaries;
ordinary MIR/verified bytecode/VM/native execution; streams/files; deterministic
encoding/errors; malicious limits, fuzzing, allocation failures, and runnable
public transformer documentation.

### Focused tests and acceptance commands

Cover the applicable standards corpus, invalid/truncated data, duplicate keys,
nesting/size/number limits, Unicode/surrogate cases, deterministic output,
unknown-field policies, private fields, enum variants, round trips, fuzz seeds,
and every allocation failure. Run:

```sh
build/debug/mmay fmt tests/public-alpha/cp38-json --check
build/debug/mmay check tests/public-alpha/cp38-json
build/debug/mmay test tests/public-alpha/cp38-json --engine both
build/debug/mmay run tests/public-alpha/cp38-json --allow-read tests/public-alpha/cp38-json -- tests/public-alpha/cp38-json/input.json
build/debug/mmay build tests/public-alpha/cp38-json --release --allow-read tests/public-alpha/cp38-json --output build/debug/cp38-json
build/debug/cp38-json tests/public-alpha/cp38-json/input.json
```

Then run the common tail and sustained JSON/parser/bytecode fuzz campaigns.

### Evidence and completion rule

CP38 evidence links the codec decision, exact API/representation/privacy/limit
rules, fuzz statistics/corpora, allocation sweeps, transformer outputs, and
VM/native/Tier 1 results. An unresolved crash, hang, OOM amplification,
reflection privacy breach, or host-dependent output blocks verification. The
linked CP38 report satisfies this rule for the pushed-CI scope recorded above.

### Removal gates / remaining limitations

Remove inline/demo-only JSON parsing and any host library bypass. Network-body
streaming remains I6/I9; schema/registry work remains outside CLI0.

### Handoff packet for the next agent

Pass the JSON codec contract and package, generated metadata if any, fuzz/
allocation evidence, public transformer, and CP41 card. CP39 waits for the
separate native-code pressure checkpoint.

## CP39 - Native run init doctor and installable artifacts

State: `planned`. Parent gate: CLI0. Predecessor: CP41.

### Outcome

A fresh supported host installs Marimay, initializes a local project,
diagnoses native prerequisites, and runs the same program in VM or native mode
without per-project compiler/linker/SDK selection.

### Why now / dependencies

CP29–CP38 provide the complete bounded application surface; CP10/CP13 provide
native build/runtime artifacts; CP41 closes the measured x86_64 code-pressure
gap. Final installation and native-run evidence must use the shipped workloads,
standard packages and required native performance repair. Clean-host inventory,
packaging and lifecycle infrastructure may be prepared under the accepted route
before CP41; this does not verify CP39 early.

### Authority and decisions

Read D001, D030, D050, D052, toolchain install/run/build/init contracts, N0,
platform matrix, and artifact/provenance requirements. Amend the alpha
distribution decision with install layout, relocatability, upgrade/uninstall,
host-toolchain ownership, `mmay init`, human/JSON `mmay doctor`, native-run
cache/arguments/capabilities, artifact names, installed std/offline docs,
source/per-host archives, checksums, provenance, and SBOM. N0 must be ready for
closure but publication remains CP40 and user-authorized.

### In scope

`mmay run --engine native`; local-only `mmay init`; `mmay doctor`; installed
version-matched debug/release runtimes, headers, standard library, and offline
docs; relocatable source and per-claimed-host artifacts; checksums/provenance/
SBOM generation; clean install/relocation/upgrade/uninstall; exact native
argument/capability/process behavior.

### Out of scope

No dependency resolver/registry, package publication, cross compilation,
automatic platform SDK redistribution beyond accepted D030 policy, release tag,
signing/upload, or former `ma`/`ruda` aliases.

### Owners to inspect

CLI run/init/doctor dispatch and help; native artifact/runtime selection and
build cache; CMake install rules and packaging configuration; POSIX/Windows
process/toolchain discovery; standard-library/doc installation; artifact
manifest/provenance generation; focused install/doctor/native-run/relocation/
upgrade/uninstall tests and clean-image CI.

### Required vertical coverage

Distribution/toolchain decision and docs; human/JSON schemas; native run from
the canonical typed MIR and cache; init templates validated by fmt/check;
toolchain preflight; installed runtime/std/docs identity; direct argv/no
network; atomic install/update; clean-image lifecycle; exact VM/native
arguments/capabilities/streams/files/JSON behavior; allocation and platform
failure coverage.

### Focused tests and acceptance commands

Cover each claimed clean host image, prefix relocation, bare PATH, init through
fmt/check/run/test/build, offline repeat, missing/stale/wrong compiler/runtime,
read-only trees, interrupted install/build, upgrade/uninstall preserving user
projects, command collision, Unicode/spaces, and absence of former aliases.
Run in the installed environment:

```sh
mmay version --json
mmay doctor --json
mmay init alpha-smoke
cd alpha-smoke
mmay fmt --check
mmay check
mmay run --engine vm . -- --help
mmay run --engine native . -- --help
mmay test --engine both
mmay build --release --output dist/alpha-smoke
```

Then run the common tail and archive reproducibility/relocation checks.

### Evidence and completion rule

CP39 evidence records every artifact hash/manifest, installed prefix/runtime/
compiler path, version/doctor schema, lifecycle command/exit, offline proof,
and claimed-host CI run at the exact revision. A configured-but-unexecuted host
workflow is not evidence; every claimed artifact must pass the clean lifecycle.

### Removal gates / remaining limitations

Remove build-tree fallback dependencies from installed artifacts and all
temporary native-run/test harness bypasses. Cross compilation and package
management remain later. Do not publish the prepared artifacts.

### Handoff packet for the next agent

Pass the exact CP39 artifact manifest/hashes, install and doctor contracts,
clean-image evidence per host, known prerequisites, N0 status, and CP40 card.

## CP40 - CLI alpha integration and publication-ready audit

State: `planned`. Parent gate: CLI0. Predecessor: CP39.

### Outcome

One versioned, installable, independently auditable candidate proves the
bounded local CLI claim and enumerates every exclusion without publishing it.

### Why now / dependencies

CP40 integrates CP29–CP39 only after each is verified and audits the separate
CP41 I4 evidence as a release prerequisite. It is the CLI0 closure audit, not
a semantic implementation checkpoint or P0.

### Authority and decisions

Use accepted decisions/specs, N0, the product roadmap release ladder, CP29
closure matrix, all CP29–CP39 evidence, and CP41. This card may change release
metadata/docs only. Any semantic/API defect returns to its owning checkpoint or
receives a newly planned atomic checkpoint. Publication additionally requires
N0 verified, a repository license, controlled public repository/docs/security
contacts/signing identity, command collision matrix, and explicit user approval.

### In scope

`LICENSE`, `SECURITY.md`, contributor/build guide, changelog/release notes,
support scope, compatibility/migration policy, known limitations, artifact
checksums/SBOM/provenance/exact revision; no placeholder published identities;
complete claim-to-requirement evidence audit; source/archive/install
reproducibility; three public workloads: Unicode line filter with broken-pipe
behavior, option-driven multi-file count/search utility with typed errors/exits,
and bounded typed JSON transformer with file/stdin modes.

### Out of scope

No new semantics, hidden final-integration fixes, networking/I6, dependency
registry, production LSP/test tooling, portable public bytecode, FFI, cross
compilation, self-hosting, beta/1.0 compatibility, publication, tag, signing,
upload, namespace reservation, or stable 0.1.0 claim.

### Owners to inspect

Only focused integration/release fixtures; version/help/doctor/artifact owners;
public examples; release/security/support docs; CI/release-manifest generation;
roadmap/register/evidence index; CP29 closure matrix, CP30–CP39 evidence, and
the CP41 before/after performance and equivalence report.
Do not turn an existing broad final-preview test into a new semantic owner.

### Required vertical coverage

Map every CLI0 claim and CP29–CP41-assigned requirement to direct evidence;
run fmt/check/all tests, VM/native run, native builds, installed/build-tree and
debug/release comparisons on all claimed hosts; compare stdout/stderr/exits,
diagnostics, file outputs, resources, panics/spans; run sanitizers, LSan,
applicable TSan/Valgrind, allocation sweeps, architecture/memory/generated/docs,
parser/formatter/bytecode/JSON fuzz thresholds, clean checkout/install, and
reproducible source archive.

### Focused tests and acceptance commands

Run the complete installed command matrix:

```sh
mmay fmt --check
mmay check --all-targets
mmay test --engine both
mmay run --engine vm examples/public/line-filter -- --help
mmay run --engine native examples/public/line-filter -- --help
mmay build examples/public/file-count --release --output dist/file-count
mmay build examples/public/json-transform --release --output dist/json-transform
mmay version --json
mmay doctor --json
```

Exercise line-filter stdin/stdout/stderr/broken pipe, file utility multi-input
errors/exits, and JSON file/stdin modes. Run the common tail plus all CP40
quality/reproducibility checks on every claimed host.

### Evidence and completion rule

The final evidence report links every direct CI run for the exact source SHA,
artifact manifest/hashes, release notes/limitations, claim audit, fuzz/
sanitizer/allocation results, and exact workload observations. CP40 is verified
only when N0 and every release dependency required for the claimed hosts is
closed. Verification makes artifacts publication-ready; actual publication
requires a separate explicit user instruction.

### Removal gates / remaining limitations

All CP29–CP39 temporary adapters/workarounds due by CLI0 must be removed or the
card remains unverified. The explicit post-alpha exclusions stay in release
notes and the product roadmap; they are absent, not stubbed behind public names.

### Handoff packet for the next agent

Pass the final claim/evidence audit, exact source and artifact hashes, known
limitations, N0/publication authorization status, and the roadmap's post-CLI0
I6/E0 requirements. CP41 is already a verified prerequisite at this point;
the focused I6 planning audit assigns CP42+.

## CP41 - x86_64 native code-generation pressure

State: `planned`. Parent gate: I4. Predecessor: CP42. Evidence scope: `pushed-ci`.
Trigger evidence:
[CP41 x86_64 code-generation trigger](evidence/cp41-x86-codegen-trigger-2026-08-24.md).

### Outcome

Portable C17 emitted from the same typed MIR no longer amplifies managed
record, array, enum, status, and cleanup state into the demonstrated x86_64
spill/branch penalty. Safe bounds and overflow checks, managed identity and
rooting, panic spans, allocation failures, and VM/native observations remain
exactly unchanged. Completion uses platform-local D056 budgets; it does not
promise universal parity with Go or Rust.

### Why now / dependencies

The exact CP30 compiler and unchanged eight-workload corpus exposed a selective
Linux x86_64 slowdown: stack, queue, heap-sort, and min-heap lose materially
more relative to the same Go/Rust participants than on macOS arm64, while the
nine focused array probes scale normally. Same-source cross-target assembly
then showed approximately one-third larger x86_64 hot functions and many more
stack-addressed operations. This is a separate backend-efficiency problem, not
a reason to rewrite the benchmark algorithms or weaken VM semantics. CP41 is
scheduled after CP38 and before CP39–CP40: it does not displace the semantic
cards, avoids a CLI0/I4 dependency cycle, and repairs the backend before
installation evidence and before I6 adds more long-lived task and cleanup
state.

### Authority and decisions

Use D013, D040, D045, D053, D055–D063, the C17 AOT and VM/native-equivalence
contracts in `docs/spec/execution.md`, the compiler architecture, and the D056
measurement protocol. D056 makes Go/Rust ratios exploratory only; formal
regression and improvement gates are per-platform Marimay distributions. D059
forbids making LTO the semantic/runtime boundary, and D063 views remain
non-escaping and safepoint-aware. No source semantic or public API change is
authorized. A native ABI version, observable failure/resource-order change, or
new representation rule requires an accepted decision and corresponding spec
and test changes before implementation.

### In scope

Capture a clean before-baseline on a dedicated Linux x86_64 runner and a
supplemental macOS arm64 report; retain emitted C, compiler optimization
remarks, symbolized assembly, and stable hardware-counter observations for the
affected workloads. Attribute live ranges and spills across managed handles,
record/array activation views, immediate enums, status values, cleanup state,
and panic/error edges. Apply only verified-MIR/native-C17 changes such as
shorter temporary live ranges, reuse of validated views, narrower materialized
state, cold-path outlining, or nonescaping internal scalar-enum transport when
the existing ABI and exact fallback remain intact. Cover the nine focused
probes and all eight canonical algorithms without changing their work counts,
checksums, or language implementations.

### Out of scope

No unchecked source operation, removed bounds/overflow/identity/generation
check, changed panic or allocation behavior, benchmark-specific opcode,
algorithm/corpus rewrite, mandatory `-march=native`, target-specific assembly
or C++, LTO-only correctness boundary, cross-language ranking budget, public
ABI promise, I6 scheduler work, or release claim enters CP41.

### Owners to inspect

Inspect `docs/implementation/compiler-architecture.md`, typed-MIR range and
verification owners, `bootstrap/src/native/c_emitter_emit.c`,
`c_emitter_managed.c`, `c_emitter_range.c`,
`c_emitter_array_mutation.c`, their narrow private headers, generated runtime
headers, and the focused native-emitter/MIR/VM/differential tests. Inspect the
sibling `algorithm-benchmarks` protocol, probe, corpus, budget, and CI owners.
Do not add optimization policy to the broad emitter coordinator or duplicate
source semantics in a lowering/backend owner.

### Required vertical coverage

Pin typed-MIR and bytecode shapes before and after; prove the VM oracle and
observable native results are unchanged at O0/O2 under Clang/GCC and the
applicable MSVC matrix. Cover call-free and allocating/safepoint paths,
reference and scalar layouts, alias replacement, nested views, growth and
generation invalidation, Option success/empty paths, cleanup/panic edges, and
every reachable deterministic allocation-failure index. Add structural x86_64
and arm64 code-generation fixtures plus exact-checksum performance runs.
Sanitizer, LSan, architecture, memory-policy, generated-artifact, and full
VM/native differential gates remain mandatory.

### Focused tests and acceptance commands

Before editing and after the candidate revision, run from the sibling corpus:

```sh
MMAY=/absolute/path/to/release/mmay ./benchmarks/probes.py \
  --samples 11 --warmups 3 \
  --output benchmarks/results/cp41-probes-linux-x86_64.json
MMAY=/absolute/path/to/release/mmay ./benchmarks/run.py \
  --languages marimay,go,rust --samples 11 --warmups 3 \
  --output benchmarks/results/cp41-algorithms-linux-x86_64.md
./benchmarks/validate_results.py \
  benchmarks/results/cp41-probes-linux-x86_64.json \
  benchmarks/results/cp41-algorithms-linux-x86_64.json
```

Compile the same retained generated C and runtime with the same C compiler and
flags for arm64 and x86_64; archive optimization remarks, symbol sizes,
disassembly, and a scripted count of stack-addressed operations. On an eligible
Linux runner, collect at least eleven balanced observations of cycles,
instructions, branches, branch misses, cache references, and cache misses for
stack, queue, heap-sort, and min-heap. The report records both these logical
counter names and the exact physical `perf` event map. Generalized perf events
are preferred. If a virtual PMU exposes a generalized cache pair but returns
unavailable or nonpositive samples, one explicit architecture-specific paired
cache-access/cache-miss profile is permitted only after a positive preflight;
before and after must use the identical profile and cross-profile comparisons
are rejected. Run the focused MIR/native-emitter tests, exact workload
checksums, full VM/native differential, and the common tail.

### Evidence and completion rule

The 2026-08-24 trigger run is diagnostic only: its host load and Go queue noise
make it ineligible as a new D056 baseline. CP41 first records a D056-compatible
before-run and then an identical-protocol after-run at exact pushed revisions
on a dedicated Linux x86_64 runner. All nine probes and eight algorithms must
remain within reviewed platform budgets. Stack and queue must each reduce the
Marimay median and retired instructions by more than the greater of 15%, the
before noise fraction, and the after noise fraction. Heap-sort and min-heap
must not regress, and at least one must improve by more than the greater of
10% and both run noise fractions. Retained same-compiler disassembly must show
at least 20% fewer stack-addressed operations across the stack and queue hot
symbols without increasing their arm64 count. Exact outputs, stderr, exits,
panic spans, allocation/resource behavior, and VM/native results must match.

The report publishes every raw sample/order event, metadata/hash, perf command
and counter, the logical-to-physical event profile, generated-C/disassembly
artifact hash, and an explanatory before/after table. A cache profile identifies
its measured level and access class; it must not be presented as a different
cache level or combined with another profile. Go/Rust and macOS ratios remain
labeled exploratory and cannot pass or fail CP41. Hardware counters are
required for the selected completion runner; an environment that cannot expose
stable counters is supplemental only. State becomes `verified` only after the
exact candidate passes pushed CI and the complete evidence is linked here and
in the evidence index.

### Removal gates / remaining limitations

Remove diagnostic emitter dumps, duplicated fast/slow lowering, target-name
conditionals, benchmark-specific special cases, and any optimization whose
fallback cannot preserve the existing portable ABI and failure order. Keep the
general checked path and D063 invalidation rules. CP41 does not close the D057
five-platform public-alpha registry or prove parity with another language.

### Handoff packet for the next agent

Pass the exact before/after revisions and artifact hashes, raw D056 reports,
perf and cross-target assembly bundle, retained correctness/sanitizer results,
remaining target-specific limitations, and the CP39 install/native-run card.
CP39 and CP40 must retain this evidence in the CLI0 artifact and claim audits;
the first atomic I6 card remains CP43 or later.

## CP42 - Notes-driven CLI ergonomics

State: `in-progress`. Parent gate: CLI0. Predecessor: CP38. Evidence scope: `local`.

Initial evidence: [typed JSON first slice](evidence/cp42-typed-json-first-slice-2026-09-05.md).
Follow-up: [notes migration and startup convergence](evidence/cp42-notes-startup-2026-09-06.md).
Follow-up: [structured JSON error paths](evidence/cp42-json-error-paths-2026-09-06.md).
Follow-up: [intrinsic metadata and external-portfolio start](evidence/cp42-json-intrinsics-2026-09-06.md).
Follow-up: [local execution policy and config resource boundaries](evidence/cp42-local-execution-policy-2026-09-06.md).
Follow-up: [config VM dispatch and native proof performance](evidence/cp42-config-performance-2026-09-06.md).
Follow-up: [invocation-scoped test bytecode preparation](evidence/cp42-test-preparation-2026-09-06.md).
Follow-up: [one native build per test invocation](evidence/cp42-native-test-build-2026-09-06.md).
Follow-up: [bounded JSON string-run parsing](evidence/cp42-json-string-runs-2026-09-06.md).
Follow-up: [void match arms and lexical cleanup](evidence/cp42-void-match-2026-09-07.md).
Follow-up: [ordinary test-file helpers](evidence/cp42-test-helpers-2026-09-07.md).
Follow-up: [exact mapError context](evidence/cp42-map-error-context-2026-09-07.md).
Follow-up: [test panic source ranges](evidence/cp42-test-locations-2026-09-07.md).
Follow-up: [compound-loop-binding recovery](evidence/cp42-for-recovery-2026-09-07.md).
Route baseline: [closure assignments and CI audit](evidence/cli0-baseline-audit-2026-09-07.md).
Shared preparation: [external application acceptance driver](evidence/cli0-shared-acceptance-2026-09-07.md).
Shared-contract baseline: [notes aliases, explicit mutation results and output failures](evidence/cli0-notes-contract-baseline-2026-09-08.md).
Shared-contract implementation: [scoped Option conditions and consumer migration](evidence/cp42-option-binding-2026-09-08.md).
Array copy/removal and functional Notes migration: [D097 local evidence](evidence/cp42-array-edit-2026-09-08.md).
Function-value/capture foundation: [D098 local evidence](evidence/cp42-function-values-2026-09-08.md).
Predicate search and Notes integration: [D099 local evidence](evidence/cp42-array-search-2026-09-08.md).
Ordinary Result mappers and root lifetime: [D100 local evidence](evidence/cp42-mapper-values-2026-09-08.md).
Streaming/file-line wave and native record views: [D101 local evidence](evidence/cp42-file-lines-2026-09-08.md).
Diagnostic removal gate: named void enum payload bindings must report T0002
instead of the observed silent internal status 70; the ordinary named-void
restriction remains unchanged.
Common-layout removal gate: implement uninhabited enum payload transport before
accepting mapper result `never`; the current I0001 is an explicit implementation
gap, not a normative source restriction.
Callback removal gate: deliver the nonfatal escaping-mutable-capture warning with
severity, escape classification and positive/negative fixtures before full §6.3
conformance or CP42 callback acceptance; legal escaping closures remain legal.
Required acceptance portfolio: [ten independent external CLI applications](cli-ergonomics-suite.md).

### Outcome

The notes utility describes its data and business rules without manually
repeating record fields in JSON readers/writers, copying/shifting arrays in
application loops, or disguising inconsistent mutation behavior. Strict
validation and VM/native equivalence are preserved.
The same ergonomics must hold across the user's ten-application external
portfolio, with combined collection/I/O/error coverage and recorded findings.

### Why now / dependencies

On 2026-09-05 the user explicitly accepted and prioritized the Go-comparison
ergonomics proposal. This extends the CLI0 acceptance bar and moves CP41 behind
this card; it does not reopen networking, self-hosting, or other I6 work.
The architecture gate must remain green, and existing formatter/match repairs
remain separately owned correctness prerequisites rather than hidden features.
On 2026-09-06 the user expanded pre-alpha acceptance beyond notes to ten
independent utility repositories outside the language checkout. The linked
portfolio contract is mandatory for CP42 and CLI0; it is not optional sample work.

### Mandatory reading before implementation

Read the compiler architecture, D082/D086, standard-library JSON/collections,
D006 result handling, D053 array mutation, language Option/matching and identity
contracts, plus the focused checker, source-package and differential owners.
Each later semantic change needs an accepted decision and normative text before
implementation; this card is not itself a language specification.

### Scope / exclusions

Complete the following semantic obligations through the accepted route:

1. Strict automatic typed JSON, explicit limits, useful field/index errors,
   source diagnostics for unsupported types, and notes migration.
2. Standard shallow-copy, position-search, and stable removal operations for
   collections with explicit aliasing, bounds and iterator semantics.
3. Mutation return contracts that remove needless discards without silently
   ignoring expected failure, capacity limits, or useful return information.
4. Convenient Option binding in conditions/loops, with grammar, formatter,
   scope, exhaustive matching, and both-engine tests.
5. A consistent notes mutation API, documentation and acceptance tests.

Use config and existing notes to design items 2–5 as one shared-contract cycle,
with separately reviewable decisions and implementations. Build and evaluate
the remaining portfolio by dependency family, using its findings to validate
the shared API. All ten utilities and their combined acceptance evidence remain
required. The JSON, allocation, resource and platform tail remains mandatory.

The user's 2026-09-06 expanded algorithm run adds a required collection
hot-path optimization to this card. Finish verification of the current JSON
repair, then diagnose and repair `frequency-map`, `unique-set`, and
`rune-frequency` before accepting the affected performance workloads. Under
the accepted route, independent preparation may proceed while this remains
open. The existing
sibling `algorithm-benchmarks` workloads retain their inputs, operation counts,
checksums, reusable APIs, and 3 warmups plus 11 measured samples. The observed
Marimay/Go medians were 81.59/14.88 ms, 58.32/17.53 ms, and 43.75/6.69 ms;
these are a local investigation trigger, not reviewed Tier 1 budgets or proof
of a particular cause. The raw report SHA-256 is
`51617a8a1f93b33dc621682a1e19a18df341f712a4eaa93c88df90eb0be778f2`.
Partial follow-up: [native collection results and verifier worklists](evidence/cp42-collection-results-2026-09-06.md).
Its local 19.5–27.4% reductions do not close this item; residual collection
costs and the discovered rune-Eq/parser-recovery defects remain obligations.
The next [hash/descriptor boundary follow-up](evidence/cp42-collection-hash-descriptors-2026-09-06.md)
adds smaller paired gains without changing SipHash, inputs, order or limits;
it likewise does not close the obligation or the separate Linux CI failures.
The [scalar lookup reuse follow-up](evidence/cp42-collection-lookup-reuse-2026-09-07.md)
removes repeated Map hash/probe work with paired counter improvements and
enabled/disabled-memo regression coverage. Set and the full performance item
remain open; the report records the fixed per-object memory tradeoff.
The [checked Set insertion follow-up](evidence/cp42-set-insertion-2026-09-07.md)
removes duplicate receiver validation without exposing resolved storage. Its
paired Set gains and expanded duplicate/error-boundary tests are additional
partial evidence, not closure of the remaining performance or CI obligations.
The [rune equality repair](evidence/cp42-rune-equality-2026-09-07.md) addresses
the recorded mismatch between primitive equality and collection-key equality.
It does not close the remaining hot-path or parser-recovery obligations.
The [private inline validation follow-up](evidence/cp42-inline-validation-2026-09-07.md)
removes small internal call boundaries while retaining all handle checks and
the separately shipped C17 runtime. Three paired series provide additional
partial evidence; LTO was a diagnostic experiment, not a selected dependency.
The [compact collection storage follow-up](evidence/cp42-compact-collections-2026-09-07.md)
reduces raw Map/Set reservation without weakening cell/reference semantics.
It records rejected copy-boundary regressions, selected paired measurements
and layout-specific memory/failure evidence; the full performance item stays open.
The [independent cursor follow-up](evidence/cp42-collection-cursors-2026-09-07.md)
replaces quadratic source-loop ordinal scans with forward traversal under D089.
Sparse/interleaved iteration, legacy wire semantics, mutation checks and the
applicable local correctness tail are covered. The three unchanged counting
workloads show no stable speedup from this traversal fix. Further repairs must
follow a system-level review of table representation, public operation paths,
input construction and generated code, rather than an open-ended sequence of
small hot-path edits. The [system-level collection review](evidence/cp42-collection-system-review-2026-09-07.md)
records phase/runtime/probe/allocation evidence and the comparative prototype
requirements. The [selected grouped index and amortized rebuilding](evidence/cp42-grouped-collections-2026-09-07.md)
implement D090/D091 after that review. Three local paired series improve the
original counting workloads; a broad matrix also exposes and fixes repeated
full-storage rebuilding after sparse deletion. Actual reservation increases,
small-table tradeoffs, exact budgets, failure/GC checks, VM/native and installed
config-app acceptance are recorded. Battery-powered local timing and emulated
Linux evidence do not close Tier 1 or product acceptance.
Separate input preparation from collection execution; profile hashing/probing,
managed-handle checks, and Option/Result construction before choosing repairs.
Acceptance requires paired same-workload improvement, unchanged insertion
order, collision resistance, mutation/iterator and failure semantics, plus
focused allocation, sanitizer and VM/native regression coverage. Feed the
resulting artifact into CP41; neither this entry nor a findings log closes it.

The first JSON slice supports public-field acyclic records, concrete generic
records, arrays, int/bool/string. Explicit codecs remain available. No unbounded
DOM/reflection, private-field discovery, host parser, new task runtime, release
tag, or publication is authorized by this card.

### Owners

JSON expression/types/decode/encode/MIR checker owners; existing collection
and control-flow owners behind their narrow APIs; source JSON package; focused
checker/allocation/formatter tests; `tests/public-alpha/typed-json`; the separate
notes repository. Backends consume the same verified typed MIR.

### Required tests and acceptance commands

Run focused positive/negative, all-index allocation, sanitizer and resource
limits tests, format idempotence, strict-C native debug/release and VM/native
differential tests. The notes application must retain all business and hostile-
input tests after migration. Register public and focused tests in CTest. Run
the common documentation/generated/architecture/memory-policy gates and the
complete supported Tier 1 matrices before verification.

### Completion / removal gates

The initial local JSON evidence does not close this card. Complete all five
language items and the ten-utility external portfolio, link exact-revision
evidence through the index and ROADMAP, and
retain honest unsupported-type and installation limitations. Generic intrinsic
signature/completion metadata must not be represented by fake external
functions. Remove any temporary diagnostic probes before committing. Hand off
the actual resulting artifact and measurements to CP41, then CP39 and CP40.

D087 removes the accidental embedder instruction fuel from ordinary local
commands. Resource-boundary acceptance must still reconcile the VM call-depth
ceiling and native host-stack limits, including classification and cleanup;
instruction-policy repair alone does not evidence complete resource parity.
<!-- checkpoint-register:end -->
