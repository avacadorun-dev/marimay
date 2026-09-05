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

N0 has repository-local migration evidence but remains blocked on external
legal/search/control/PATH evidence and an accepted follow-up identity decision.

## I0

- [Build and CLI foundation](i0-iteration-1-2026-08-10.md), [source ownership and positions](i0-iteration-2-2026-08-10.md), [portability/fault injection/CI](i0-iteration-3-2026-08-10.md), and [memory-safety gates](i0-memory-safety-gates-iteration-4-2026-08-13.md).

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
parent product gate; CP41 is the sole ready checkpoint, and CP39–CP41 must
each add completion evidence.

## CP41

[The x86_64 code-generation trigger](cp41-x86-codegen-trigger-2026-08-24.md)
localizes selective stack/queue/heap pressure in generated native C. CP41 is
ready after CP38 and before CP39–CP40 so the installed CLI0 artifact contains
the repair. Its trigger run is too noisy to become a new D056 baseline; a clean
before/after pushed-run evidence pair is still required. Go/Rust ratios remain
exploratory.

## I6 and E0

No direct implementation or external-pilot evidence exists. I6 and E0 remain
planned after CLI0, whose I4 path now includes CP41.

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
