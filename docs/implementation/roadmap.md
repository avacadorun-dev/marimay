# Ruda roadmap: design baseline to production 1.0

Status: normative delivery gates.  
Allowed gate states: `not-started`, `in-progress`, `verified`, `blocked`.

A gate is `verified` only with linked command output and artifacts satisfying every criterion. Feature presence is insufficient. Later gates may begin for risk reduction, but no release can skip an earlier dependency.

## Current status

| Gate | State | Evidence |
|---|---|---|
| D0 — research and design baseline | `verified` | [2026-08-10 D0 verification](evidence/d0-2026-08-10.md) |
| N0 — naming and distribution identity | `in-progress` | [open external identity audit and historical name collision](../design/naming-audit.md) |
| I0 — repository/bootstrap foundation | `in-progress` | [iteration 1: build/CLI](evidence/i0-iteration-1-2026-08-10.md); [iteration 2: source ownership/positions](evidence/i0-iteration-2-2026-08-10.md); [iteration 3: portability/fault injection/CI](evidence/i0-iteration-3-2026-08-10.md) |
| I1 — lossless frontend and formatter | `in-progress` | [lexer iteration 1: Unicode/literals/lossless tokens](evidence/i1-lexer-iteration-1-2026-08-10.md); [parser iteration 2: immutable green tree/structural recovery](evidence/i1-parser-iteration-2-2026-08-10.md); [frontend iteration 3: generated kinds/Pratt/AST projection](evidence/i1-frontend-iteration-3-2026-08-10.md); [formatter library iteration 4: D026 document model/round trip](evidence/i1-formatter-library-iteration-4-2026-08-10.md); [formatter CLI iteration 5: D027/D028/D031 targets, manifests, and import ordering](evidence/i1-formatter-cli-iteration-5-2026-08-10.md); [frontend iteration 6: syntax-kind matrix, D032 strings, incremental reuse, and fuzz harnesses](evidence/i1-frontend-iteration-6-2026-08-10.md); [grammar registry iteration 7: generated productions and independent coverage axis](evidence/i1-grammar-registry-iteration-7-2026-08-10.md); [grammar completion iteration 8: 117/117 baseline productions](evidence/i1-grammar-completion-iteration-8-2026-08-11.md); [keyword-boundary iteration 9: initial D038 partition](evidence/i1-keyword-boundary-iteration-9-2026-08-12.md); [keyword-boundary iteration 10: exhaustive contextual-collision audit](evidence/i1-keyword-boundary-iteration-10-2026-08-12.md) |
| I2 — typed core and MIR oracle | `in-progress` | [iteration 1: type identity, verified MIR oracle, and internal source-to-MIR slice](evidence/i2-typed-core-iteration-1-2026-08-11.md); [iteration 2: indexed interning and bounded verifier dataflow](evidence/i2-foundation-hardening-iteration-2-2026-08-11.md); [iteration 3: control-flow lowering and definite assignment](evidence/i2-control-flow-iteration-3-2026-08-11.md); [iteration 4: staged signatures and loop control](evidence/i2-signatures-loop-control-iteration-4-2026-08-11.md); [iteration 5: C-style `for` and compound assignment](evidence/i2-c-for-iteration-5-2026-08-11.md); [iteration 6: typed direct calls and program oracle](evidence/i2-calls-iteration-6-2026-08-11.md); [iteration 7: deterministic multi-file package checking](evidence/i2-multifile-package-iteration-7-2026-08-11.md); [iteration 8: bounded top-level constants](evidence/i2-constants-iteration-8-2026-08-11.md); [iteration 9: logical short-circuiting and composable expression CFG](evidence/i2-short-circuit-iteration-9-2026-08-11.md); [iteration 10: deterministic non-generic type aliases](evidence/i2-type-aliases-iteration-10-2026-08-11.md); [iteration 11: portable `i64` bitwise operators](evidence/i2-bitwise-i64-iteration-11-2026-08-11.md); [iteration 12: D035 checked `i64` shifts](evidence/i2-checked-shifts-iteration-12-2026-08-11.md); [iteration 13: deterministic nominal record declarations](evidence/i2-record-declarations-iteration-13-2026-08-11.md); [iteration 14: public-type visibility](evidence/i2-public-visibility-iteration-14-2026-08-11.md); [iteration 15: algebraic enum declarations](evidence/i2-enum-declarations-iteration-15-2026-08-11.md); [iteration 16: executable managed-record MIR oracle](evidence/i2-managed-record-oracle-iteration-16-2026-08-11.md); [iteration 17: algebraic-enum construction oracle](evidence/i2-enum-construction-oracle-iteration-17-2026-08-11.md); [iteration 18: checked enum-match oracle](evidence/i2-enum-match-oracle-iteration-18-2026-08-11.md); [iteration 19: guarded enum match](evidence/i2-guarded-enum-match-iteration-19-2026-08-11.md); [iteration 20: enum OR patterns](evidence/i2-enum-or-patterns-iteration-20-2026-08-11.md); [iteration 21: finite-boolean match](evidence/i2-boolean-match-iteration-21-2026-08-11.md); [iteration 22: concrete match witnesses](evidence/i2-match-witnesses-iteration-22-2026-08-11.md); [iteration 23: i64 literal match](evidence/i2-i64-literal-match-iteration-23-2026-08-11.md); [iteration 24: block match arms and `yield`](evidence/i2-match-block-arms-iteration-24-2026-08-11.md); [iteration 25: return-only match arms](evidence/i2-match-return-arms-iteration-25-2026-08-11.md); [iteration 26: all-`never` match propagation](evidence/i2-all-never-match-iteration-26-2026-08-11.md); [iteration 27: `never` conditions and guards](evidence/i2-never-conditions-guards-iteration-27-2026-08-11.md); [iteration 28: `never` loop conditions](evidence/i2-never-loop-conditions-iteration-28-2026-08-11.md); [iteration 29: `never` C-style `for` steps](evidence/i2-never-loop-steps-iteration-29-2026-08-12.md); [iteration 30: string literals and explicit-panic MIR oracle](evidence/i2-string-panic-oracle-iteration-30-2026-08-12.md); [iteration 31: declared-`never` terminating-call edge](evidence/i2-declared-never-calls-iteration-31-2026-08-12.md); [iteration 32: exact string literal patterns and MIR equality](evidence/i2-string-literal-match-iteration-32-2026-08-12.md); [iteration 33: flat nominal record patterns](evidence/i2-record-patterns-iteration-33-2026-08-12.md); [iteration 34: stable nominal record identity](evidence/i2-record-identity-iteration-34-2026-08-12.md) |
| I3 — bytecode VM vertical slice | `in-progress` | [iteration 1: internal bytecode registry, codec, verifier, and disassembler foundation](evidence/i3-bytecode-foundation-iteration-1-2026-08-12.md); [iteration 2: scalar MIR lowering, VM dispatch, and MIR/VM differential execution](evidence/i3-scalar-vm-iteration-2-2026-08-13.md); [iteration 3: portable `i64` bitwise and D035 checked shifts](evidence/i3-bitwise-shifts-iteration-3-2026-08-13.md); [iteration 4: verified direct-call programs and explicit VM frames](evidence/i3-direct-calls-iteration-4-2026-08-13.md) |
| I4 — C17 AOT and engine equivalence | `not-started` | none |
| I5 — managed data and standard core | `not-started` | none |
| I6 — direct-style tasks and I/O | `not-started` | none |
| E0 — pre-self-host ergonomics pilot | `not-started` | none |
| I7 — packages, cache, and supply chain | `not-started` | none |
| I8 — complete first-party developer loop | `not-started` | none |
| I9 — FFI, capabilities, and platform layer | `not-started` | none |
| I10 — self-hosting and bootstrap reproducibility | `not-started` | none |
| P0 — alpha ecosystem and reference workloads | `not-started` | none |
| P1 — beta compatibility/security hardening | `not-started` | none |
| P2 — production reference service and operations | `not-started` | none |
| P3 — 1.0 release candidate | `not-started` | none |
| P4 — production 1.0 | `not-started` | none |

## Immediate execution priority: V0 engine-equivalence spine

I2, I3, and I4 are intentionally overlapped after the minimally useful pure
subset exists. The next execution release blocker is the first complete V0
spine:

```text
source -> typed verified MIR -> encoded verified bytecode -> VM result
       -> portable generated C17 -> native result -> exact comparison
```

V0 is not a new release or a shortcut around the I2–I4 gates. It is a
sequencing constraint: I2 does not need every planned language surface before
I3/I4 begin, and broad new I2 features cannot displace this spine unless they
close a documented semantic blocker. The initial V0 corpus covers literals,
direct calls, branches/loops, checked arithmetic including D042, and one
record/enum construction path with D040 source-order effects. Later I2 slices
must extend the same differential path rather than accumulate indefinitely in
the MIR oracle alone.

## D0 — Research and design baseline

Purpose: freeze enough semantics to implement vertical slices without inventing a different language in each subsystem.

Required:

- evidence matrix covers Go/Rust strengths and pain points using primary sources;
- design constitution and accepted/provisional/rejected decisions exist;
- source, execution, and toolchain normative baselines exist;
- at least three realistic examples pass a human readability review;
- every keyword and public construct appears consistently in grammar, formatter rules, examples, and tests plan;
- open high-risk decisions have prototype gates rather than vague TODOs;
- an independent consistency audit finds no unresolved normative contradiction;
- implementation brief and test strategy cover every production gate.

Evidence command before implementation exists:

```sh
./tools/check-doc-links
./tools/check-spec-terms
./tools/check-example-syntax
```

These scripts may initially be host tools but become `ruda doc --check` and parser tests later.

Verification evidence: [D0 design-baseline verification, 2026-08-10](evidence/d0-2026-08-10.md).

## N0 — Naming and distribution identity

Purpose: obtain legally and operationally defensible external distribution identities for the accepted `Ruda`/`ruda` repository identity. D001 is accepted; that acceptance is not a trademark, domain, namespace, package-manager, or PATH clearance claim.

Deliver:

- the versioned [naming audit](../design/naming-audit.md) covering name/confusables, trademarks in initial jurisdictions, domains/handles, package/registry organization, command/PATH/package-manager collisions, artifact suffixes/MIME/editor identities, and migration cost;
- direct dated evidence for confusingly similar Ruda candidates and preserved historical evidence for the separate “Malang Compiler” that triggered the pre-release rename;
- reservation/control evidence for the chosen organization, domains, registry/module namespace, documentation/security contact, and signing identity;
- Tier 1 clean-image command/install/upgrade/uninstall collision matrix for `ruda` and alternatives;
- repository migration evidence proving that current authority/spec/test/generated/build surfaces consistently use `Ruda`, `ruda`, and `.ruda`, with former names confined to explicit history;
- a follow-up accepted decision for controlled public organization, registry/module namespace, signing identity, and residual legal/search risk.

Gate:

- legal/search/package/domain/command matrix has no missing row and explicitly distinguishes confirmed facts from inference;
- the chosen public identity and namespaces are controlled, not merely observed as available;
- residual external identity risk and mitigation are accepted in the follow-up naming decision before compatibility promises;
- no public/indexed preview or alpha, registry/package namespace implementation, package publication, or release signing identity precedes verification.

Dependencies: I7 is blocked beyond replaceable internal prototypes; E0 may run only as private invitation work under an explicit working codename; P0 publication is blocked.

## I0 — Repository and C17 bootstrap foundation

Deliver:

- documented CMake debug/release/sanitizer presets;
- portable base library: allocation, arenas, vectors, maps, UTF-8, strings, hashing, paths, process, diagnostics;
- source manager and test fixture harness;
- CLI dispatcher with `ruda --version`/`ruda version` and task-oriented `ruda help`;
- Tier 1 CI skeleton for Linux x86_64/arm64, macOS arm64/x86_64, and Windows x86_64;
- reproducible generated-table workflow with outputs committed;
- zero network access during configure/build/test.

Gate:

- all C unit tests pass under Clang/GCC/MSVC where applicable;
- ASan+UBSan clean on Linux/macOS, Windows memory tooling configured;
- failure injection covers allocator and file I/O paths;
- clean checkout builds from documented prerequisites;
- CLI/diagnostic JSON snapshots are stable.

## I1 — Lossless frontend and canonical formatter

Deliver:

- complete lexer and lossless syntax tree for the baseline grammar;
- recovery-capable parser and AST projection;
- comments/doc comments/interpolated strings/Unicode/numeric literal handling;
- `ruda fmt`, `--check`, `--diff`, and stdin mode;
- editor-friendly incremental source overlays;
- generated syntax reference.

Gate:

- every grammar production has positive, negative, recovery, and formatter fixtures;
- `fmt(fmt(x)) == fmt(x)` across corpus and fuzz generation;
- parse/format/parse preserves semantic AST and comments;
- formatter never modifies an unrecoverably invalid file;
- repository and examples are canonically formatted;
- parser/formatter fuzz for an accepted CPU-time campaign with no crash, hang, OOM amplification, or semantic mismatch.

## I2 — Typed core and MIR oracle

Deliver:

- package/name resolution, visibility, imports, and constant evaluation;
- primitives, bindings, functions, records, enums, interfaces, generics, closures;
- eligible first-parameter functions and dot-call equivalence without `impl`;
- `Option`, `Result`, postfix `?`, `defer`, control flow;
- exhaustive pattern matching with useful diagnostics;
- type/effect metadata and typed MIR;
- unoptimized MIR interpreter for the pure core;
- minimum overlay-based compiler service with diagnostics, definition, symbol search, and dot-call-complete completion;
- `ruda check`.

Gate:

- compile-pass/fail corpus covers every type rule and rejected omission;
- missing-pattern witnesses and redundant arms are correct under nested generics;
- exact structural interface conformance is deterministic across files/import order;
- task/FFI restricted-type escape checker has synthetic fixtures even before runtime use;
- MIR verifier rejects generated malformed IR;
- randomized well-typed pure programs agree with a small reference evaluator;
- completion fixtures enumerate the exact visible first-parameter dot-call set, show the equivalent free function, and never disagree with type checking;
- every public README language fence is parser-backed here and becomes a compile-pass/fail doc fixture as soon as its imports are available;
- incremental rebuild invalidation tests cover public/private/effect changes.

Current internal removal gates before I2 verification:

- publish adversarial generic/package-scale time and memory measurements for
  the deterministic indexed type interner;
- replace the current deterministic multi-file flat exact-name index with the
  complete package/import/visibility/overload index and incremental invalidation
  model required by this gate;
- remove the checker's remaining partial source-coverage boundary by
  implementing every baseline construct above, preserving an explicit
  incomplete status until each construct has conformance tests;
- extend D035 shifts to every integer width and require VM/native differential
  coverage; no backend may inherit signed-shift behavior from host C;
- extend D042 division/remainder to every integer width and require
  truncation/sign/zero/`MIN / -1`/`MIN % -1` VM/native differential coverage;
- implement D041 defer typing/lowering and cleanup outcome tests before any
  resource API or task runtime can claim verified cleanup behavior;
- make the D006 `readText(...).mapError(...) ?` documentation pair
  checker-backed as soon as generic `Result` and `std/fs` imports exist;
- extend the executable local nominal-record slice with qualified imported
  construction, cross-package field visibility, derived equality, and
  nested/refutable/imported record patterns. Iterations 16, 33, and 34
  provide checked local/alias
  construction, completeness, projection, mutation, typed MIR operations, and
  bounded managed-reference alias execution plus exact nominal, field-complete
  flat binding/wildcard record destructuring with guards and projections, plus
  exact same-nominal-type `identity.same` lowering over stable opaque handles;
  the
  execution-owned oracle arena
  is not a tracing/generational GC, production object layout, optimizer escape
  proof, or D007 allocation-performance evidence;
- extend the executable algebraic-enum slice with generic/prelude constructors,
  constant construction/matching, imported/package-qualified resolution and
  visibility, nested patterns, string and other literal domains, record/tuple
  patterns, proven divergence, and remaining control-flow `never` paths.
  Iterations 17–33
  provide same-package direct/alias construction,
  dynamically checked tag/payload MIR operations, simple exhaustive enum
  patterns, exact arm types, guarded arms with false-edge continuation,
  guard-aware coverage, top-level OR patterns with exact shared bindings,
  finite-boolean and top-level `i64` literal matching, concrete bounded missing
  witnesses for the current pattern subset, explicit-yield block arms with
  fallthrough analysis, individual return-only arms alongside value-producing
  arms, all-return bottom propagation without dummy MIR values, bottom-valued
  `if` conditions and selected guards, bottom-valued `while`/C-style `for`
  initializers, conditions, and assignment-only steps, exact immutable string
  literals, a typed explicit-source-panic terminator with stable oracle
  observation, declared-`never` calls represented by a verified terminating
  edge after left-to-right argument evaluation, and exact decoded-byte string
  literal patterns with a typed equality operation and bounded witnesses; they
  do not claim a production runtime tag/payload/string
  layout, VM/native
  representation, allocation strategy, nested/generic witness coverage, or
  hardened serialized-MIR dominance proof;
- keep `ruda check` unavailable until the source service performs the complete
  authoritative I2 pipeline rather than exposing a partial checker;
- extend bounded MIR-verifier stress and malformed-CFG generation beyond the
  current long-chain/fault-injection fixtures before accepting untrusted
  generated MIR at the gate.

## I3 — Typed bytecode VM vertical slice

Deliver:

- versioned opcode table, encoder, decoder, verifier, disassembler;
- `.rudab` section format and integrity checks;
- VM execution for the typed pure core;
- managed runtime object descriptors sufficient for records/enums/strings/closures;
- VM source stack traces and deterministic limits;
- `ruda run` defaulting to VM and `ruda exec` for verified bundles;
- VM-backed `ruda test` for core language tests.

Start with V0 as soon as its minimal I2 MIR subset is stable; do not wait for
all I2 generics, interfaces, closures, `?`, or `defer` work before decoding and
executing the first typed bytecode corpus.

Gate:

- every bytecode instruction has verifier and execution tests;
- corrupt/truncated/adversarial bundles cannot partially execute or corrupt memory;
- MIR interpreter and VM agree across fixtures and generated programs;
- untrusted bundle limits are enforced before allocation amplification;
- VM startup/cold-run baseline is published;
- bytecode corpus fuzzing is sanitizer clean.

## I4 — C17 AOT and engine equivalence

Deliver:

- portable C emitter with explicit evaluation temporaries and safe arithmetic helpers;
- native runtime linkage, D030 toolchain discovery/provisioning, object/build cache;
- `ruda build`, `--release`, `--emit c`, native `ruda run`;
- deterministic VM/native test record/replay;
- `ruda test --engine native|both`;
- source-oriented native stack traces.

Start the V0 portable-C emitter alongside the first I3 opcode subset. The first
required proof is a narrow end-to-end equality result, not a feature-complete
backend arriving only after the whole I2 surface.

Gate:

- all deterministic core tests agree across MIR oracle, VM, and native;
- generated C compiles at debug/release optimization with supported Clang/GCC/MSVC;
- generated identifiers pass the EXEC-020 adversarial matrix for every Ruda
  namespace without entering C keyword, implementation-reserved, standard
  library, runtime-helper, truncation, Unicode, or case-folded collision space;
- generated programs are ASan/UBSan clean;
- evaluation-order, overflow, NaN, bounds, enum-layout, interface, closure, and panic corpora agree;
- divergence minimizer produces a checked regression fixture;
- an official Tier 1 installation builds a native hello/service fixture without user-selected `CC`, linker, SDK path, or project configuration; missing prerequisites fail at preflight with one actionable diagnostic and machine-readable `ruda doctor` result;
- clean/incremental compile and binary-size baselines are published.

## I5 — Managed data and standard core

Deliver:

- precise non-moving per-isolate GC and shadow roots;
- arrays, maps with insertion order, sets, bytes, strings/runes, iterators;
- record identity, mutable-field semantics, derives, equality/hashing;
- error formatting and source traces;
- `std/io`, path basics, conversion, collections, and deterministic test capabilities;
- escape analysis prototype for stack/scalar replacement without semantic change;
- shape-shared generic code generation prototype.

Decision gates:

- D007: allocation/identity ergonomics and performance validated on JSON encode/decode DTO graphs, immutable UI-style trees, request/response models, and collection-heavy transformations, with heap allocations/object count/bytes/GC time compared against explicit alternatives;
- D013: shape sharing compared with boxing and monomorphization for compile time, size, and throughput.

Gate:

- collect-at-every-allocation and randomized heap graph tests pass both engines;
- cycles, closures, interfaces, enums, and suspended-frame roots survive correctly;
- no user finalizer dependency;
- hash/equality laws and ordered-map behavior are property tested;
- allocation/GC pause/throughput budgets are set from published measurements.

## I6 — Direct-style tasks, scheduler, and I/O

Deliver:

- `maySuspend` inference and causal diagnostics;
- VM frame parking and C17 resumable state-machine ABI;
- `scope`, affine `Task<T>`, `spawn`, `wait`, cancellation, task tree;
- safepoint fairness and deterministic scheduler;
- lexical `defer` across suspension/cancellation;
- timers, bounded channels, platform pollers, blocking pool;
- D029 structured first-completion race and readiness selection over tasks/channels/timers/cancellation-aware I/O;
- TCP/DNS/files and minimal HTTP client/server in direct style;
- per-isolate worker execution and send/copy/transfer prototypes;
- task dumps and tracing.

Decision gates:

- D010: all ten execution-spec suspension prototypes pass;
- D012: isolate architecture meets service and CPU-parallel workload targets.
- D029: race/select prototype passes fan-out, hedged-call, timeout/channel-loop, simultaneous-ready, closed-channel, cancellation-registration, cleanup, and panic cases before API spelling freezes.

Gate:

- `return`, `?`, panic, timeout, cancellation, nested scopes, and cleanup agree across engines;
- no test can leak a task or channel endpoint silently;
- carrier thread stays available under parked network/file operations;
- blocking-pool saturation applies backpressure and shuts down cleanly;
- deterministic task trace replay matches VM/native;
- race losers are cancelled/joined, selection losers are unregistered exactly once, and deterministic simultaneous readiness agrees across VM/native;
- one million parked tasks meets the published memory goal on reference hardware;
- cancellation latency/fairness/load benchmarks are published.

## E0 — Pre-self-host ergonomics pilot

Purpose: put working I4–I6 semantics in external hands before the compiler rewrite makes language mistakes expensive to change. I7–I9 risk-reduction work may proceed, but I10 cannot begin until this gate is verified and every semantic blocker is resolved through the normal decision/spec/test protocol.

Deliver:

- an installable preview with `fmt`, `check`, VM run/test, native build, managed data, direct I/O, structured concurrency, and the minimum I2 editor service;
- guided external tasks covering a JSON/DTO command-line tool, immutable tree transformation, concurrent HTTP fan-out with race/select and deadlines, and a small package boundary;
- instrumentation/reporting for record allocations, GC work, explicit `mapError` sites per domain boundary, dot-call completion discovery, native-toolchain setup steps, and task completion time;
- anonymized issue transcripts and a decision-impact report rather than preference-only survey totals.
- a dot-call import-form task using named type-only, named function, and
  namespace imports, with the same value passed to an interface parameter in
  every case; record both correctness and whether completion makes the owning
  functions discoverable without prior free-function knowledge.

Gate:

- at least five external pilot developers complete the defined tasks on at least two Tier 1 host families;
- D007 workloads establish accepted allocation/object-count/GC budgets or D007 is amended before 1.0 semantics freeze;
- exact error mapping is measured on at least three multi-layer boundaries; if repeated `mapError` materially impairs task completion/readability, D006 is revisited before I10 rather than papered over with implicit conversion;
- dot-call tasks are completable from editor discovery without prior free-function knowledge, and completion results match the checker exactly;
- race/select covers the service coordination tasks without detached work or hand-built cancellation plumbing;
- an ordinary host install reaches native `ruda build` without manual compiler/linker/SDK configuration beyond any single unavoidable platform-owned installation action;
- all semantic findings are accepted, rejected with evidence, or fixed in decisions/spec/tests; no unresolved language-ergonomics blocker enters self-hosting.

## I7 — Packages, cache, and supply chain

Dependency: N0 is `verified`; public module/registry namespaces cannot be built around provisional D001 identifiers.

Deliver:

- semantic validation of the I1 `ruda.mod`/`ruda.work` syntax, plus canonical `ruda.lock` parsing/formatting;
- semantic-version resolver with clear conflict paths;
- immutable content-addressed artifact cache and offline builds;
- secure archive extraction and checksum/provenance verification;
- `ruda init/add/remove/update/list/why/outdated/vendor/audit`;
- registry protocol draft, local test registry, yanking/advisory model;
- capability reachability in audit output.

Gate:

- identical lock and graph across platforms/input order;
- build/test proven network-free after fetch;
- path traversal, symlink, case collision, bomb, signature, downgrade, and corruption suites pass;
- dependency code cannot execute during install/build configuration;
- cache corruption is detected and repaired without trusting partial results;
- reproducible vendored and non-vendored builds agree.

## I8 — Complete first-party developer loop

Deliver:

- production `ruda test`, filters/watch/cache/isolation/output;
- doc tests, integration tests, expectations, property tests;
- coverage-guided `ruda fuzz` with minimization and corpus persistence;
- `ruda doc`, `ruda vet`, `ruda fix`, `ruda bench`;
- production LSP transport, indexing, cancellation, compatibility, and editor integration over the I2 compiler-service/completion owner;
- stable JSON diagnostics and test event protocol;
- help/documentation that answers common commands without external lookup.

Gate:

- a fresh user can initialize, format, check, test, run, build, document, and add a dependency from the CLI alone;
- `ruda test` has no external framework/config dependency;
- test ordering/isolation/cache invalidation are deterministic;
- doc examples compile and public APIs meet documentation policy;
- built-in fuzzing finds seeded defects in parser/verifier/runtime exercises;
- end-to-end CLI tests cover Unicode/spaces/read-only/interruption/corrupt cache.

## I9 — FFI, capabilities, and platform layer

Deliver:

- documented C ABI types, handles, callbacks, and `unsafe` rules;
- `@blocking` bridge with bounded marshalling;
- capability enforcement for VM and standard native APIs;
- production platform adapters for files, process, signals, sockets, TLS, secure random;
- platform tier tests, installer/uninstaller, crash reporting;
- unsafe/FFI audit inventory and fuzz harnesses.

Gate:

- no managed raw pointer crosses suspension or foreign retention untracked;
- callback effect diagnostics prove non-suspending paths;
- denied capabilities fail identically across engines;
- FFI ABI tests pass against C fixtures on every Tier 1 target;
- TLS/HTTP interoperability suite and security configuration review pass;
- graceful signal shutdown leaves no tasks/resources.

## I10 — Self-hosting and bootstrap reproducibility

Dependency: E0 is `verified`; self-hosting does not begin merely because I9 implementation is complete.

Deliver:

- self-hosted compiler/formatter/test tooling in Ruda;
- Stage 0 C builds Stage 1, Stage 1 builds Stage 2;
- versioned bootstrap subset and internal compiler libraries;
- normalized artifact comparison and provenance report;
- retained maintained C17 bootstrap path;
- cross-platform bootstrap from source release archive.

Gate:

- Stage 1/Stage 2 pass the same conformance and differential suites;
- normalized Stage 1/Stage 2 outputs agree or every nondeterministic byte is understood and removed from trust comparison;
- no undeclared bootstrap binary/network dependency;
- at least two independent Tier 1 environments reproduce the chain;
- self-hosted and C formatter outputs are identical over the corpus during transition.

## P0 — Alpha ecosystem and reference workloads

Dependency: N0 is `verified`; P0 is the first public alpha and cannot publish an externally uncleared identity.

Deliver:

- language tour, package authoring guide, FFI guide, operations guide;
- CLI tool, JSON transformer, static site tool, and concurrent HTTP service examples;
- database driver/package through public APIs;
- registry staging service and package publication workflow;
- initial compatibility/conformance kit for third-party packages.

Gate:

- external pilot developers complete defined tasks and findings are recorded;
- readability hypotheses for `fn`/`scope`, match, first-parameter dot calls, and direct suspension are evaluated;
- compiler crash-free rate and diagnostic task-completion measurements meet alpha targets;
- at least three nontrivial packages use only public APIs.

## P1 — Beta compatibility and security hardening

Deliver:

- source/bytecode/package compatibility policy and automated old-version corpus;
- formal unsafe/FFI threat model and bytecode/package security review;
- sustained fuzzing for lexer/parser/formatter/resolver/MIR/verifier/VM/GC/protocols;
- reproducible release signing, SBOM, provenance, advisory response;
- fixed Tier 1/Tier 2 platform policy;
- release performance budgets with regression automation.

Gate:

- no open critical/high security defects;
- beta toolchain builds and runs the compatibility corpus;
- fuzz campaigns meet published core-hours with no unresolved crash/miscompile;
- all sanitizer/platform/differential/bootstrap gates continuously green;
- language surface frozen except release-blocking corrections.

## P2 — Production reference service and operations

Deliver and operate the service specified in `IMPLEMENTATION-BRIEF.md` under realistic load. Publish deployment, rollback, task/heap diagnostics, on-call runbook, dashboards, failure injection, and post-test analysis.

Gate:

- sustained and burst load meet latency/throughput/memory objectives;
- dependency failure, slow clients, connection churn, malformed traffic, memory pressure, database restart, and signals recover within defined SLOs;
- no leaked tasks/resources and bounded shutdown;
- VM tests and native deployment exercise the same application semantics;
- at least one staged version upgrade and rollback preserves data/API compatibility.

## P3 — 1.0 release candidate

Required release-candidate evidence:

```sh
ruda fmt --check
ruda check --all-targets
ruda test --engine both
ruda vet --strict
ruda doc --check
ruda audit
```

Plus compiler C tests, all platform CI, sanitizers, fuzzing, bootstrap, reproducibility, compatibility, package-registry, installer, reference-service, and performance gates.

The RC has a published known-limitations list, migration policy, support/security contacts, release signing keys/process, standard-library API review, and no untracked production TODO.

## P4 — Production 1.0

1.0 is `verified` only after the RC soak period closes all release blockers and an independent completion audit maps every normative **must**, decision gate, test matrix row, platform claim, and operational claim to direct evidence.

Post-1.0 work does not retroactively excuse a missing 1.0 requirement. Deferred features remain deferred unless separately designed; 1.0 does not add JIT, arbitrary macros, source-level async, shared-memory threads, inheritance, or general ownership syntax through the back door.
