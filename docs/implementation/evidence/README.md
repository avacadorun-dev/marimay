# Implementation evidence index

This is the evidence navigation layer for the authoritative
[product roadmap](../roadmap.md) and [checkpoint ledger](../checkpoints.md).
Evidence reports are immutable historical observations: their scope can support
a checkpoint without verifying its broader parent product gate.

## Historical identity

Reports dated before the 2026-08-23 Marimay rebrand record commands, paths,
targets, schemas, and artifacts exactly as they existed under the historical
Ruda identity. Their prose, filenames, and external URLs are intentionally not
rewritten as if those runs used Marimay, `mmay`, `.my`, or the consolidated
checkpoint ledger. Historical references to retired planning filenames describe
the context of the original run and are not active planning authority.

## Current versus historical evidence

Product-gate state is current only in `roadmap.tsv` and the product dashboard.
The reports below retain their original identity and scope. A `local` or
`pushed-ci` report does not meet a `tier-1-complete`, `external-pilot`, or
`release-audit` exit criterion by implication.

## D0

- [Design baseline verification](d0-2026-08-10.md).

## N0

- [Open external naming and distribution audit](../../design/naming-audit.md).
- [Atomic repository rebrand](marimay-rebrand-2026-08-23.md).

N0 has repository-local migration evidence, but the external legal, control,
namespace, signing and PATH evidence still does not exist. D123 leaves the gate
open and removes it from the CLI0 claim; it returns as a prerequisite for I7
registry namespaces, any signing or notarization claim, and the 1.0 audit.

## I0

- [Build and CLI foundation](i0-iteration-1-2026-08-10.md), [source ownership and positions](i0-iteration-2-2026-08-10.md), [portability/fault injection/CI](i0-iteration-3-2026-08-10.md), and [memory-safety gates](i0-memory-safety-gates-iteration-4-2026-08-13.md).
- [Gitea recovery and bounded CI concurrency](gitea-recovery-2026-09-08.md)
  records restored services and disk space, preserved failed-job evidence and
  concurrency containment; historical compiler/stall causes remain unknown.
- [Run 212 and Python-cache installation boundary](ci-cache-install-2026-09-08.md)
  records successful C17/fuzz jobs and the 75/76 sanitizer result, plus the
  focused fix for source-tool caches entering a staged distribution.

## A0

- [Bounded architecture CP0](a0-bounded-architecture-checkpoint-0-2026-08-14.md).
- Extraction history: [research/plan](architecture-checkpoint-1-2026-08-13.md), [test owner](architecture-checkpoint-2-2026-08-14.md), [checker result boundary](architecture-checkpoint-3-2026-08-14.md), and [checker sibling boundary](architecture-checkpoint-4-2026-08-14.md).

## I1

- Frontend: [lexer](i1-lexer-iteration-1-2026-08-10.md), [parser](i1-parser-iteration-2-2026-08-10.md), [AST/frontend](i1-frontend-iteration-3-2026-08-10.md), and [incremental/fuzz foundation](i1-frontend-iteration-6-2026-08-10.md).
- Formatter: [library](i1-formatter-library-iteration-4-2026-08-10.md) and [CLI](i1-formatter-cli-iteration-5-2026-08-10.md).
- [Expression-scoped boolean formatting](boolean-expression-formatting-2026-09-05.md)
  corrects continuation indentation in matches, conditions and nested groups;
  it does not establish public-alpha or Tier 1 acceptance.
- Grammar/keywords: [registry](i1-grammar-registry-iteration-7-2026-08-10.md), [completion](i1-grammar-completion-iteration-8-2026-08-11.md), [keyword boundary 9](i1-keyword-boundary-iteration-9-2026-08-12.md), and [keyword boundary 10](i1-keyword-boundary-iteration-10-2026-08-12.md).
- Later public syntax: [canonical managed-array spelling](cp18-array-spelling-2026-08-21.md).

## I2

- Typed foundation and control flow: [typed core](i2-typed-core-iteration-1-2026-08-11.md), [hardening](i2-foundation-hardening-iteration-2-2026-08-11.md), [control flow](i2-control-flow-iteration-3-2026-08-11.md), [signatures/loops](i2-signatures-loop-control-iteration-4-2026-08-11.md), [C-style for](i2-c-for-iteration-5-2026-08-11.md), [calls](i2-calls-iteration-6-2026-08-11.md), [multi-file packages](i2-multifile-package-iteration-7-2026-08-11.md), [constants](i2-constants-iteration-8-2026-08-11.md), [short circuit](i2-short-circuit-iteration-9-2026-08-11.md), [aliases](i2-type-aliases-iteration-10-2026-08-11.md), [bitwise i64](i2-bitwise-i64-iteration-11-2026-08-11.md), and [checked shifts](i2-checked-shifts-iteration-12-2026-08-11.md).
- Nominals and matching: [record declarations](i2-record-declarations-iteration-13-2026-08-11.md), [visibility](i2-public-visibility-iteration-14-2026-08-11.md), [enum declarations](i2-enum-declarations-iteration-15-2026-08-11.md), [record oracle](i2-managed-record-oracle-iteration-16-2026-08-11.md), [enum construction](i2-enum-construction-oracle-iteration-17-2026-08-11.md), [enum match](i2-enum-match-oracle-iteration-18-2026-08-11.md), [guards](i2-guarded-enum-match-iteration-19-2026-08-11.md), [OR patterns](i2-enum-or-patterns-iteration-20-2026-08-11.md), [bool match](i2-boolean-match-iteration-21-2026-08-11.md), [witnesses](i2-match-witnesses-iteration-22-2026-08-11.md), [i64 literal match](i2-i64-literal-match-iteration-23-2026-08-11.md), [block arms](i2-match-block-arms-iteration-24-2026-08-11.md), [return arms](i2-match-return-arms-iteration-25-2026-08-11.md), [all-never matches](i2-all-never-match-iteration-26-2026-08-11.md), [never conditions/guards](i2-never-conditions-guards-iteration-27-2026-08-11.md), [never loop conditions](i2-never-loop-conditions-iteration-28-2026-08-11.md), [never loop steps](i2-never-loop-steps-iteration-29-2026-08-12.md), [strings/panic](i2-string-panic-oracle-iteration-30-2026-08-12.md), [declared never](i2-declared-never-calls-iteration-31-2026-08-12.md), [string patterns](i2-string-literal-match-iteration-32-2026-08-12.md), [record patterns](i2-record-patterns-iteration-33-2026-08-12.md), [record identity](i2-record-identity-iteration-34-2026-08-12.md), and [array oracle](i2-managed-array-oracle-iteration-35-2026-08-13.md).
- Compiler service: [foundation](i2-compiler-service-foundation-iteration-36-2026-08-14.md), [incremental requests](i2-compiler-service-incremental-iteration-37-2026-08-14.md), [checker facts](i2-checker-facts-iteration-38-2026-08-14.md), and [dot-call resolver](i2-dot-call-resolution-iteration-39-2026-08-14.md).
- Vertical/checkpoint evidence: [f64 matrix](v0-f64-matrix-iteration-10-2026-08-14.md), [source arrays](v0-source-arrays-iteration-11-2026-08-14.md), [nominal engines](v0-nominal-engines-checkpoint-1-2026-08-15.md), [local packages CP2](i2-local-packages-checkpoint-2-2026-08-16.md), [service queries CP3](i2-compiler-service-queries-checkpoint-3-2026-08-17.md), [generics CP4](i2-generics-checkpoint-4-2026-08-17.md), [batch check CP8](cp8-authoritative-check-2026-08-18.md), [receiver resolution CP16](cp16-receiver-collection-names-2026-08-20.md), [array spelling CP18](cp18-array-spelling-2026-08-21.md), [program entry/status CP31](cp31-program-arguments-2026-08-24.md), [typed streams CP32](cp32-typed-standard-streams-2026-08-25.md), [text/Bytes CP33](cp33-text-bytes-2026-08-26.md), [paths/files CP34](cp34-paths-filesystem-2026-08-29.md), [environment/process CP35](cp35-environment-process-context-2026-08-29.md), and [Map/Set CP36](cp36-map-set-iterators-2026-08-30.md).
- [Bounded typed JSON CP38](cp38-bounded-typed-json-2026-09-01.md)
  supplies explicit-codec privacy, typed error, and package-boundary evidence.

## I3

- [Bytecode foundation](i3-bytecode-foundation-iteration-1-2026-08-12.md), [scalar VM](i3-scalar-vm-iteration-2-2026-08-13.md), [bitwise/shifts](i3-bitwise-shifts-iteration-3-2026-08-13.md), [direct calls](i3-direct-calls-iteration-4-2026-08-13.md), [strings/panic/never](i3-string-panic-never-iteration-5-2026-08-13.md), [f64](i3-f64-iteration-8-2026-08-13.md), [managed arrays](i3-managed-arrays-iteration-9-2026-08-14.md), [source arrays](v0-source-arrays-iteration-11-2026-08-14.md), [test loop CP11](cp11-ruda-test-2026-08-18.md), [resource policy CP19](cp19-resource-limits-2026-08-21.md), [program arguments/status CP31](cp31-program-arguments-2026-08-24.md), [typed streams CP32](cp32-typed-standard-streams-2026-08-25.md), [text/Bytes CP33](cp33-text-bytes-2026-08-26.md), [paths/files CP34](cp34-paths-filesystem-2026-08-29.md), [environment/process CP35](cp35-environment-process-context-2026-08-29.md), and [Map/Set CP36](cp36-map-set-iterators-2026-08-30.md).
- [Bounded typed JSON CP38](cp38-bounded-typed-json-2026-09-01.md)
  supplies verified-bytecode, VM, hostile-input, and sustained-fuzz evidence.

## I4

- [Scalar C17 emitter](i4-scalar-c17-emitter-iteration-1-2026-08-13.md), [strings/panic/never](i4-string-panic-never-iteration-2-2026-08-13.md), [managed arrays](i4-managed-arrays-iteration-9-2026-08-14.md), [f64/matrix](i3-f64-iteration-8-2026-08-13.md), [native build CP10](cp10-native-build-2026-08-18.md), [test equivalence CP11](cp11-ruda-test-2026-08-18.md), [profile runtime CP13](cp13-release-runtime-2026-08-19.md), [resource policy CP19](cp19-resource-limits-2026-08-21.md), [algorithm rebaseline CP28](cp28-release-rebaseline-2026-08-22.md), [release hardening](cp28-release-hardening-2026-08-23.md), [program arguments/status CP31](cp31-program-arguments-2026-08-24.md), [typed streams CP32](cp32-typed-standard-streams-2026-08-25.md), [text/Bytes CP33](cp33-text-bytes-2026-08-26.md), [paths/files CP34](cp34-paths-filesystem-2026-08-29.md), [environment/process CP35](cp35-environment-process-context-2026-08-29.md), and [Map/Set CP36](cp36-map-set-iterators-2026-08-30.md).
- [Bounded typed JSON CP38](cp38-bounded-typed-json-2026-09-01.md)
  supplies generated-C, VM/native differential, sanitizer, and resource
  evidence.
- [CP41 x86_64 code-generation trigger](cp41-x86-codegen-trigger-2026-08-24.md)
  records a pushed-run performance anomaly and same-source cross-target
  narrowing. It scopes the ready checkpoint but is not its clean D056 baseline
  or completion evidence.

## I5

- [Managed arrays foundation](i5-managed-arrays-iteration-1-2026-08-13.md), [generic arrays CP5](i5-arrays-checkpoint-5-2026-08-17.md), [nominal engines](v0-nominal-engines-checkpoint-1-2026-08-15.md), [array mutation CP17](cp17-array-mutation-2026-08-21.md), [array spelling CP18](cp18-array-spelling-2026-08-21.md), [resource policy CP19](cp19-resource-limits-2026-08-21.md), [typed streams CP32](cp32-typed-standard-streams-2026-08-25.md), [text/Bytes CP33](cp33-text-bytes-2026-08-26.md), [paths/files CP34](cp34-paths-filesystem-2026-08-29.md), [environment/process CP35](cp35-environment-process-context-2026-08-29.md), [Map/Set CP36](cp36-map-set-iterators-2026-08-30.md), [bounded typed JSON CP38](cp38-bounded-typed-json-2026-09-01.md), and CP20–CP28 reports indexed below.

## CLI0

[CP29](cp29-release-contract-2026-08-23.md) supplies the local release contract,
version/help/install metadata, runnable quick start, and 42-row closure matrix.
[CP30](cp30-dynamic-strings-2026-08-23.md) supplies verified local dynamic-text,
differential, allocation, performance, and sustained-fuzz evidence.
[CP31](cp31-program-arguments-2026-08-24.md) supplies bounded argument,
status, cleanup, VM/native differential, and allocation evidence.
[CP32](cp32-typed-standard-streams-2026-08-25.md) supplies bounded typed
stdin/stdout/stderr, flush, capability, closed-pipe, process, differential,
and allocation evidence. [CP33](cp33-text-bytes-2026-08-26.md) supplies owned
UTF-8 text, nominal Bytes, bounded builders, binary-stream, sustained-fuzz,
allocation, and VM/native evidence. [CP34](cp34-paths-filesystem-2026-08-29.md)
supplies typed paths, capability roots, bounded and atomic files, metadata,
directory order, cleanup, and differential evidence.
[CP35](cp35-environment-process-context-2026-08-29.md) supplies explicit
environment grants, exact lookup, redaction, normalized process paths,
relocation, allocation, sanitizer, and VM/native evidence.
[CP36](cp36-map-set-iterators-2026-08-30.md) supplies insertion-ordered
`Map`/`Set`, coherent closed keys, iterator invalidation, precise tracing,
allocation, fuzz, and VM/native evidence.
[CP37](cp37-cli-parser-2026-08-30.md) supplies ordinary `std/cli`, typed
definition/usage errors, deterministic help, allocation, fuzz, and
VM/native evidence. [CP38](cp38-bounded-typed-json-2026-09-01.md) supplies
explicit bounded codecs, privacy, hostile-input, allocation, sustained-fuzz,
sanitizer, and VM/native evidence. None of these reports closes CLI0 or any
parent product gate. The user-prioritized CP42 is now the sole in-progress
checkpoint; CP39–CP42 must each add completion evidence.

## CP42

[Scalar array VM regions](cp42-vm-scalar-arrays-2026-09-09.md)
([raw evidence](cp42-vm-scalar-arrays-2026-09-09.json)) records all scalar storage
widths, a 2.76× paired Dijkstra gain, the complete 32-workload VM diagnostic,
86/86 Release tests, focused sanitizers and the installed system binary.

[Per-run catalogue toolchain snapshots](cp42-catalog-snapshot-2026-09-09.md)
record isolation from compiler updates, 95 harness tests, and real smoke
acceptance while a disposable original installation is replaced. The external
harness patch preserves the user's pre-existing uncommitted catalogue work.

[Scalar/control MIR preparation](cp42-scalar-flow-2026-09-09.md)
([raw evidence](cp42-scalar-flow-2026-09-09.json)) records D106, full native and
diagnostic VM matrices, a caught native fill regression, and passing applicable
results for all 86 Release cases across the full run and focused reruns. Parity
and release gates remain open.

[VM execution regions and frame reuse](cp42-vm-regions-2026-09-09.md)
([raw matrices, instruction probes and checks](cp42-vm-regions-2026-09-09.json))
records D104/D105, 32 unchanged full-size diagnostics, exact instruction counts,
GC/fuel/failure coverage and 86 passing Release tests. It leaves parity and all
release-gate obligations open.

[Verified VM execution image](cp42-vm-execution-image-2026-09-09.md) records
the measured D107 rejection, D108 fixed operations, prepared branches/callees,
root-borrow eligibility, generic composed-loop fuel checks and paired
insertion/DSU/control diagnostics. It measures an interpreter ceiling and leaves
additional trace families, parity and every product/platform gate open.

[SLJIT feasibility with the reused ARM64 prototype](cp42-sljit-feasibility-2026-09-10.md)
([paired observations](cp42-sljit-feasibility-paired-2026-09-10.json),
[32-workload correctness](cp42-sljit-feasibility-correctness-2026-09-10.json))
records the shared backend, corrected record selectors and retained view
identity. Dijkstra improves 7.70× against the paired control, but insertion
regresses 2.52×. This is reproducible experimental evidence; central allocation,
W^X/failure/platform conformance and performance parity remain open.

[Shared JIT memory ownership and composed loops](cp42-sljit-owned-memory-2026-09-10.md)
([validation](cp42-sljit-owned-memory-validation-2026-09-10.json),
[paired 32-workload matrix](cp42-sljit-owned-memory-paired-2026-09-10.json))
adds central SLJIT allocation, immutable code publication, D109 composition,
all-index local ownership checks and 32-workload correctness on macOS ARM64,
x86-64 under Rosetta and ARM64 ASan/UBSan. The paired full matrix reduces the
sum of VM medians by 46.1%, with no median regression above 2%.
It remains an experiment; production
integration, full JIT conformance, Linux LSan and Tier 1 parity remain open.

[JIT fuel selected from invocation options](cp42-sljit-fuel-mode-2026-09-10.md)
([validation](cp42-sljit-fuel-mode-validation-2026-09-10.json),
[paired six-workload observations](cp42-sljit-fuel-mode-paired-2026-09-10.json))
records exact bounded/shared accounting, conservative rejection of incompatible
unmetered entries and the additional 15.3% Dijkstra improvement. All 32
correctness cases pass in three local configurations; production and Tier 1
parity remain open.

[Optional shared JIT integrated into the C17 compiler](cp42-sljit-integration-2026-09-10.md)
([validation](cp42-sljit-integration-validation-2026-09-10.json),
[paired 32-workload observations](cp42-sljit-integration-paired-2026-09-10.json))
records D111's opt-in production ownership, pinned offline inputs, exact-type
admission, signed/wide selectors, 90/90 enabled and 86/86 disabled CTest passes,
and three complete local correctness replays. The fresh full matrix reduces
the VM median sum by 46.5%, without a median regression above 2%. Default
enablement, Linux JIT/LSan results, full Tier 1 acceptance and Go/Rust parity
remain open.

[Native-call checkpoint rejected by the performance gate](cp42-sljit-calls-2026-09-10.md)
records a reproducible source patch, full JIT/interpreter matrices, one planned
confirmation per flagged case and passing local correctness checks. Stack,
queue and DSU meet the stage's speedup targets, but frequency-map has confirmed
regressions above 2% in both configurations. The candidate is not accepted into
the production compiler. The [SMART plan](../vm-performance-plan.md) retains
the original per-workload gate and the open overall parity objective.

[Frame-cursor candidate held and remaining JIT costs profiled](cp42-sljit-cursor-2026-09-10.md)
records complete local correctness, full JIT/interpreter matrices and the one
planned BFS confirmation. Median criteria pass, but repeatable slow BFS runs
leave stability unresolved; the implementation remains a reproducible patch.
Nine subsequent diagnostic profiles locate 49.6% of BFS and 58.2% of DSU samples
under object binding, with raw stacks and a reproduction script preserved.

[Checked object-view reuse across native JIT entries](cp42-sljit-views-2026-09-10.md)
([validation](cp42-sljit-views-validation-2026-09-10.json),
[fresh five-participant matrix](cp42-sljit-views-comprehensive-2026-09-10.json))
records exact GC/provenance revalidation, shared logical calls, 93/93 JIT and
86/86 interpreter tests, sanitizer/Rosetta correctness and replayable inputs.
The paired JIT median sum improves 23.2%, with no confirmed median/P90 regression;
OFF remains within its gates. The new full comparison includes all 32 workloads
in native Marimay, Go, Rust, opt-in VM/JIT and Bun. Overall parity remains open.

[Array-segment candidate rejected by DSU confirmation](cp42-sljit-array-segments-2026-09-10.md)
([validation](cp42-sljit-array-segments-validation-2026-09-10.json)) preserves
the read-only experiment, the extended candidate's full correctness and both
paired matrices, and the 2.14% / 2.05% DSU median regressions. Its checkpoint
overrun and all observations remain visible.

[Retaining unchanged array identity after native push](cp42-sljit-array-identity-2026-09-10.md)
([validation](cp42-sljit-array-identity-validation-2026-09-10.json)) records the
changed candidate, new enabled/sanitizer/Rosetta validation and the explicit
reuse of the identical OFF compiler's prior evidence. The new full JIT matrix
improves the median sum 9.9%; no workload exceeds either threshold, and no
additional JIT confirmation is needed. DSU remains within tolerance at +1.77%
median and -1.05% P90. This accepts the opt-in implementation locally, not parity
or full Tier 1 support.
The [fresh five-participant comparison](cp42-sljit-array-identity-comprehensive-2026-09-10.md)
([raw observations](cp42-sljit-array-identity-comprehensive-2026-09-10.json))
uses the same candidate and passes all 32 unchanged workloads. All five columns
come from one 3-warmup/11-sample series; source preparation remains inside VM
timings. Dispersion, including the noisy Rust frequency-map result, is retained.

[Prepared activations and retained-view proofs](cp42-sljit-prepared-activations-2026-09-10.md)
preserves both early candidates, profiling, the complete JIT/OFF matrices and
the second candidate's original observed-performance rejection.
[Compile-time OFF isolation](cp42-sljit-prepared-isolation-2026-09-11.md)
([validation and reproduction](cp42-sljit-prepared-isolation-validation-2026-09-11.json))
retains that exact measured JIT executable and proves canonical OFF code identity.
It records the inconclusive A/A control, all raw observations, current tests,
the sleep-timeout recheck, sanitizer/Rosetta checks and the deadline overrun.
Local opt-in acceptance does not establish default enablement or overall parity.
The [fresh 32-workload comparison](cp42-sljit-prepared-isolation-comprehensive-2026-09-11.md)
([raw observations](cp42-sljit-prepared-isolation-comprehensive-2026-09-11.json))
records all five participants in one series with the same measured compiler,
114 unchanged source/harness inputs and 64 MiB managed heap limit. Its host
monitor and dispersion are retained; absolute values from different series
are not a causal code-regression test.

[Source-run phase attribution](cp42-source-phase-profile-2026-09-11.md) records
two 32-workload diagnostic series with unchanged inputs and exact compiler
identity. Repeated MIR and bytecode-function verification dominates preparation
on short cases; it selects a bounded verification-deduplication candidate.
These are diagnostic measurements, not a new accepted optimization or parity.

[Canonical program verification](cp42-program-verification-2026-09-11.md)
removes three redundant function-verifier passes from source preparation while
retaining D049, standalone API checks, complete program verification and VM
admission. All 32 encoded programs match the control byte-for-byte. Full paired
JIT/interpreter median sums fall 3.26%/0.77%, with geometric mean speedups of
1.055x/1.032x; no workload exceeds the predeclared 2% median / 5% P90 thresholds.
Current validation passes 96/96 JIT tests, 86/86 interpreter tests, and 12 focused
suites plus all 32 workloads under both ASan/UBSan and x86 Rosetta. This is local
implementation acceptance; default JIT enablement, Linux LSan, full Tier 1 and
overall Go/Rust parity remain open.

[JIT execution diagnosis](cp42-jit-execution-profile-2026-09-11.md)
attributes about 51% of DSU and 46% of stack sampled stacks to compiled call/return
transitions. Separate counters show 15 million DSU calls with almost complete
prepared-layout reuse, while stack alternates layouts. This selects a guarded
nonallocating activation-path experiment in the shared owners, with a predeclared
15% early DSU/stack latency target. These are diagnostic samples and counts, not
an additional accepted speedup; all 32, safety and overall parity gates remain.

[Compiler IPO, shared calls and VM dispatch](cp42-ipo-shared-vm-2026-09-11.md)
passes the complete unchanged all 32 paired gate in both modes, with geometric
mean speedups of 1.196x for JIT and 1.200x for the default interpreter.
Rejected no-IPO, combined IPO/call and partial cursor variants remain recorded.
Exact finite/shared fuel, roots, depth and allocation-failure behavior are retained;
97/97 ON and 87/87 OFF tests plus focused sanitizer/Rosetta and all 32 replays pass.
Native runtime objects and Native/Go/Rust/Bun executable hashes match prior evidence.
JIT remains opt-in; Linux LSan, full Tier1 and overall Go/Rust parity remain open.

[Address-level JIT attribution](cp42-jit-address-attribution-2026-09-11.md) maps current generated-code samples back to
VM operations on DSU, BFS, stack and min-heap. Both diagnostic variants preserve
all inputs and all48 expected outcomes. Repeated checked-binding work across
managed-reference copies selects a segment-local alias analysis experiment;
C call/runtime costs and source preparation remain independent gaps. This is
diagnostic evidence, not an additional speedup or a parity claim.

[Verified VM trace batching](cp42-vm-trace-batching-2026-09-09.md) records D109,
the exact array-shift trace contract, exhaustive finite frame/shared-fuel and
side-exit equivalence, a passing 32-workload VM replay and paired D108/candidate
measurements. `insertion` improves 4.18× over its same-base control and about
26.2× over the original user result while preserving its checksum. VM/Bun
parity, other trace families, source-startup work and all product/platform
gates remain open.

[Full catalogue measurement and native child access](cp42-catalog-performance-2026-09-09.md)
([raw matrices and profiles](cp42-catalog-performance-2026-09-09.json)) records
32 full-size workloads, the quadratic native queue-cache cause, and remaining
text/collection/VM costs. It supplies local performance evidence, not CP42 or
Tier 1 closure.

[Managed VM string lifetimes](cp42-vm-string-lifetime-2026-09-09.md)
([raw evidence](cp42-vm-string-lifetime-2026-09-09.json)) records the exact
10,000-row concatenation regression, shared D055 live-heap accounting,
GC/host-reference/allocation coverage, and the new 21-workload correctness
matrix. It does not establish native/Go/Rust performance parity or close CP42.

[Stateful CLI wave and shared text/CFG fixes](cp42-stateful-2026-09-09.md)
([raw evidence](cp42-stateful-2026-09-09.json)) records the final two independent
apps, 85/85 local tests, focused sanitizer coverage, fixed substring/RPO causes
and unchanged failing startup/test-loop budgets. Full acceptance remains open.

[Nonfatal mutable-capture diagnostics](cp42-escape-warning-2026-09-09.md)
([raw evidence](cp42-escape-warning-2026-09-09.json)) records D103's bounded
checker analysis, continuing diagnostic transport, allocation/budget and
both-engine checks. This closes the local warning removal gap from D098;
source-lint/delimited, full portfolio acceptance and Tier 1 remain open.

[Direct-bottom enum transport](cp42-enum-bottom-2026-09-09.md)
([raw evidence](cp42-enum-bottom-2026-09-09.json)) records D102's common
Result/Option/user-enum metadata, impossible match/propagation paths, negative
and both-engine checks. This removes the mapper never guard locally; D103 above subsequently supplies
the warning. Full CP42/Tier 1 acceptance remains open.

[Array-search CMake policy correction](cp42-array-search-cmake-policy-2026-09-08.md)
reproduces run 220's panic-expectation failure on CMake 3.25 and verifies the
standalone script with explicit policies on both CMake 3.25.2 and 4.4.2. No
runtime behavior or required test was changed; exact-revision CI remains open.

[File/context applications and void-pattern diagnostics](cp42-files-context-2026-09-08.md)
records manifest/project-check/env-paths, their shared bounded tree/config owners,
88 exact process scenarios, 1000/10000-file measurements and the D092 named-void
pattern correction. Eight apps are implemented; full acceptance and first native
startup remain open. Both callback diagnostic/never-transport gates remain open.

[Shared file lines and three streaming CLI consumers](cp42-file-lines-2026-09-08.md)
records D101, file/stdin cursor and allocation contracts, native record-view
lifetime repair, logstats/grep/frequency source/process acceptance, and 2/20 MiB
measurements. Full local Release passes 82/82 and ASan/UBSan 81/81. First native
startup, full portfolio/platform evidence and diagnostics gates remain open.

[Ordinary Result mappers and native root lifetime](cp42-mapper-values-2026-09-08.md)
records D100 callback scope/order, common callable reuse in Notes/config and the
forced-GC repair for branch, loop and mutable-child root lifetime. Uninhabited
Result payload transport and the escaping-capture warning remain open.

[Predicate array search and Notes integration](cp42-array-search-2026-09-08.md)
records D099, callback/generation order, allocation/GC and public engine checks,
plus shared ID lookup/removal and literal-text CLI search in Notes. This supplies
an application consumer of D098. Full local Release passes 80/80 and macOS
ASan/UBSan 79/79; warning delivery and full CP42 acceptance remain open.

[Exact function values and closure lifetime](cp42-function-values-2026-09-08.md)
records D098, shared managed captures, typed interface dispatch, source locations,
allocation/GC and public VM/native checks; final local Release passed 79/79 and
macOS ASan/UBSan passed 78/78. The nonfatal escaping-mutable warning,
predicate search and full platform/application acceptance remain open.

[Shallow array copy and stable removal](cp42-array-edit-2026-09-08.md) records
D097, typed MIR/wire/runtime ownership, allocation and GC boundaries, public
VM/native behavior and the functional Notes migration; all 78 local Release
tests have passing results and full macOS ASan/UBSan passed 77/77. The near-limit paired
sample records time and memory rather than declaring full performance
acceptance. D098 supplies the callback lifetime foundation below the still-open
predicate search and platform/application gates.

[Scoped Option conditions](cp42-option-binding-2026-09-08.md) records D096,
exact enum/CFG lowering, parser recovery, scope/cleanup/allocation coverage,
full local Release 76/76, focused ASan/UBSan and config/notes consumer migration.
It closes the local implementation slice, not the remaining shared collection
contracts or platform/application acceptance.

[Notes collection baseline and mutation-result consumption](cli0-notes-contract-baseline-2026-09-08.md)
records 14 source tests, 27 process cases across three engines and 15 closed-pipe
checks, including a reproduced and fixed false-success output failure. The
linked shared-contract packet proposes the remaining collection/Option family;
it does not mark those APIs, the notes migration or CP42 complete.

[Shared external-CLI acceptance mechanics](cli0-shared-acceptance-2026-09-07.md)
records the versioned process/build/parity driver, its failure-detection tests
and migration of both config adapters with all 32 process and 23 filesystem
cases retained across VM/native Debug/Release. This closes shared harness
preparation within cycle 1, not the remaining CI/platform/application gates.

[Accepted-route baseline audit](cli0-baseline-audit-2026-09-07.md) reconciles
the 42 CLI0 assignments and separates run 206 passing test steps from failed
artifact uploads. Historical native/allocation failures, skipped TSan and
missing host/portfolio evidence remain open; this is cycle 1 progress, not
CP42 verification.

[Compound-loop-binding recovery](cp42-for-recovery-2026-09-07.md) records
D068 lossless synchronization, exact command rejection, allocation/limit
teardown and full local Release 74/74; predecessor Linux CI remains red.

[Test panic source ranges](cp42-test-locations-2026-09-07.md) records D095
primary source metadata and preservation of stderr before a panic. Its report
tracks local verification, negative/allocation cases and remaining checks.

[Exact mapError context](cp42-map-error-context-2026-09-07.md) records D094
nominal mapper selection, scoped propagation context, shadowing/Result checks,
allocation and engine parity, and removal of the config mapper workaround.

[Ordinary test-file helpers](cp42-test-helpers-2026-09-07.md) records D093
expectation scope, discovery/isolation, full panic metadata, allocation
coverage, local/emulated checks and migration of the config error helper.
The terse human failure report remains a separate diagnostic finding.

[Void match arms and lexical cleanup](cp42-void-match-2026-09-07.md)
records D092 typing, branch-local defer/root lifetime, all-index checker
failure coverage, VM/native/formatter acceptance and migration of the real
config application. Local and emulated Linux checks retain their stated scope.

[Grouped index and amortized rebuilding](cp42-grouped-collections-2026-09-07.md)
records the selected D090/D091 implementation after the structural review:
paired original workloads, broad key/operation cases, reservation tradeoffs,
failure/GC/VM/native checks and installed application acceptance. This remains
local evidence, with battery-powered timing and emulated Linux qualifications;
CP42 and CLI0 stay open.

[System-level collection review](cp42-collection-system-review-2026-09-07.md)
separates input construction, generated counting code and checked runtime,
records probe/allocation counts and compares actual benchmark implementations.
It sets the next structural experiments without claiming a selected table
replacement, counting speedup or checkpoint closure.

[Independent collection cursors](cp42-collection-cursors-2026-09-07.md) remove
quadratic full traversal while preserving legacy ordinal bytecode semantics.
Scaling, sparse/nested loops, full local macOS suites and focused Linux
sanitizer/fuzz evidence are recorded. Counting-workload performance and the
broader collection implementation review remain open.

[Compact collection storage](cp42-compact-collections-2026-09-07.md) reduces
raw Map/Set reservation with exact layout/accounting and failure coverage.
This is a memory improvement and partial hot-path evidence, not closure of
the remaining collection performance or public-alpha gates.

[Private inline reference validation](cp42-inline-validation-2026-09-07.md)
removes internal call overhead without removing checks or requiring LTO.
Three local series show a further 5.6–7.5% frequency-map improvement;
smaller Set/rune gains do not close the collection performance obligation.

[Rune equality](cp42-rune-equality-2026-09-07.md) repairs primitive and
structural comparison of Unicode scalar values. Collection performance and
the broader CP42 acceptance remain open.

[Checked Set insertion](cp42-set-insertion-2026-09-07.md) removes repeated
receiver validation through a shared inlined insertion core. Three local
series show 10.7–15.8% Set improvement; the full collection item remains open.

[Scalar lookup reuse and bounded probing](cp42-collection-lookup-reuse-2026-09-07.md)
avoid repeating the hash/probe in scalar Map counters. Three local paired runs
show further frequency-map and rune-frequency gains; Set remains unresolved.
Memoization adds fixed per-object bytes and does not close the performance item.

[Collection hash and descriptor boundaries](cp42-collection-hash-descriptors-2026-09-06.md)
remove repeated immutable-layout validation and specialize the exact fixed-word
SipHash path. Two local paired runs show further Map/Set improvement, while
rune-frequency measurements remain noisy. The optimization obligation stays open.

[Native collection results and verifier worklists](cp42-collection-results-2026-09-06.md)
reduce local Map/Set/rune workload medians by 19.5–27.4% and remove repeated
whole-CFG fixed-point scans. This is partial progress; the full collection
optimization and language ergonomics obligations remain open.

[Bounded JSON string runs](cp42-json-string-runs-2026-09-06.md) remove per-byte
builder/Result overhead in the ordinary source decoder. Paired version-last
config median falls from 9.449 s to 5.243 s with unchanged inputs, diagnostics
and limits. The first native launch and other acceptance obligations stay open.

[Shared native test build](cp42-native-test-build-2026-09-06.md) replaces seven
native compilations with one in the config test loop while keeping fresh
processes and arguments. Paired cold time falls from 10.505 s to 1.910 s; all
three samples meet the unchanged 10 s budget. Other performance/resource
obligations still block progression to the next utility.

[Test preparation reuse](cp42-test-preparation-2026-09-06.md) removes repeated
whole-program lowering and codec work while retaining verification and fresh
per-test execution state. At that revision the unchanged seven-test workload
was faster cold and warm but still missed its cold budget; the shared-native
follow-up above resolves that remaining compilation cost.

[Config performance repairs](cp42-config-performance-2026-09-06.md) identify and
remove redundant VM dispatch and native proof searches, retaining exact
generated C and local before/after measurements. That revision left the cold
test-loop gate open; subsequent reports above continue the repair. None is
completion of the application or CP42.

[Local VM execution policy](cp42-local-execution-policy-2026-09-06.md) records
the first external config utility's maximum-input failure, D087's separation
of local commands from embedder fuel, and focused boundary/differential checks.
The portfolio and the remaining resource-parity work stay open.

[Intrinsic metadata and the external-portfolio start](cp42-json-intrinsics-2026-09-06.md)
records local signature/completion, shadowing, cleanup and differential checks,
the exact prior run 189 scope, and the first independent config application.
The [ten-application acceptance portfolio](../cli-ergonomics-suite.md) is mandatory
before CP42/CLI0 closure; neither this first application nor the metadata closes it.

[Initial automatic typed-JSON slice](cp42-typed-json-first-slice-2026-09-05.md)
records local checker allocation and four both-engine source tests. It does
not close the complete notes-driven ergonomics card or any release gate.
[The notes/startup follow-up](cp42-notes-startup-2026-09-06.md) records the
Russian application migration, process smoke tests, sanitizer observations and
the shared D062 convergence/bytecode-adapter repair. Its scope remains local.
[Structured JSON error paths](cp42-json-error-paths-2026-09-06.md) records the
field/index model, 13 both-engine source tests, a complete 67-test Debug run,
focused sanitizer/allocation checks and a short Linux fuzz smoke. It also
records the installed Release-command handoff; its scope remains local and
does not close the remaining CP42 work or establish pushed-revision CI.

[Portfolio coverage matrix and finding dispositions](cp42-portfolio-matrix-2026-11-11.md)
([raw evidence](cp42-portfolio-matrix-2026-11-11.json)) re-runs the ten
applications at compiler `9139cca` on both engines: 51 source tests, 312 process
cases, a coverage table generated from application source, and a disposition for
every finding, including the reproduced `byte`-literal `I0001` gap. It closes
the coverage gap only; startup budgets, the perf timebox, the canonical program,
the release, Tier 1, sanitizers and parity remain open.

[Startup budget re-check](cp42-startup-budget-recheck-2026-11-11.md) runs both
applications' own measurement harnesses at release compiler `4bf2885`: the small
VM row passes at 154-226 ms and the warm `test --engine both` row at
0.44-1.86 s, while the first-native row is shown to measure platform first-exec
cost (246 ms against a 240 ms trivial-binary baseline) and is re-budgeted by
D125 rather than repaired.

## CP41

[The x86_64 code-generation trigger](cp41-x86-codegen-trigger-2026-08-24.md)
localizes selective stack/queue/heap pressure in generated native C. CP41 is
planned after CP42 and before CP39–CP40 so the installed CLI0 artifact contains
the repair. Its trigger run is too noisy to become a new D056 baseline; a clean
before/after pushed-run evidence pair is still required. Go/Rust ratios remain
exploratory.

## I6 and E0

No direct I6 implementation or pilot evidence exists. I6 remains planned after
CLI0, whose I4 path includes CP41. E0 is now an agent pilot defined by D124 and
[the pilot protocol](../pilot-protocol.md); no session has run, so no evidence
exists yet.

## Historical checkpoint evidence

| Checkpoint | Direct evidence | Historical outcome scope |
|---|---|---|
| CP0 | [A0 bounded architecture](a0-bounded-architecture-checkpoint-0-2026-08-14.md) | Local plus partial Tier 1 architecture closure; A0 remains open. |
| CP1 | [Nominal engines](v0-nominal-engines-checkpoint-1-2026-08-15.md) | Record/enum/interface vertical slice. |
| CP2 | [Local packages](i2-local-packages-checkpoint-2-2026-08-16.md) | Local package/import/D043 slice. |
| CP3 | [Compiler-service queries](i2-compiler-service-queries-checkpoint-3-2026-08-17.md) | Internal facts/completion slice. |
| CP4 | [Generics](i2-generics-checkpoint-4-2026-08-17.md) | Bounded Option/Result/question slice. |
| CP5 | [Generic arrays](i5-arrays-checkpoint-5-2026-08-17.md) | Managed array vertical slice. |
| CP6 | [Numeric widths](cp6-numeric-widths-2026-08-17.md) | Numeric/text formatter slice. |
| CP7 | [Lexical defer](cp7-lexical-defer-2026-08-18.md) | Pre-suspension cleanup slice. |
| CP8 | [Batch check](cp8-authoritative-check-2026-08-18.md) | Checker-backed preview command. |
| CP9 | [VM run/stdout](cp9-ruda-run-stdout-2026-08-18.md) | Local verified VM execution. |
| CP10 | [Native build](cp10-native-build-2026-08-18.md) | Content-addressed native artifact. |
| CP11 | [Test loop](cp11-ruda-test-2026-08-18.md) | Deterministic VM/native test core. |
| CP12 | [Private-preview integration](cp12-final-preview-2026-08-18.md) | Local/private integration only. |
| CP13 | [Release runtime](cp13-release-runtime-2026-08-19.md) | Profile-coherent runtime selection. |
| CP14 | [Imported arrays](cp14-imported-arrays-2026-08-19.md) | Cross-package managed arrays. |
| CP15 | [Imported results](cp15-imported-results-2026-08-20.md) | Imported generic enum/results. |
| CP16 | [Receiver collection names](cp16-receiver-collection-names-2026-08-20.md) | D003/D043 resolution correction. |
| CP17 | [Array mutation](cp17-array-mutation-2026-08-21.md) | Mutation/reservation/iteration contract. |
| CP18 | [Array spelling](cp18-array-spelling-2026-08-21.md) | Canonical `[]T` replacement. |
| CP19 | [Resource limits](cp19-resource-limits-2026-08-21.md) | Managed-heap budget and I0002. |
| CP20 | [Reusable corpus](cp20-reusable-algorithm-corpus-2026-08-21.md) | Workload fidelity, not speed authority. |
| CP21 | [Performance baseline](cp21-array-performance-baseline-2026-08-22.md) | D057 Linux baseline; five-platform gate open. |
| CP22 | [MIR range proofs](cp22-mir-range-proofs-2026-08-22.md) | Verified bounds capabilities. |
| CP23 | [Native array access](cp23-native-array-access-2026-08-22.md) | Versioned exact-access ABI. |
| CP24 | [Dense storage](cp24-dense-array-storage-2026-08-22.md) | Raw-scalar representation optimization. |
| CP25 | [Capacity](cp25-array-capacity-2026-08-22.md) | Exact/reserved capacity planning. |
| CP26 | [Nested views](cp26-nested-array-views-2026-08-22.md) | Activation-scoped managed views. |
| CP27 | [Integer proofs](cp27-integer-proofs-2026-08-22.md) | Verified checked-integer elimination. |
| CP28 | [Rebaseline](cp28-release-rebaseline-2026-08-22.md); [hardening](cp28-release-hardening-2026-08-23.md) | Algorithm integration boundary before product work. |
| CP29 | [Release contract](cp29-release-contract-2026-08-23.md) | Local metadata/help/install contract and closure assignment; no publication claim. |
| CP30 | [Dynamic strings](cp30-dynamic-strings-2026-08-23.md) | Verified local primitive interpolation, format/parse, VM/native, allocation, performance, and fuzz vertical. |
| CP31 | [Program arguments](cp31-program-arguments-2026-08-24.md) | Verified local bounded UTF-8 arguments, test injection, and cleanup-safe VM/native process statuses. |
| CP32 | [Typed standard streams](cp32-typed-standard-streams-2026-08-25.md) | Verified local bounded typed stdin/stdout/stderr, flush, capability, closed-pipe, allocation, and VM/native process evidence. |
| CP33 | [Text and Bytes](cp33-text-bytes-2026-08-26.md) | Verified local owned UTF-8, nominal Bytes, bounded builders, binary stdio, allocation, sustained-fuzz, and VM/native evidence. |
| CP34 | [Paths and filesystem](cp34-paths-filesystem-2026-08-29.md) | Verified local typed paths, capability-confined bounded files, atomic writes, metadata, sorted directories, cleanup, allocation, sanitizer, and VM/native process evidence. |
| CP35 | [Environment and process context](cp35-environment-process-context-2026-08-29.md) | Verified local explicit environment grants, exact lookup, redaction, normalized process paths, relocation, allocation, sanitizer, and VM/native process evidence. |
| CP36 | [Map, Set, and iterators](cp36-map-set-iterators-2026-08-30.md) | Verified local insertion order, coherent key laws, iterator invalidation, managed tracing, exhaustive allocation cleanup, fuzz, and VM/native evidence. |
| CP37 | [First-party CLI parser](cp37-cli-parser-2026-08-30.md) | Verified local ordinary `std/cli`, typed errors, deterministic help, allocation, fuzz, and VM/native evidence. |
| CP38 | [Bounded typed JSON](cp38-bounded-typed-json-2026-09-01.md) | Verified pushed-CI explicit codecs, deterministic transformation, privacy, allocation, sanitizers, sustained fuzz, and VM/native evidence. |

The [planning migration record](../planning-migration.md) maps retired planning
sections to the consolidated roadmap, ledger, and this index without altering
the historical reports.

[JIT reference aliases and record-field reuse](cp42-jit-reference-values-2026-09-11.md) passes the local
unchanged all 32 gate: median sum -5.49%,
geometric speedup 1.022x, with no confirmed regression.
The alias-only variant remains rejected. 99/99 ON tests, 14 focused tests in
sanitizer/Rosetta builds and all 32 correctness pass. Exact OFF binary/dependency
provenance reuses previous interpreter evidence; Native/comparator binaries are
unchanged. JIT remains opt-in; full Tier1, Linux LSan and parity remain open.

[VM reachable preparation: rejected candidate](cp42-vm-reachable-preparation-rejected-2026-09-11.md) records completed
source-phase diagnosis and the unchanged-corpus experiment. A fixed recheck
with sampled foreign-compute rejection gives JIT median sum -3.41% and OFF -1.45%,
but Dijkstra OFF P90 repeats above the 5% gate (+18.22%, +15.64%). Production
remains at the f4c9961 implementation. The rejected patch, correctness evidence,
contaminated first series and all recheck observations are preserved. Full
Tier 1, Linux LSan and overall Go/Rust parity remain open.

[Text runtime diagnosis and current accepted-version snapshot](cp42-text-runtime-diagnosis-2026-09-11.md) records
reproducible string counters and a fresh five-participant all 32 comparison of
the unchanged f4c9961 implementation. Both engines duplicate 1.356 GB of
concatenation copying in report-concat; direct shared-runtime string construction
is selected for the next experiment, not implemented or accepted. The record
also specifies focused diagnostic additions while preserving the fixed corpus.
Full Tier 1, Linux LSan and overall parity remain open.

[Shared string storage and direct concatenation](cp42-shared-string-storage-2026-09-11-rejected.md) is **rejected**.
It removes temporary concatenation copies and a separate payload allocation;
Unchanged-corpus measurements, allocation/GC/root tests and the exact candidate patch are
preserved. The record also exposes the existing D055 string-allocation precedence
gap. Overall parity, full Tier 1 and Linux LSan remain open.

[Immediate enum construction inside JIT segments](cp42-jit-immediate-enum-2026-09-11-rejected.md) is **rejected**.
Paired unchanged-corpus timing and exact before/after counters connect a shared
VM boundary removal to several workloads. All 32 Native executables remain
byte-identical. Full ON/OFF, focused sanitizer/Rosetta and failure/fuel checks
are recorded. Overall parity, complete Tier 1 and Linux LSan remain open.

[Immediate enums and one checked-index guard](cp42-jit-enum-bounds-2026-09-11.md) is **accepted locally**.
It combines a shared VM boundary removal with the existing array-length invariant.
The unchanged 32-workload comparison, revised user-directed tradeoff policy,
101/87 full tests, sanitizer/Rosetta checks and frame-work counters are retained.
Native executable hashes remain identical. Overall parity and full Tier 1 remain open.

[Parity architecture review and ordered workload work](cp42-parity-architecture-review-2026-09-11.md) records the user-directed text-first queue against accepted `51e0be3`. It distinguishes measured implementation costs from unproven language limits, preserves Native/VM boundaries and all 32 completion requirements, and makes no new speedup claim.

[Retained string bytes in Native, interpreter and JIT](cp42-string-views-2026-09-11.md) is **accepted locally**.
It removes redundant byte validation while preserving original 32 inputs, logical
comparison/allocation counts and exact fuel. Full103/88 tests,20/20 sanitizer and
Rosetta checks, all 32 Native/ON/OFF paired timing and a fresh five-way comparison
are recorded. Three rejected candidates remain archived. Title-search stays
active; all-row parity, known baseline defects and full Tier1 remain open.

[Title costs after retained string bytes](cp42-title-after-string-views-2026-09-11.md) records completed local
phase diagnosis on `78d3070`: shared string creation/storage is the next common
mechanism, subject to D055 preflight/lifetime obligations and a fresh protocol.
About 40 ms of VM source preparation remains a separate dependency. No new
optimization, row closure or parity result is claimed.

[Shared string allocation and budget preflight: rejected candidate](cp42-string-publication-2026-09-11-rejected.md) is **rejected**.
Title-search improves, but the unchanged all 32 gate contains a confirmed Native
record-create regression. Full104/89 tests,21/21 sanitizer/Rosetta checks, exact
allocation counters and every Native/ON/OFF timing remain archived. Production
is restored; D055, title-search parity and complete Tier1 remain open.

[String-publication regression diagnosis](cp42-string-publication-regressions-2026-09-12.md) records two preserved local
diagnostics. Record registration volume and a controlled wrapper replacement
support revising the publication path; OFF Dijkstra's tail consumes extra CPU
inside otherwise unchanged hot-array owners. The rejected candidate stays
rejected. Cache/path counters are the next bounded investigation; production,
title-family closure and full Tier1 status are unchanged.

[Shared strings, publication and recent array views](cp42-string-publication-v2-2026-09-12.md) is **accepted locally**.
It records coallocated string storage with D055 preflight, shared publication
internals and D104 previous-entry-first lookup, full 104/89 tests, 21/21 sanitizer
and Rosetta checks, exact allocation/cache counters and all 32 paired timings.
The original title family, all-32-workload parity and full Tier1 remain open.

[Title costs after string publication](cp42-title-after-publication-2026-09-12.md) records local repeated-entry
profiles with per-return zero allocator ledgers and a refuted Native alias-cache
hypothesis. String-array projection and checked JIT publication remain measured
costs to investigate. Production and all 32/Tier1 parity status are unchanged.

[Native title string-projection screening](cp42-title-projection-probe-2026-09-12.md) passes its predeclared
diagnostic thresholds with unchanged counts and zero post-return allocator
ledgers. It motivates a general descriptor-selected implementation with full
GC and checked identity support; no production optimization or parity status
is accepted by this two-row experiment.

[String-array projection: rejected early](cp42-string-array-projection-2026-09-12-rejected.md) preserves the general
same-stride implementation and its fixed title-pair results. Native improves
15.28% geometrically, but VM+JIT improves only 0.33%, missing its 3% minimum.
Focused 7/6 tests and all 32 Native/ON/OFF correctness pass; full validation and
full32 timing were not run. Production is restored. Checked JIT publication
is the next bounded investigation; accepted all 32/Tier1 status is unchanged.

[String-array projection V2: rejected early](cp42-string-array-projection-v2-2026-09-12-rejected.md) records a separate checked
machine-code projection owner and actual native/C oracle coverage. Focused 8/6
checks and all 32 correctness pass; Native improves 14.47% geometrically, while
VM+JIT shows no useful gain and misses its 3% minimum. Production is restored.
Repeated invariant binding stores are the next hypothesis; no full32/Tier1
acceptance or parity claim follows this rejected fixed series.

[String-array projection V3: rejected early](cp42-string-array-projection-v3-2026-09-12-rejected.md) removes over 98% of
invariant binding-header publications with unchanged compiled string-read counts,
but VM+JIT elapsed time does not improve. Native also misses its 10k row minimum.
Focused 8/6 and all 32 correctness pass; the exact candidate, failed setup logs and
fixed series remain archived. Production is restored. Map anonymous JIT frames
before another instruction change; original all 32/Tier1 parity remains open.

[Mapped accepted title JIT instructions](cp42-title-jit-map-2026-09-12.md) completes the bounded diagnosis.
All 434 anonymous leaf observations map across captured code lifetimes; 512 original
returns and 256 zero-ledger returns pass. The next candidate targets repeated view
checks within a segment and immediate boolean reloads, preserving all VM stores
and exact exits. Production, the accepted all 32 comparison and parity/Tier1 status
remain unchanged.

[Local JIT view/boolean candidate](cp42-jit-local-views-2026-09-12-rejected.md) is **rejected early**: the original
VM title pair improves 2.10%, below its fixed 3% minimum. Focused 19/5 checks and
all 32 correctness pass; Native executables are byte-identical to the accepted
control. The exact candidate and fixed observations remain archived; production
is restored. The next hypothesis concerns invalidation of a copied alias's own
view. Original all 32 parity and complete Tier1 remain open.

[Alias-cache JIT V2](cp42-jit-local-views-v2-2026-09-12-rejected.md) is **rejected early**: the unchanged VM title
pair improves 2.995623%, below its fixed 3% minimum (exact ratio 0.970043769715).
Focused 20/5 checks and all 32 correctness pass; Native executables are identical
to accepted controls. The candidate and failed fixed series remain archived;
production is restored. Next investigate full checked original-Cell string
projection in the JIT. Original all 32 parity and complete Tier1 remain open.

[Inline original-Cell string projection](cp42-jit-cell-projection-2026-09-12-rejected.md) is **rejected early**:
the unchanged VM title pair improves 2.353783%, below its fixed 3% minimum.
Focused 21/5 tests, 108 native/C-oracle cases and all 32 stdout checks pass;
Native executables are identical to accepted controls. The exact candidate and
series are archived and production restored. Next measure stable array identity,
generation and heap-epoch reuse before considering speculative whole-array
child validation. Original all 32 parity and complete Tier1 remain open.

[Stable string-array reuse diagnosis](cp42-string-array-reuse-2026-09-12.md) records 128 unchanged original
results, four-way counter agreement per workload and 64 zero-ledger returns.
An independent source oracle confirms repeated reads of the same title array.
Waiting for twice its length in prior reads avoids one-pass text arrays in the
work-count model. No speedup or new child certificate is accepted; production,
the accepted all 32 comparison and complete Tier1/parity status remain unchanged.

[Adaptive string-array proof: rejected early](cp42-jit-string-array-proof-2026-09-12-rejected.md) preserves the full checked
certificate candidate and fixed 3.55% title-pair slowdown. Focused 18/5 tests,
all 32 outputs and Native byte identity pass. Post-rejection 128-run counters
confirm predicted reuse and 64 zero-ledger returns; the C helper still performs
almost as many hot-path instructions. Production is restored. Next investigate
certified projection inside the JIT segment under fresh fixed gates; accepted
all 32/Tier1 and original parity status remain unchanged.

[Inline certified string-array projection: rejected early](cp42-jit-string-array-proof-v2-2026-09-12-rejected.md) archives the
fixed 1.45% title-pair slowdown and restores production. Focused 18/5 tests,
all 32 outputs and Native byte identity pass. Direct 128-run counters and 64
zero-ledger returns prove 7,894,270 title100k reads bypass C; emitted disassembly
shows complete destination publication remains. Next inspect repeated invariant
metadata writes under the existing entry-epoch proof from accepted production.
The accepted full comparison, original parity and Tier1 status remain unchanged.

[Checked string metadata reuse: rejected early](cp42-jit-string-metadata-2026-09-12-rejected.md) records the fixed 0.62%
title-pair reduction, below the 3% minimum, and restores production. Focused 18/5,
all 32 outputs and Native byte identity pass after a recorded C17 fixture repair.
128 diagnostic runs and 64 zero ledgers prove metadata reuse on 98.76% of title100k
reads. Next test proof-based emitted projection and entry metadata reuse together
under fresh gates, against accepted production. Full parity/Tier1 remain open.

[Combined certified reads and metadata reuse: rejected early](cp42-jit-string-array-proof-v3-2026-09-12-rejected.md) archives the
fixed 0.65% title-pair slowdown and restores production. First-attempt focused
19/5 tests, all 32 outputs and Native byte identity pass. Direct 128-run counters,
64 zero-ledger returns and exact emitted capture prove both mechanisms execute
without the required elapsed-time improvement. Stop isolated bridge variants;
attribute the whole original search loop and source/JIT startup before another
candidate. Generic checked loop composition is a hypothesis, not acceptance.
The accepted full comparison, original parity and Tier1 status remain unchanged.

[Checked string-search loop composition](cp42-jit-string-search-loop-2026-09-13-rejected.md) is **rejected in the full gate**.
The title pair improves 9.33% early, but OFF record-create
P90 exceeds the 5% allowance in both fixed series. Full105/89, sanitizer22,
fresh x86-64/Rosetta22 and all 32 correctness pass; production is restored.
Raw recovery evidence and a fresh unchanged-production five-way table are retained.
Shared runtime string construction is the next measured title-family dependency;
original all 32 parity, complete Tier1 and Linux LSan remain open.

[Shared immutable string bodies](cp42-string-body-sharing-2026-09-13-rejected.md) is **rejected early**.
The title pair is 1.15% slower in Native and
1.07% slower in VM+JIT. Focused 7, the final storage
test and all 32 Native/ON/OFF results pass; production is restored. Actual body
counts and later text-row diagnostics remain evidence, not acceptance.
Next audit repeated MIR verification with explicit proof/rollback ownership;
the original all 32 parity and complete Tier1/LSan requirements remain open.
