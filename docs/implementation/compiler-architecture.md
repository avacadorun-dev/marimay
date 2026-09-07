# Marimay Stage 0 compiler architecture

Status: implementation architecture for the C17 bootstrap.
Last reviewed: 2026-09-01.

This document maps the normative compiler and execution contracts to physical
owners. It changes no language semantics. When it conflicts with `docs/spec/`
or an accepted design decision, the higher-authority document wins and this
map must be corrected before implementation proceeds.

The supporting external evidence is recorded in
[`../research/compiler-architecture.md`](../research/compiler-architecture.md).

## Central architecture

```mermaid
flowchart LR
  SRC["Source snapshots"] --> LEX["Lossless tokens"]
  LEX --> CST["Lossless CST"]
  CST --> HIR["Resolved HIR"]
  HIR --> THIR["Typed HIR"]
  THIR --> MIR["Typed MIR"]
  MIR --> MV["MIR verifier"]
  MV --> ORACLE["MIR oracle"]
  MV --> BCL["Bytecode lowering"]
  BCL --> BCF["Encoded bytecode"]
  BCF --> BCV["Bytecode verifier"]
  BCV --> VM["VM"]
  MV --> C17["Portable C17 emitter"]
  C17 --> NATIVE["Native executable"]
  VM --> RT["Shared runtime semantics"]
  NATIVE --> RT
```

Typed MIR is the only semantic convergence point. Backends may reject a typed
MIR feature they do not yet implement, but they may not reinterpret types,
evaluation order, bounds, arithmetic, panic identity, cleanup, or managed
identity. A preview feature is incomplete until both engine paths and the
differential harness cover it.

## Owners and allowed dependencies

| Owner | Owns | May depend on | Must not own |
|---|---|---|---|
| `source` | exact bytes, snapshots, line mapping | base storage/UTF-8 | grammar or semantic types |
| `lex` | lossless tokenization and lexical limits | source, diagnostics, generated token/Unicode tables | parser recovery or typing |
| `syntax` / `parse` / `ast` | immutable lossless tree, recovery, semantic projection | lex, source, diagnostics | package resolution or MIR |
| `check` | package phases, symbols, type rules, exhaustiveness, typed-HIR-to-MIR coordination | AST, types, MIR builder, diagnostics | bytecode/native repair or runtime layout policy |
| `mir` | typed representation, builders, verifier, deterministic oracle | base, canonical types, shared numeric semantics | source-name lookup or backend-specific types |
| `bytecode` | opcode metadata, program format, encode/decode, verifier, MIR lowering | verified MIR, generated opcode registry | source semantics or C lowering |
| `vm` | frames and execution of verified bytecode | verified bytecode, shared runtime | type inference or bytecode repair |
| `native` | auditable C17 emission and tool invocation | verified MIR, shared runtime ABI | independent source lowering |
| `runtime` | managed objects, arrays, panic/arithmetic/bounds helpers, later GC/tasks/I/O | base/platform adapters | compiler AST/HIR or engine-specific semantics |
| `cli` | target discovery and reusable service orchestration | compiler/tool libraries | duplicate parser/checker/backend implementations |

Dependencies flow downward through public headers. Private headers expose only
the state needed by sibling files in the same owner. A private header is not a
shortcut for sharing checker internals with a backend.

## Phase contracts

### Frontend to checker

- The CST remains lossless and source-owned.
- AST/HIR projections preserve stable source spans and invalid origins.
- Package checking orders collection, imports/visibility, signatures, and
  bodies deterministically.
- Recovery state can produce diagnostics but cannot be published as valid
  typed HIR or MIR.
- Generic specialization may append to and relocate the function declaration
  table during expression checking. Retain declaration indices across that
  boundary. A body checker borrows a stable metadata snapshot (including type
  binding vector descriptors); name publication transfers ownership from the
  actual indexed declaration, never from that snapshot.

### Checker to MIR

- The checker lowers all implicit observable behavior before publication.
- A `MarimayMirFunction` is either failure-atomically owned by its result or not
  published.
- The builder does not substitute for the verifier; malformed structures built
  by tests must be rejected independently.
- Source-order evaluation, checked operations, bounds, cleanup, and panic spans
  are explicit operations or edges.

### MIR to engines

- `mmay_mir_verify` is required before oracle, bytecode, or native execution.
- Bytecode lowering may return an explicit unsupported-feature result; it must
  not omit an instruction.
- The bytecode decoder/verifier treats bytes as hostile and finishes before VM
  execution.
- The C emitter uses explicit temporaries and runtime helpers where a host C
  expression could introduce undefined, unspecified, locale-dependent, or
  backend-divergent behavior.

### Engines to runtime

- VM and generated C use the same semantic helper contracts for managed arrays,
  checked arithmetic where required, panic identity, and later GC/task/I/O
  behavior.
- Backend frames/addresses are not semantic identities.
- Runtime helpers do not accept unverified type/layout identifiers without
  checking their own public boundary.

## Physical decomposition plan

The current public headers remain stable during behavior-preserving extraction.
New files are linked into the same `mmay_bootstrap` library.

### Enforced module-size contract

Every handwritten production and test translation unit must be understandable
from a bounded context packet: this document, its narrow public/private
contract, the owner implementation, and its focused tests. Requiring an agent
or reviewer to load an unrelated multi-thousand-line file is an architecture
defect.

- Target 500–1,200 physical lines per handwritten `.c`; hard ceiling 1,500.
- Target 50–250 physical lines per owner-private `.h`; hard ceiling 400.
  Owner API headers, including every handwritten header under
  `bootstrap/include/mmay/` and `runtime/include/mmay/`, have a tighter hard
  ceiling of 160. Generated artifacts are the only standing exemption.
- Temporary exceptions live only in `architecture-exceptions.tsv`. They name
  the exact path, owner, explicit ceiling, reason, and removal checkpoint. An
  exception is migration debt, not permission to add feature logic.
- A valid split moves semantic ownership into independently compiled
  translation units. Handwritten `.inc` shards, inclusion of `.c` files,
  include facades, and giant internal headers do not count.
- Line count cannot excuse mixed ownership: a coordinator below the ceiling
  that still implements declarations, expressions, patterns, CFG, and
  publication is a god object and must be divided.
- Stable coordinators have tighter path-specific ceilings in the executable
  gate: checker 650, parser/MIR model/formatter 1,200, and bytecode model,
  VM, and CLI 900 lines. The checker shared internal contract is capped at 250
  lines. These are ceilings, not growth targets.

`tools/check-architecture` enforces the mechanical portion on every push and
pull request. Its policy self-check proves that non-bytecode public headers are
discovered and that 160 lines are accepted while 161 are rejected. Review and
checkpoint evidence enforce cohesive ownership.

### MIR owner

Checkpoint 0 established these independently compiled owners:

```text
bootstrap/src/mir/
  mir.c                 representation lifecycle and public builders
  mir_internal.h        narrow sibling contracts
  verify.c              local, CFG, and initialization verification
  verify_support.c      type/dataflow verifier primitives
  verify_managed.c      string/record/enum/array layout verification
  mir_collections.c     D079 collection-layout lifecycle and exact interning
  execution_collections.c
                        D079 ordered Map/Set oracle state and operations
  verify_range.c        verified array proof-capability validation
  range.c               transactional checked-to-in-bounds MIR rewrite
  integer_range.c       MIR adapter, transactional arithmetic rewrite, proof validation
  verify_interfaces.c   interface metadata and nominal payload verification
  verify_io.c           exact D075 stream instruction/layout verification
  verify_text.c         D076 text, UTF-8 array-layout, and binary-I/O verification
  verify_fs.c           D077 path/fs operation and result-layout verification
  verify_context.c      D078 environment/process result-layout verification
  verify_format.c       D065 primitive-format and concat verification
  verify_parse.c        D065 Result<int, ParseError> layout verification
  execution.c           oracle graph and interpreter dispatch
  execution_array.c     checked and verifier-gated in-bounds array execution
  execution_heap.c      execution state and managed allocations
  execution_io.c        D075 stdin/stdout/stderr oracle and Result construction
  execution_text.c      D076 text transforms and UTF-8 array construction
  execution_fs.c        D077 capability-confined path/fs oracle operations
  fs_internal.h         narrow D077 verifier/oracle result-layout contract
  execution_context.c   D078 injected environment/process oracle operations
  context_internal.h    narrow D078 verifier/oracle result-layout contract
  execution_format.c    locale-independent formatting and counted concat
  execution_parse.c     D065 integer parsing and ParseError construction
```

`mmay_mir_verify` and `mmay_mir_interpret` retain their public signatures. No
representation or runtime semantics change in these extraction commits.

D076 keeps the public representation declarations in `model.h` and moves the
text/binary operation vocabulary to the narrow `operations.h` API. Shared text
verification and execution helpers live in owner-private `text_internal.h`;
backend owners consume verified operations without reconstructing source-level
UTF-8 policy.

### Checker owner

Checkpoint 0 leaves `checker.c` as the package coordinator and gives every
semantic family an independently compiled owner:

```text
bootstrap/src/check/
  checker.c             package coordinator and stable public entry points
  checker_internal.h    common utility/forward contract only
  checker_*_context.h   narrow package/function/match state contracts
  checker_core.c        shared checker operations and type helpers
  checker_lookup.c      deterministic name/type lookup
  checker_lookup_imports.c
                        file-local imported names and D043 owned lookup
  checker_imported_nominal.c
                        imported record/enum MIR layout adaptation
  checker_imported_enums.c
                        concrete imported-enum checker views and payload metadata
  checker_declaration_*.c
                        lifecycle, constants, functions, nominal families,
                        and publication phases
  checker_expression.c  expression dispatcher only
  checker_expr_*.c      scalar, call, nominal, and record expression owners
  checker_compound_types.c
                        tuple/function type recognition and interning
  checker_expr_tuples.c tuple value lowering and recursive MIR layout ownership
  checker_tuple_match.c tuple-pattern binding and match lowering
  checker_range_loop.c  exact D068 integer-range loop CFG lowering
  checker_expr_io.c     exact imported std/io operation/alias recognition and lowering
  checker_expr_fs.c     exact imported std/path and std/fs recognition and lowering
  checker_expr_fs_api.h narrow D077 checker owner contract
  checker_expr_context.c
                        exact imported std/env and std/process primitive lowering
  checker_expr_context_api.h
                        narrow D078 checker owner contract
  checker_collection_types.c
                        compiler-owned Map/Set type recognition and interning
  checker_derive.c      closed D079/D080 Debug/Eq/Hash validation
  checker_equality.c    D080 structural equality typing and explicit MIR CFG
  checker_expr_test.c   D046/D080 compiler-known expectation lowering
  checker_expr_collections.c
                        D079 exact operation typing and MIR lowering
  checker_collection_loop.c
                        insertion-order loop CFG and generation checks
  checker_expr_string.c interpolation plus closed std/convert lowering
  checker_match_*.c     binding, analysis, patterns, witnesses, dispatch,
                        and lowering
  checker_enum_pattern_names.c
                        exact D084 enum pattern qualifier/alias resolution
  checker_cfg.c         block/edge/value-state ownership
  checker_statement.c   lexical statement/control-flow lowering
  checker_publication.c checked-package publication
  checker_interpret.c   checked-result MIR oracle adapter
  checker_*_api.h       one narrow contract per sibling owner
  result.c              result/default/dispose/accessor ownership
  facts.c               canonical definition/reference fact publication
  queries.c             result-scoped definition/search/completion queries
  resolution.c          shared D003/D043 resolver and exact completion facts
  checker_receiver_calls.c
                        receiver-directed package/prelude collection dispatch
  checker_array_types.c D054 prefix-type recognition and existing array interning
  package_graph.c       bounded deterministic local-package graph coordinator
  package_imports_api.h narrow graph/checker import contract
  service.c             snapshots, package-graph checks, frontend lifetimes
  service_queries.c     generation/snapshot-safe compiler-service queries
  type_spelling.c       bounded canonical checked-type display ownership
  array.c               D045 type/literal/access/assignment lowering
  entry.c               root main and structural Error-contract selection
```

`mmay_check_package` remains the single-package coordinator;
`mmay_check_package_graph` owns only canonical graph ordering, import binding,
and repeated package-phase orchestration. One graph check uses one result and
one type table; independently checked package results are never concatenated.
Pattern and statement owners share only explicit context contracts; copying
that state into feature-specific modules is forbidden. Recursive expression
lowering returns through the dedicated dispatcher contract instead of making
a record/scalar owner the implicit hub.

`mmay_compiler_service_check` is the reusable owner above the frontend and
checker. It retains immutable source snapshots, rejects stale overlay versions,
orders current documents by exact path, and owns lex/parse/AST/check teardown.
`mmay_compiler_service_check_request` can reuse immutable top-level parse
subtrees from one exact versioned edit, polls cooperative cancellation between
bounded phases, and still rechecks the whole semantic package. It introduces
no persistent semantic cache and does not duplicate semantic lookup.
Successful diagnostic-free checks publish bounded, result-owned top-level
definition facts and references to that stable symbol universe in canonical
source order. `facts.c` owns publication, occurrence canonicalization, and
cross-fact validation; `queries.c` owns allocation-free result-scoped
definition-from-reference, byte-prefix package-symbol search, and completion
access. Source-path hashing consumes exact bytes, and nominal-receiver lookup
is indexed during declaration collection. Result-local `MarimayTypeId` values do
not cross the query boundary; receiver and candidate identity use stable
`MarimaySymbolId` values.

Package-local and imported non-generic free and dot calls share one resolver in
`resolution.c`: same-name functions are accepted only for distinct exact local
nominal receivers after alias unwrap, both spellings lower to one symbol, and
the dot receiver is evaluated once before explicit arguments. The same live
resolver publishes per-snapshot dot completion after signature collection;
the compiler service never reconstructs candidates by filtering published
function vectors. Canonical package surfaces provide D043 public candidates,
and import bindings contribute only the available named, aliased, namespace,
or empty free spelling. `service.c` maps current documents into one bounded
package-graph check, while `service_queries.c` rejects both older result
generations and snapshots outside the supplied current result. Full semantic
rechecking provides public/private/import-aware invalidation without a hidden
persistent semantic cache.

`checker_receiver_calls.c` owns the narrow collision boundary between ordinary
D003/D043 collection names and compiler-known `[]T` operations. It parses
only receiver-bearing `push` and `length` calls, compiles the receiver once,
asks the live resolver for an exact nominal candidate, and dispatches either to
ordinary call lowering or to `array.c`. Namespace-qualified calls remain with
the import resolver. `array.c` therefore owns only the selected D045 operation;
it does not shadow ordinary functions by spelling, and neither VM nor native
lowering repairs source-level candidate selection.

`checker_array_types.c` owns recognition of the D054 `[]T` AST prefix and
interns the unchanged D045 array descriptor. Public-signature visibility
recurses through that element node rather than treating `Array` as reserved.
`type_spelling.c` renders result-local checked types for compiler-service
hover/signature consumers with explicit depth/byte limits and caller-owned
output; `service_queries.c` supplies current-generation validation. The
formatter adjacency predicate lives in `formatter_types.c`; none of these
owners changes MIR, bytecode, runtime layout, or ABI.

`json_intrinsics.c` owns the immutable D086 package/name/signature registry;
import validation and JSON call selection consume its generic policy and
arity. `intrinsic_completions.c` publishes file-import spellings from the live
checker import environment, owns their bounded result storage and cleanup,
and answers allocation-free prefix queries. It does not invent functions,
source definitions, type IDs or nominal receivers. The existing service query
owner validates generations and snapshots before exposing those facts. Both
ordinary and intrinsic candidates share the completion budget. Result rollback
and teardown delegate intrinsic spelling cleanup to its owner. The focused
`test_compiler_service_intrinsics.c` owner covers imports, descriptors, overlays,
limits, cancellation and every allocation-failure index independently of the
real JSON execution corpus.

The present occurrence universe covers stable top-level type, constant, and
resolved call references. Local bindings, record fields, enum variants,
generic instantiations, effect metadata, and production indexing/LSP transport
remain later I2/I8 work. Public `mmay check` now owns fresh disk-snapshot target
discovery and delegates the authoritative selected graph to this service; it
reports an internal failure instead of success for a checker-incomplete
accepted construct.

### Test owners

Test aggregators now contain only the original ordered suite calls. Focused
translation units mirror production owners:

```text
tests/c-unit/
  test_checker.c                  ordered aggregator
  test_checker_{core,records,enums,enum_match,match_scalar,
                match_control,calls_constants,operators,control,failures}.c
  test_checker_imports.c
  test_checker_imports_determinism.c
  test_checker_imports_support.{h,c}
  test_checker_support.{h,c}
  test_mir.c                      ordered aggregator
  test_mir_{scalar,calls,records,enums,arrays,arrays_rhs}.c
  test_mir_support.{h,c}
  test_bytecode_{model,program,support}.c
  test_bytecode_vm_{scalar,arithmetic,arrays,support}.c
  test_mir_bytecode_{model,programs,support}.c
  test_native_c_emitter_{baseline,arrays,array_access,scalar,matrix,strings,
                         imports,support}.c
  runtime/test_managed_array_access.c
  test_parser_{core,declarations,expressions,incremental,support}.c
  test_parser_array_types.c
  test_formatter_{core,trivia,failures,support}.c
  test_formatter_array_types.c
  test_checker_array_types.c
  test_pre1_surface_differential.c
                        D067-D075 checker/MIR/VM/native integration contract
  test_compiler_service_array_types.c
  test_cli_cp18_array_spelling.c
  test_check_entry.c
  test_cp9_io_pipeline.c
  test_cli_run.c
  test_runtime_io.c
  test_cp31_program_arguments.c
  runtime/test_program_arguments.c
  test_cp32_streams.c             D075 checker/MIR/bytecode/VM vertical owner
  cp32_process_fixture.c          exact counted-stdin process fixture
  test_cp32_process_streams.cmake VM/native three-stream process comparison
  test_cp33_text_bytes.c          D076 text/MIR/bytecode/VM/native vertical owner
  test_native_text_failures.c     generated text all-index allocation teardown
  test_cp33_process_text_bytes.cmake
                        VM/native binary-I/O and iterator-invalidated parity
  test_cp34_fs.c                  D077 runtime/path/capability/allocation owner
  test_cp34_fs_pipeline.c         D077 checker/MIR/bytecode hostile vertical owner
  test_cp34_process_files.cmake   real VM/native filesystem and CLI comparison
  test_cp35_environment.c         D078 runtime/capability/allocation owner
  test_cp35_context_pipeline.c    D078 checker/MIR/bytecode/native hostile owner
  test_cp35_process_context.cmake real env/cwd/relocation/cache/CLI comparison
  test_cp36_collections.c         D079 checker/MIR/bytecode/VM/native vertical owner
  runtime/test_managed_map_set.c  D079 order/hash/GC/allocation property owner
  test_cp36_map_set.cmake         public VM/native Map/Set acceptance owner
  test_cp37_cli_parser.c          D081 standard-package embedding allocation owner
  test_cp37_cli_parser.cmake      exact VM/Release-native CLI process owner
  ../public-alpha/cp37-cli-parser/
                        D081 package/formatter/property/differential corpus
  test_cp38_json.c                D082 JSON-package embedding allocation owner
  test_cp38_json.cmake            hostile-input, limit, and both-engine process owner
  ../public-alpha/cp38-json/
                        D082 explicit-codec/formatter/differential corpus
  ../fuzz/json_fuzz.c             real embedded-package/checker/bytecode/VM fuzz owner
  test_cli_cp39.c                 D083 init/doctor/toolchain/allocation owner
  test_cp39_distribution.cmake    installed native-run/lifecycle/archive owner
```

The existing `mmay_test_mir` and `mmay_test_checker` aggregators retain their
registrations. A new semantic owner may add one adjacent registered entry point
when that keeps new coverage out of an existing hotspot; splitting existing
tests must not delete, rename, reorder, or weaken assertions.

### Remaining Stage 0 owners

D087 leaves instruction-fuel accounting in `bytecode/vm_prepare.c` and host
policy selection in `cli_run_program.c`/`cli_test_program.c`. The shared VM
dispatchers, verifier, memory and capability owners remain unchanged.
`test_bytecode_vm_arithmetic.c` owns finite/unmetered/shared fuel boundary and
cleanup checks; `test_cli_local_execution.cmake` owns process parity for long
finite work and maximum-sized JSON input. It uses explicit test timeouts, not
a relaxed verifier or smaller workload.

The CP42 config performance follow-up keeps opcode-family routing in
`vm_value.c` and live-expression eligibility in `c_emitter_relations.c`.
Dispatch does not probe unrelated execution owners for scalar operations;
relational transfer does not scan definitions of ineligible expressions.
Neither reordering adds shared mutable caches or changes proof ownership.

`cli_test_bytecode.c` owns invocation-scoped preparation of the complete test
program through verified MIR lowering and the verified encode/decode boundary.
`cli_test_program.c` prepares lazily at the first VM test, retains only decoded
code, and disposes it at command teardown. The preparation owner selects and
fully verifies each subsequent entry; the VM's execution verifier is unchanged.
Per-test heaps, arguments, panic state, captures and capabilities remain fresh.
The narrow `cli_test_bytecode.h` contract borrows MIR only during preparation;
it contains no global or persistent cache. `test_cli_test_bytecode.c` owns
entry-switch/byte-identity, hostile-body, all-index allocation-failure and
panic-followed-by-fresh-arguments differential coverage.

`cli_test_native.c` owns the canonical test-symbol table and borrowed child
argument vector. The coordinator retains one native build result per command
and starts a fresh process for each selected symbol. `c_emitter_test_entry.c`
validates the internal, symbol-sorted void-entry table and emits its checked
ordinal selector; it delegates argument construction and cleanup to the
ordinary executable-entry owner. The selector is removed before D066 argument
validation/import and cannot consume a user argument or managed heap budget.
No per-test heap, capture, panic state, result or process is reused.
`test_cli_test_native.c` owns table/argument allocation rollback;
`test_native_c_emitter_test_entry.c` owns hostile tables, host selectors,
argument boundaries and emitter/host allocation-failure teardown.

Checkpoint 0 applies the same rule to every existing hotspot, not only checker
and MIR:

```text
bootstrap/src/parse/       parser coordinator/tree builder, expression/pattern/
                           type grammar, declaration/statement grammar
bootstrap/src/format/      formatter/render coordinator, token collection and
                           import ordering
  formatter_groups.c      CST-derived logical-expression boundaries and
                           statement/document group lifecycle; continuation
                           indentation never leaks across expression owners
bootstrap/src/bytecode/    model/builders, codec, verifier orchestration,
                           instruction verifier, definite-assignment and CFG
                           provenance verifiers, MIR
                           lowering, VM coordinator, VM preflight, and VM value
                           operations
bootstrap/src/native/      C-emission coordinators and focused instruction/
                           descriptor owners; native build orchestration;
                           profile-specific runtime artifact discovery,
                           validation, content identity, and pinned compiler
                           preflight
bootstrap/src/cli/         dispatch/options/help, format actions, target
                           discovery/traversal, checked native build/run,
                           installed doctor, and atomic project initialization
bootstrap/src/lex/         public result lifecycle and scanner/tokenization
bootstrap/src/manifest/    public result lifecycle, parser, and formatter
runtime/common/            managed heap core and focused object families
tests/c-unit/              focused suites mirroring those production owners
```

D059 assigns the generated-C/runtime array boundary specifically to
`bootstrap/src/native/c_emitter_range.c`,
`runtime/common/managed_array_access.c`, and the owner API
`runtime/include/mmay/runtime/array_access.h`. The emitter owner recognizes
only verifier-approved checked/in-bounds instruction pairs and preserves
barrier spans. The runtime owner resolves and validates one exact access; it
does not expose mutable heap state or retain a cell pointer. VM range dispatch
uses the same exact API after obtaining its verified program descriptor.

D060 assigns array representation, descriptor/stride validation, dense raw
packing, zero-initialized growth, and exact storage-byte accounting to
`runtime/common/managed_array_storage.c`. Its narrow owner-private
`managed_array_storage_internal.h` provides the checked-callers-only inlined
byte-copy primitive so the separately linked D059 boundary does not add a
second hot runtime call. The heap core owns slot publication and collection
coordination, access owns the D059 call boundary, and mutation owns D053
sequencing; none duplicates descriptor selection or raw/reference semantics.
`bootstrap/src/native/c_emitter_descriptors.c` and
`bootstrap/src/bytecode/vm_array.c` map their already verified scalar element
types to the same closed runtime layout tags without changing MIR or bytecode
wire identity. Focused storage tests inspect private layout/accounting while
native descriptor tests prove the emitted mapping.

D079 assigns source collection recognition and lowering to the four focused
checker owners above. `mir_collections.c` owns only layout metadata, while
`execution_collections.c` owns the independent semantic oracle.
`bytecode_collections.c`, `bytecode_verify_collections.c`, and
`vm_collection.c` respectively own wire layout metadata, hostile instruction
validation, and managed execution. `c_emitter_collections.c` consumes verified
typed MIR without reconstructing source key eligibility. The runtime boundary
is `mmay/runtime/map.h` plus `mmay/runtime/set.h`: `managed_map.c` owns coherent
hash/equality, insertion order, growth, and precise tracing; the small set
owner delegates storage to that exact map contract and exposes no duplicate
table implementation.

Both Map and Set constructors validate the immutable field descriptor before
publishing an object. Operations rely on that shape invariant, but still
resolve each receiver's heap/slot/generation/kind and validate reference-valued
keys and values. The map owner exposes its descriptor predicate and a fully
checked Set insertion operation through `managed_map_set_internal.h`; no
resolved object or mutable storage crosses that boundary. Set insertion
validates its handle/kind once in that operation, preserving receiver-before-
output error order, and shares the map owner's insertion core. Its local
resolved lookup/core is not an unchecked caller-facing fast path. A guarded
compiler inlining hint keeps sharing that core from adding a call boundary
to Map.put; other C17 compilers retain the ordinary inline implementation.
`runtime/test_managed_set_insertion.c` owns this operation's handle, output,
key, duplicate/limit and lifetime boundaries. Raw 64-bit keys
use the base hash owner's fixed-word SipHash-2-4 entry point, exactly equivalent
to hashing the native word bytes with the generic routine. Round count, seeds,
reference-key hashing, probing, order and storage accounting rules are unchanged.

`managed_map_probe.c` owns the bounded bucket walk, integrity checks and
resumable hash-candidate enumeration behind `managed_map_probe_internal.h`.
It neither allocates nor calls recursive equality. The map owner compares
reference candidates with its existing nominal/string rules and resumes the
same walk; raw keys finish in the leaf probe. Required output locations and
locally initialized progress are owner-private preconditions, not unchecked
source inputs. Public Map/Set APIs still validate handles and output arguments.

The map object may remember one successful raw-key `get`, including absence,
as scalar indices/hash/seed words and its mutation generation. Every use first
resolves the receiver and checks exact key, generation and seeds; storage
publication explicitly invalidates it. Reference keys and Set never populate
this memo. It retains no managed reference or storage pointer and introduces
no allocation; its object bytes are included by the existing `sizeof`-based
accounting. Isolate execution remains serialized under execution §7.9/§8.1;
this internal read memo does not increment source mutation generations.
`runtime/test_managed_map_probe.c` owns synthetic collision/tombstone walks;
`runtime/test_managed_map_cache.c` owns enabled/disabled memo differentials,
alias/mutation, seed, limits, reference lifetime and stale/foreign handle cases.

Collection Option/Result emission uses the existing generated-enum ABI and
entry-registered descriptor IDs for empty/raw-cell results. Reference payloads
retain the rooted checked constructor. Descriptor registration remains in
`c_emitter_descriptors.c`; the collection emitter does not duplicate it or
introduce a runtime map fast path with weaker validation.

`bytecode_verify_assignments.c` owns definite-assignment transfer and bounded
successor worklist scheduling. Its narrow borrowed-storage API receives the
validated predecessor table and fact buffers; the coordinator reuses its
finished predecessor cursors as queue storage. `bytecode_verify_provenance.c`
separately owns the array/enum must-facts and its temporary pending queue.
Both retain the fixed entry boundary, meet every predecessor, and enqueue
successors only when outgoing facts change. Instruction-walk array fact
invalidation uses the same predicate as block transfer, including non-writing
void calls and stores. The focused provenance tests cover reversed block order,
joins, loops, missing/conflicting proofs, barrier restoration and all
deterministic allocation-failure indices. These are existing D058 verification
contracts, not new source syntax or permission to trust producer proofs.

D081 adds no compiler-known semantic owner. `std/cli/cli.my` owns the public
model, factories, definition validation, and stable messages;
`std/cli/parse.my` owns argument consumption and typed usage errors; and
`std/cli/help.my` owns deterministic bounded rendering. The toolchain embeds
those three ordinary documents through the narrow
`cli_standard_cli_package.c` integration owner and passes them through the
same package graph, checker, typed MIR, bytecode VM, and C17 emitter as user
source. No C owner recognizes a `std/cli` symbol or reconstructs its parsing
semantics.

The D081 workload exposed three pre-existing generic-nominal boundary defects.
`checker_match_binding.c` now omits an impossible payload projection for the
wildcard of an erased `Result<void,E>.Ok`; `checker_imported_nominal.c` closes
record/enum layouts over nested Map/Set fields and reacquires vector elements
after recursive appends; and `bytecode_codec_nominal.c` accepts every verified
register type while decoding nominal layouts, including collection fields.
These remain within their existing owners, and focused checker/CP36 codec
regressions prove the repairs independently of the CLI parser.

The original D082 cursor added no compiler-known JSON semantics. `std/json/json.my` owns
the public model, limits, and stable errors; `std/json/decode.my` owns the
strict UTF-8/token/container cursor and iterative unknown-value skipping; and
`std/json/encode.my` owns the poisoned encoder state machine and deterministic
compact form. The narrow `cli_standard_json_package.c` owner embeds those
three ordinary source documents. Application-owned ordinary codec functions
then pass through the unchanged package graph, checker, typed MIR, bytecode VM,
and C17 emitter. JSON has no host parser, dynamic value tree, ambient file
access, or backend-specific opcode.

D086 adds a separate type-directed convenience owner. `checker_expr_json.c`
owns exact package/import/shadowing selection and the four direct-call
boundaries; `checker_json_types.c` owns the closed eligibility walk, privacy,
generic field substitution, and adaptation to existing nominal layouts.
`checker_json_decode.c` and `checker_json_encode.c` independently own typed
record/array cursor expansion. Their narrow sibling contract is
`checker_json_internal.h`; `checker_json_mir.c` owns the ordinary call,
Result/Option, and control-flow construction used by those expansions.
The path extension borrows a stack-local field/index chain during expansion;
the same MIR owner emits ordinary `atField`/`atIndex` calls only on error
edges. Read/write owners restore the chain on every successful or failed
expansion. The source model owns error payloads, independent path copies and
bounded escaped display; no runtime/backend side table tracks context.
The package graph asks only the export predicate from `checker_json_api.h`.
No JSON-specific backend/runtime operation is added. The C allocation/negative
owner is `test_checker_json.c`; the real source-package and both-engine owner
is `test_typed_json.cmake` with `tests/public-alpha/typed-json`.

`test_cp38_json.c` exhausts every deterministic allocation failure in the
three-document embedding boundary. `test_cp38_json.cmake` owns exact public
formatter/check/test/build behavior, hostile-input diagnostic parity, explicit
VM/native capability denial, native heap pressure, and the file-transform
example. `json_fuzz.c` compiles the real
embedded package graph and a bounded driver once, then executes every fuzz
input through verified bytecode and the VM; it neither substitutes a host JSON
parser nor places fuzz-only parsing in production.

The CP38 call graph also exposed a native-emitter resource issue independent
of JSON semantics: functions with ordinary calls but no `defer` reserved the
program-wide maximum suppressed-cleanup stack arrays. The existing native
owners now compute and emit those arrays only for functions that actually
contain a defer operation. `c_emitter_roots.c` owns that MIR scan,
`c_emitter.c` owns the global bound, and the emit/defer owners consume it;
the baseline emitter test pins the call-only case.

A separate CP37 bytecode-fuzz input round-tripped the codec and verifier, then
failed the call-free run-status contract: `FS_CLOSE_BEST_EFFORT` is a void
opcode, so `vm_fs.c` must dispatch it before Result-layout intern. The repair
stays in that VM owner; `test_cp34_fs_pipeline.c` and the bytecode fuzz corpus
pin the saved input.

D061 assigns capacity rounding, checked maximum clamping, rooted collection
retry, resize accounting, and failure-atomic commit to
`runtime/common/managed_array_capacity.c`. Construction remains in the storage
owner and deliberately bypasses this planner when its final count is known.
Mutation supplies only the requested minimum and any temporary reference root;
it does not duplicate rounding or retain resolved storage. Bytecode and native
construction/reserve paths converge on the same runtime owners, so neither
backend owns a shadow capacity policy.

D062 assigns representation-independent interval lattice math, branch
refinement, and bounded widening to `bootstrap/src/base/integer_range.c` behind
the narrow `mmay/base/integer_range.h` owner API. MIR adaptation,
checked-to-proven transactional rewrites, and MIR proof validation live in
`bootstrap/src/mir/integer_range.c`; hostile serialized-opcode reconstruction
lives separately in
`bootstrap/src/bytecode/bytecode_verify_integer_range.c`. The bytecode and MIR
owners translate their own models but share every arithmetic transfer/proof
rule. Loop-header narrowing retains already finite endpoints when a candidate
would tighten them again; it must not count through a 64-bit interval to reach
the work limit. Bytecode adaptation consults the opcode-effects owner before
treating operand zero as a destination: void-call table indices and stores are
not scalar-register writes. Focused convergence tests exercise real loop CFGs
under a small deterministic work budget and sweep allocation failures.
VM/MIR/native execution only consumes already verified proven forms and
uses the portable unsigned-domain helpers in `checked_i64.c`; no backend grants
or repairs a proof.

D063 assigns activation-view declaration and companion-state emission to
`bootstrap/src/native/c_emitter_emit.c`, array access use to
`bootstrap/src/native/c_emitter_range.c`, array mutation use to
`bootstrap/src/native/c_emitter_array_mutation.c`, and record projection use to
`bootstrap/src/native/c_emitter_managed.c`. The runtime bind/access owners are
`runtime/common/managed_array_access.c` behind the narrow
`mmay/runtime/array_view.h` contract and `runtime/common/managed_record.c`
behind `mmay/runtime/record.h`. Views contain no copied storage address and do
not own memory. The emitter coordinator owns the conservative transitive
safepoint predicate and root-location placement because those are activation
construction decisions; runtime owners do not infer compiler liveness.
`mmay/base/checked_i64_fast.h` is a header-only emission boundary mirroring the
portable checked/proven implementation, not a second semantic arithmetic
owner. `bootstrap/src/native/c_emitter_relations.c` owns the backend-local,
conservative CFG proof for impossible checked-array failures. Its strict-order,
nonnegative, and current-value equality facts are intersected at joins;
dependent expression facts are discarded on operand redefinition, and a
branch-selected index is accepted only when every copy source is proved at its
assignment point. This proof changes neither typed MIR nor source semantics and
falls back to the exact checked path on ambiguity or resource failure.

D066 assigns bounded counted-UTF-8 validation and managed `[]string`
construction to `runtime/common/program_arguments.c` behind
`mmay/runtime/program_arguments.h`. POSIX byte import and Windows wide-CRT
conversion remain separate platform owners in
`runtime/platform/{posix,windows}/program_arguments.c`. The checker entry owner
selects only the accepted root signatures;
`bootstrap/src/bytecode/vm_arguments.c` constructs the verified VM parameter;
and `bootstrap/src/native/c_emitter_entry.c` owns the host scaffold, argument
construction, common cleanup, and public status/result mapping. The emitter
coordinator only sequences that boundary. `bootstrap/src/cli/cli_program_arguments.c` owns separator-value
validation shared by run and test. The focused runtime and CP31 CLI suites
cover limits, encoding, allocation rollback, VM/native argument identity,
result/status precedence, and lexical cleanup without adding process semantics
to a backend.

The exact filenames may follow the stable concepts already present in the
code, but responsibility boundaries and dependency direction are fixed. A
single broad `util.c` or `support.c` that merely relocates unrelated code is
not an owner.

## Extraction checkpoint contract

Every physical extraction must satisfy all of the following:

1. no source, MIR, bytecode, runtime, diagnostic, or CLI semantics change;
2. public headers remain unchanged unless an ownership defect makes that
   impossible and the change is reviewed separately;
3. the diff is mechanical enough to compare moved bodies with their originals;
4. focused tests for the extracted owner pass;
5. the full C unit suite passes in debug and release;
6. ASan+UBSan passes for an extraction touching allocation or execution;
7. generated freshness and documentation checks pass;
8. roadmap/evidence records exact commands, environment, limitations, and
   commit identity.

An extraction is not combined with a new language feature. If a feature needs
an internal interface first, land and verify the interface extraction, then
land the vertical feature slice.

## Vertical feature contract

Each new preview surface follows the same path:

```text
syntax/formatter fixture
  -> checker pass/fail and diagnostic
  -> verified typed MIR and oracle result
  -> encoded/verified bytecode and VM result
  -> generated C17 debug/release and native result
  -> VM/native output, result bits, and panic identity comparison
```

The first implementation checkpoint for a surface may stop at MIR only when it
is explicitly named an internal foundation and leaves all public CLI claims
unavailable. The next checkpoint priority is then backend completion, not an
unrelated broad source feature.

## Pass and analysis discipline

The preview needs only a small sequential pass driver. Each future MIR pass
nevertheless declares:

- function or module scope;
- required verified input state;
- analyses read;
- analyses preserved or invalidated;
- whether observable source spans or panic identities can change;
- a deterministic dump before/after in test mode.

Debug/test pipelines verify after every transform. Persistent query caching is
deferred until session-local `PackageId`/`ItemId`/`FunctionId`, content
fingerprints, dependency edges, and invalidation fixtures exist. No hidden
process-global mutable compiler state is introduced in the interim.

## Checkpoint 0 hotspot inventory

As measured on 2026-08-14 immediately before the full extraction:

| File | Lines | Mixed responsibilities |
|---|---:|---|
| `bootstrap/src/check/checker.c` | 15,112 | declarations, expressions, patterns, statements/CFG, package coordination |
| `tests/c-unit/test_checker.c` | 8,271 | all checker domains and failure injection |
| `bootstrap/src/mir/mir.c` | 5,373 | model/builders, verifier, managed oracle, interpreter |
| `bootstrap/src/bytecode/bytecode.c` | 3,363 | model, codec, verifier, disassembly |
| `tests/c-unit/test_mir.c` | 3,396 | scalar, verifier, calls, records, enums, oracle |
| `tests/c-unit/test_bytecode_vm.c` | 3,152 | every VM value family and failure mode |
| `bootstrap/src/parse/parser.c` | 2,992 | event engine, all grammar, tree builder, incremental parsing |
| `tests/c-unit/test_native_c_emitter.c` | 2,737 | all emitter and differential families |
| `tests/c-unit/test_parser.c` | 2,304 | all parser grammar, recovery, incremental, limits |
| `bootstrap/src/cli/cli.c` | 2,248 | options/help, format actions, target discovery |
| `tests/c-unit/test_mir_bytecode.c` | 2,003 | all lowering/program differential families |
| `tests/c-unit/test_bytecode.c` | 1,971 | model, codec, verifier, program, allocation tests |
| `bootstrap/src/bytecode/vm.c` | 1,865 | preflight, frames, values, strings, arrays, dispatch |
| `bootstrap/src/format/formatter.c` | 1,596 | collection/import ordering and document emission |
| `tests/c-unit/test_formatter.c` | 1,545 | fixtures, properties, limits, failure injection |

These counts triggered checkpoint 0. The 1,500-line ceiling is a hard automated
backstop, and cohesive ownership with a narrow private API is the human review
criterion. Both must pass.

### Checkpoint 0 result

The completed local tree has focused production/platform and unit/fuzz C
translation units behind the owner boundaries below. The largest handwritten
production or test file is 1,304 lines; the largest owner-private header is 333 lines; the largest
checker owner API is 139 lines. The exception registry is empty. No handwritten
`.inc` shard or `.c` inclusion is used.

A post-checkpoint audit found that the first gate revision applied the public
owner-header ceiling only to the bytecode include subtree. The corrected gate
walks both complete public include trees. The former aggregate bytecode, MIR,
checker/service, type-system, and managed-runtime headers are now small
backwards-compatible umbrellas over self-contained semantic owner headers; the
largest handwritten public owner header is exactly 160 lines. This closes the
mechanical blind spot without changing ABI or source semantics.

| Owner family | Largest before | Largest after | Resulting boundary |
|---|---:|---:|---|
| checker | 15,112 | 1,137 | coordinator, declarations, expression dispatcher/families, match families, CFG/statements, publication |
| checker tests | 8,271 | 1,175 | exact-order aggregator plus focused semantic suites |
| MIR | 5,373 | 1,108 | model/builders, verifier families, oracle, execution heap |
| MIR tests | 3,396 | 1,089 | scalar/calls/nominals/arrays and shared fixtures |
| bytecode + VM | 3,363 | 1,017 | model, codec, verifier families, program/lowering, VM preflight/value/dispatch |
| bytecode + VM tests | 3,152 | 1,072 | model/program/scalar/arithmetic/array suites |
| parser | 2,992 | 1,150 | event/tree coordinator, expression/pattern/type grammar, declaration/statement grammar |
| parser tests | 2,304 | 918 | core, declarations, expressions, incremental/limits |
| native emitter | 1,343 | 525 | coordinator/descriptors, validation, instruction/function emission |
| native emitter tests | 2,737 | 867 | baseline, arrays, scalar/f64, matrix, strings/panic |
| CLI | 2,248 | 826 | dispatch/options, format actions, target discovery |
| formatter | 1,596 | 1,115 | rendering coordinator and collector/import ordering |
| lexer | 1,231 | 1,161 | public lifecycle and scanner/tokenization |
| manifest | 1,284 | 698 | public lifecycle, parser, formatter |

## Extraction progress

The first behavior-preserving extraction landed after the managed-array oracle
checkpoint:

| Owner | Before | After | Boundary |
|---|---:|---:|---|
| `tests/c-unit/test_mir.c` | 4,612 | 3,396 | scalar, verifier, calls, records, enums, strings, and `f64`; remains the registered aggregator |
| `tests/c-unit/test_mir_arrays.c` | 0 | 1,251 | array layouts, construction, provenance, bounds, allocation failure, limits, and D045 RHS ordering |
| `bootstrap/src/check/checker.c` | 14,559 | 14,299 | package/checking phases remain coordinated here; published-result ownership moved out |
| `bootstrap/src/check/result.c` | 0 | 263 | defaults, nested published-value cleanup, rollback helpers, public dispose, and const accessors |
| `bootstrap/src/check/checker.c` | 14,299 | 14,246 | owner-private checker/function/expression contexts moved to the sibling-module contract |
| `bootstrap/src/check/checker_internal.h` | 21 | 121 | result lifecycle plus minimal expression-lowering context and helper boundary |
| `bootstrap/src/check/checker.c` | 14,246 | 14,481 | `[]T` type/signature integration and expression/call/assignment dispatch remain in the coordinator |
| `bootstrap/src/check/array.c` | 0 | 1,371 | D045 layout, literal, index, selected `length`/`push`, and checked-store lowering behind the private boundary |
| `bootstrap/src/check/checker_receiver_calls.c` | 0 | 210 | exact receiver-directed ordinary/Array candidate dispatch with once-only receiver evaluation |
| `bootstrap/src/check/checker_internal.h` | 121 | 153 | bounded D045 sibling-module contract; still uninstalled and owner-private |
| `tests/c-unit/test_checker_arrays.c` | 0 | 548 | source array typing, matrix oracle, G4002, diagnostics, limits, and allocation-failure ownership |
| `bootstrap/src/check/service.c` | 0 | 839 | versioned snapshots, exact edit proofs, request-local parse reuse, phase-boundary cancellation, canonical package recheck, and transactional teardown |
| `tests/c-unit/test_compiler_service.c` | 0 | 616 | multi-file order, retained overlays, stale versions, aggregate limits, transactional reuse, and fail-every-allocation cleanup |
| `tests/c-unit/test_compiler_service_incremental.c` | 0 | 869 | exact edit validation, parse reuse/fallback, multi-file semantic recheck, cancellation, result provenance/lifetimes, and fact stability |
| `bootstrap/src/check/checker.c` | 14,481 | 14,898 | bounded fact seeds, receiver-aware identity, and indexed local-nominal ownership remain package-phase coordination |
| `bootstrap/src/check/facts.c` | 0 | 159 | fact-name ownership, canonical ordering, collision rejection, and symbol-ID lookup index |
| `bootstrap/src/check/result.c` | 263 | 340 | definition-fact/package-name lifetime plus exact declaration accessors |
| `tests/c-unit/test_checker_facts.c` | 0 | 804 | five-kind inventory, canonical order, exact identity, limits, diagnostics, and allocation cleanup |
| `bootstrap/src/check/queries.c` | 0 | 168 | allocation-free result-scoped occurrence, definition, prefix-search, and completion access |
| `bootstrap/src/check/service_queries.c` | 0 | 171 | current-generation and exact-snapshot validation around result queries |
| `tests/c-unit/test_compiler_service_queries.c` | 0 | 839 | import-form completion/resolver equivalence, reverse order, overlays, stale safety, cancellation, limits, and fail-every-allocation cleanup |
| `tests/c-unit/test_receiver_collection_names.c` | 0 | 373 | same-name nominal/Array behavior, diagnostics, MIR/backend selection, and fail-every-allocation cleanup |
| `tests/c-unit/test_receiver_collection_service.c` | 0 | 419 | import-form completion/free-spelling/reference identity and compiler-service allocation cleanup |
| `tests/c-unit/test_cli_cp16_receiver_names.c` | 0 | 107 | public fmt/check/run/test-both/build/artifact acceptance for the checkpoint-16 corpus |
| `bootstrap/src/check/checker_array_mutation.c` | 0 | 522 | D053 typed mutation results, guards, and iterator-generation MIR lowering |
| `bootstrap/src/mir/execution_array.c` | 0 | 371 | alias-safe array capacity/mutation oracle and exact G4002/G4005 panics |
| `bootstrap/src/bytecode/vm_array_mutation.c` | 0 | 162 | verified mutation/generation opcode execution and G4005 mapping |
| `bootstrap/src/native/c_emitter_array_mutation.c` | 0 | 152 | portable-C mutation/generation emission and exact G4005 runtime helper |
| `runtime/common/managed_array_mutation.c` | 0 | 397 | failure-atomic storage mutation, reservation, generation, and precise slot release |
| `tests/c-unit/test_checker_array_mutation.c` | 0 | 130 | typed surface/MIR shape, diagnostics, and fail-every-allocation cleanup |
| `tests/c-unit/test_mir_array_mutation.c` | 0 | 110 | aliased MIR state plus exact iterator-invalidated message/span |
| `tests/c-unit/test_bytecode_vm_array_mutation.c` | 0 | 264 | malformed/round-trip/state/G4005 bytecode and VM allocation coverage |
| `tests/c-unit/test_native_c_emitter_array_mutation.c` | 0 | 115 | emitted runtime calls, strict-C execution, and allocation cleanup |
| `tests/c-unit/runtime/test_managed_mutation.c` | 0 | 361 | raw/reference/nested tracing, capacity, generation, and resize-failure coverage |
| `bootstrap/src/check/checker_array_types.c` | 0 | 73 | D054 prefix AST recognition and existing managed-array type interning |
| `bootstrap/src/format/formatter_types.c` | 0 | 15 | canonical D054 prefix adjacency without semantic rewriting |
| `bootstrap/src/check/type_spelling.c` | 0 | 253 | bounded canonical checked-type spelling for compiler-service consumers |
| `bootstrap/src/check/service_queries.c` | 171 | 214 | current-generation wrapper and status mapping for owned canonical type spelling |
| `tests/c-unit/test_checker_array_types.c` | 0 | 216 | identity, contextual empty arrays, migration diagnostics, nominal-name coexistence, and allocation cleanup |
| `tests/c-unit/test_compiler_service_array_types.c` | 0 | 194 | canonical nested/generic signatures, stale results, limits, and output allocation cleanup |
| `tests/c-unit/test_cli_cp18_array_spelling.c` | 0 | 132 | public fmt/check/run/test-both/build/G4002 differential acceptance |
| `tests/c-unit/test_cli_cp17_array_mutation.c` | 0 | 135 | public fmt/check/run/test-both/build/artifact and VM/native G4005 acceptance |
| `bootstrap/src/parse/parser_match.c` | 0 | 94 | D067 block-arm tail-expression recognition without a contextual transfer statement |
| `bootstrap/src/parse/parser_closure.c` | 0 | 105 | closure lookahead and optional contextual parameter/result annotations |
| `bootstrap/src/check/checker_range_loop.c` | 0 | 287 | D068 exact-once integer bounds, immutable binding, loop CFG, and inclusive-maximum safety |
| `bootstrap/src/check/checker_compound_types.c` | 0 | 125 | parenthesized tuple/function type recognition and deterministic interning |
| `bootstrap/src/check/checker_expr_tuples.c` | 0 | 262 | tuple construction plus recursive tuple layout availability across function boundaries |
| `bootstrap/src/check/checker_tuple_match.c` | 0 | 417 | tuple destructuring bindings, exhaustiveness, scoped projection, and typed arm joining |
| `bootstrap/src/format/formatter_layout.c` | 0 | 221 | D071 required singleton-tuple commas plus D073 blank-line preservation, boolean-chain breaks, and compact index grouping |
| `tests/c-unit/test_pre1_surface_differential.c` | 0 | 254 | D067-D075 positive integration, ignored-Result and range-value negatives, MIR oracle, VM, and native parity |
| `bootstrap/src/cli/cli_display.c` | 0 | 61 | bounded UTF-8-safe command-argument display escaping shared by diagnostics and formatter reporting |
| `bootstrap/src/cli/cli_heap_limit.c` | 0 | 83 | D055 decimal range, default, duplicate, and argument-location policy |
| `bootstrap/src/cli/cli_resource_report.c` | 0 | 48 | D055 classified human/short/JSON runtime-resource rendering shared by run and test |
| `runtime/common/managed_resource.c` | 0 | 53 | deterministic managed budget/host failure publication and read-only inspection |
| `tests/c-unit/runtime/test_managed_resource.c` | 0 | 262 | D055 exact boundary, raw/reference growth, semantic precedence, host-failure sweep, and zero-counter cleanup |
| `tests/c-unit/test_cli_heap.c` | 0 | 259 | D055 CLI range/help/JSON, test isolation, embedding/cache identity, and VM/native exact observations |
| `tests/c-unit/test_cli_cp20_algorithm_corpus.c` | 0 | 121 | reusable eight-algorithm fmt/check/VM/test-both/debug/release native acceptance with exact output parity |
| `runtime/common/managed_array_access.c` | 0 | 207 | D059 one-resolve exact descriptor/length/checked-or-proven cell access ABI |
| `bootstrap/src/native/c_emitter_range.c` | 42 | 225 | D059 barrier/access fusion, exact ABI emission, and G4002 span preservation |
| `tests/c-unit/runtime/test_managed_array_access.c` | 0 | 202 | D059 raw/reference, bounds, alias, stale-handle, safepoint, and no-allocation runtime coverage |
| `tests/c-unit/test_native_c_emitter_array_access.c` | 0 | 225 | D059 direct/checked-proof/static-proof generated-C inspection and O0/O2 execution |
| `runtime/common/managed_array_storage.c` | 0 | 540 | D060 descriptor validation, exact allocation/accounting, publication, and failure-atomic dense growth |
| `runtime/common/managed_array_storage_internal.h` | 0 | 82 | D060 C17 inline byte-copy primitive used only after descriptor/value/index validation |
| `runtime/common/managed_array_access.c` | 207 | 210 | D060 dense exact-access transfer while preserving the D059 one-resolve ABI |
| `runtime/common/managed_array_mutation.c` | 397 | 266 | D060 dense mutation transfer and no-op-capacity fast path without changing growth policy |
| `tests/c-unit/runtime/test_managed_array_storage.c` | 0 | 407 | D060 layout/bits/alignment/accounting/tracing/corruption/failure-atomic storage coverage |
| `tests/c-unit/test_bytecode_vm_array_storage.c` | 0 | 168 | unchanged generic-array wire round trip plus VM descriptor/bit mapping |
| `tests/c-unit/test_native_c_emitter_array_storage.c` | 0 | 185 | every raw stride, canonical f64, reference fallback, and strict-C execution |
| `runtime/include/mmay/runtime/array_view.h` | 0 | 148 | D063 non-escaping array-view ABI and validated C17 inline RAW64 access primitives |
| `bootstrap/include/mmay/base/checked_i64_fast.h` | 0 | 156 | D063 portable inline mirror of checked/proven i64 emission helpers |
| `runtime/common/managed_array_access.c` | 417 | 428 | D063 exact view bind plus growth-aware access and mutation without copied storage addresses |
| `runtime/common/managed_record.c` | 186 | 215 | D063 exact non-moving record-view bind |
| `bootstrap/src/native/c_emitter_emit.c` | 588 | 879 | D063 companion-view declarations, conservative transitive safepoint proof, and root-location placement |
| `bootstrap/src/native/c_emitter_managed.c` | 383 | 520 | D063 record-view projection plus mutation-capability-scoped identity guard for alias-replaced array fields |
| `tests/c-unit/runtime/test_managed_array_access.c` | 202 | 300 | D063 rooted collection, growth, allocation-free view, atomic failure, and stale/descriptor coverage |
| `tests/c-unit/runtime/test_managed_record_view.c` | 0 | 95 | D063 exact record view, rooted collection, no-allocation, wrong-type, and stale-handle coverage |
| `tests/c-unit/test_native_c_emitter_managed_views.c` | 0 | 118 | D063 nested record-array view, alias replacement, direct RAW64, rootless emission, and O0/O2 execution |
| `bootstrap/src/native/c_emitter_relations.c` | 1,040 | 1,273 | D063 bit-packed, path-intersected current-value equality and branch-selected array-index proofs with conservative invalidation |
| `tests/c-unit/test_native_c_emitter_relations.c` | 0 | 158 | D063 repeated-expression and branch-selection elimination plus retained checks after unsafe selection or immediate/delayed operand redefinition |
| `runtime/common/program_arguments.c` | 0 | 113 | D066 counted UTF-8 validation and failure-atomic managed `[]string` construction |
| `runtime/platform/posix/program_arguments.c` | 0 | 109 | D066 bounded POSIX argv import and owned UTF-8 views |
| `runtime/platform/windows/program_arguments.c` | 0 | 146 | D066 bounded wide-CRT validation and strict UTF-16-to-UTF-8 import |
| `bootstrap/src/bytecode/vm_arguments.c` | 0 | 43 | D066 verified entry-array descriptor binding for VM execution |
| `bootstrap/src/cli/cli_program_arguments.c` | 0 | 114 | D066 shared run/test separator-value validation and diagnostics |
| `tests/c-unit/runtime/test_program_arguments.c` | 0 | 253 | D066 validation, Unicode/platform conversion, managed values, and allocation cleanup |
| `tests/c-unit/test_cp31_program_arguments.c` | 0 | 556 | D066 run/test/build/native arguments, status, Result, panic, cleanup, diagnostics, and allocation coverage |
| `bootstrap/main.c` | 61 | 68 | D075 CLI host startup suppresses POSIX broken-pipe termination before typed stream execution |
| `runtime/common/io.c` | 78 | 110 | D075 bounded read/short-write/retry/flush core with exact EOF, CRLF, UTF-8, and failure classification |
| `runtime/include/mmay/runtime/io.h` | 106 | 116 | D075 typed transfer callback and independent stdin/stdout/stderr sink contract |
| `runtime/include/mmay/platform/process.h` | 68 | 80 | exact counted-stdin process-capture API used by cross-engine fixtures |
| `runtime/platform/posix/process.c` | 410 | 472 | deadlock-free exact-stdin child capture through an unlinked temporary file |
| `runtime/platform/windows/process.c` | 529 | 610 | inherited delete-on-close exact-stdin child capture for the Windows owner |
| `tests/c-unit/test_runtime_io.c` | 325 | 474 | D075 EOF/LF/CRLF/NUL/UTF-8/limit, partial/retry/failure, and allocation coverage |
| `tests/c-unit/test_cp32_streams.c` | 0 | 743 | D075 checker/MIR/hostile bytecode/VM/native capability/stream/allocation vertical coverage |
| `tests/c-unit/cp32_process_fixture.c` | 0 | 14 | exact binary stdin echo plus independent stderr process observation fixture |
| `tests/c-unit/test_cp32_process_streams.cmake` | 0 | 122 | VM/native status/stdout/stderr parity including invalid bytes, limits, EOF, CRLF, and closed pipe |
| `bootstrap/src/base/text.c` | 0 | 160 | D076 counted UTF-8 byte/rune/grapheme queries and exact split/trim primitives |
| `bootstrap/src/mir/verify_text.c` | 0 | 119 | D076 text operation and result-layout verification |
| `bootstrap/src/mir/execution_text.c` | 0 | 404 | D076 oracle text transforms and managed result construction |
| `bootstrap/src/bytecode/bytecode_verify_text.c` | 0 | 143 | D076 hostile bytecode text-operation verification |
| `bootstrap/src/bytecode/bytecode_verify_terminator.c` | 0 | 92 | independently compiled terminator verifier extracted to retain the owner ceiling |
| `bootstrap/src/bytecode/mir_lower_text.c` | 0 | 67 | D076 verified MIR-to-bytecode text lowering |
| `bootstrap/src/bytecode/vm_text.c` | 0 | 465 | D076 VM text transforms and managed result construction |
| `bootstrap/src/native/c_emitter_text.c` | 0 | 190 | D076 portable-C text helper and instruction emission |
| `tests/c-unit/test_cp33_text_bytes.c` | 0 | 696 | D076 randomized UTF-8, hostile MIR/bytecode, VM/native differential, and allocation coverage |
| `tests/c-unit/test_cp33_process_text_bytes.cmake` | 0 | 108 | VM/native binary stdin/stdout and iterator-invalidated parity |
| `runtime/common/fs.c` | 0 | 865 | D077 lexical normalization, capability matching, handle ownership, bounded I/O, atomic publication, metadata, and deterministic directories |
| `runtime/include/mmay/runtime/fs.h` | 0 | 133 | D077 closed runtime filesystem request/result/capability contract |
| `bootstrap/src/check/checker_expr_fs.c` | 0 | 173 | D077 exact std/path and std/fs call recognition and typed MIR lowering |
| `bootstrap/src/mir/verify_fs.c` | 0 | 249 | D077 exact filesystem operation and result-layout verification |
| `bootstrap/src/mir/execution_fs.c` | 0 | 597 | D077 MIR-oracle path/filesystem execution and managed result construction |
| `bootstrap/src/bytecode/bytecode_verify_fs.c` | 0 | 315 | D077 hostile bytecode filesystem-operation verification |
| `bootstrap/src/bytecode/mir_lower_fs.c` | 0 | 42 | D077 verified MIR-to-bytecode filesystem lowering |
| `bootstrap/src/bytecode/vm_fs.c` | 0 | 676 | D077 VM filesystem execution and managed result construction |
| `bootstrap/src/native/c_emitter_fs.c` | 0 | 338 | D077 portable-C capability roots, filesystem helpers, and instruction emission |
| `tests/c-unit/test_cp34_fs.c` | 0 | 692 | D077 path/capability/partial-I/O/atomicity/resource/allocation-failure coverage |
| `tests/c-unit/test_cp34_fs_pipeline.c` | 0 | 250 | D077 hostile MIR/bytecode verification plus FS_CLOSE_BEST_EFFORT fuzz regression |
| `tests/c-unit/test_cp34_process_files.cmake` | 0 | 208 | real VM/native capability, symlink, atomic file, cleanup, CLI, and output parity |
| `runtime/common/env.c` | 0 | 209 | D078 portable-name grants, denied-before-host lookup, bounded UTF-8 values, and owned cleanup |
| `runtime/common/process_context.c` | 0 | 171 | D078 injected/default cwd and executable identity normalization and stable errors |
| `runtime/include/mmay/runtime/env.h` | 0 | 76 | D078 environment capability and result contract |
| `runtime/include/mmay/runtime/process_context.h` | 0 | 59 | D078 injected process-context contract |
| `runtime/include/mmay/platform/environment.h` | 0 | 36 | counted host environment lookup adapter boundary |
| `runtime/platform/posix/environment.c` | 0 | 42 | POSIX exact-name environment lookup adapter |
| `runtime/platform/windows/environment.c` | 0 | 112 | Windows UTF-16 and ASCII-case-insensitive environment lookup adapter |
| `bootstrap/src/check/checker_expr_context.c` | 0 | 159 | D078 exact std/env and std/process primitive recognition and typed MIR lowering |
| `bootstrap/src/mir/verify_context.c` | 0 | 134 | D078 exact environment/process operation and nominal-result verification |
| `bootstrap/src/mir/execution_context.c` | 0 | 237 | D078 injected MIR-oracle environment/process execution and result construction |
| `bootstrap/src/bytecode/bytecode_verify_context.c` | 0 | 159 | D078 hostile bytecode environment/process verification |
| `bootstrap/src/bytecode/mir_lower_context.c` | 0 | 31 | D078 verified MIR-to-bytecode context lowering |
| `bootstrap/src/bytecode/vm_context.c` | 0 | 319 | D078 VM environment/process execution and managed result construction |
| `bootstrap/src/native/c_emitter_context.c` | 0 | 208 | D078 native grants, context helpers, and portable-C instruction emission |
| `tests/c-unit/test_cp35_environment.c` | 0 | 383 | D078 name/value/error/path and exhaustive allocation-failure coverage |
| `tests/c-unit/test_cp35_context_pipeline.c` | 0 | 560 | D078 exact checker signatures, injected MIR/VM oracles, hostile MIR/bytecode, native redaction, and exhaustive allocation coverage |
| `tests/c-unit/test_cp35_process_context.cmake` | 0 | 283 | real VM/native env timing, cwd, relocation, cache-input, redaction, and CLI parity |
| `bootstrap/src/check/checker_collection_types.c` | 0 | 108 | compiler-owned D079 Map/Set recognition and deterministic type interning |
| `bootstrap/src/check/checker_derive.c` | 0 | 120 | closed Debug/Eq/Hash validation without arbitrary user execution |
| `bootstrap/src/check/checker_equality.c` | 0 | 534 | D080 equatability checks and explicit scalar/record/enum MIR CFG lowering |
| `bootstrap/src/check/checker_expr_test.c` | 0 | 417 | D046 output capture plus D080 typed expectation lowering |
| `tests/c-unit/test_checker_equality.c` | 0 | 67 | counted string, enum, Option, derived-record equality and rejection coverage |
| `tests/c-unit/test_checker_expectations.c` | 0 | 117 | typed expectation execution, G4004 span/message, scope, arity, and type coverage |
| `bootstrap/src/check/checker_expr_collections.c` | 0 | 1,122 | exact Map/Set operation typing, key eligibility, Result/Option construction, and typed MIR lowering |
| `bootstrap/src/check/checker_collection_loop.c` | 0 | 291 | insertion-order tuple/value loops and exact mutation-generation guard lowering |
| `bootstrap/src/mir/mir_collections.c` | 0 | 68 | collection layout lifecycle and exact interning |
| `bootstrap/include/mmay/mir/instruction_kinds.h` | 0 | 94 | append-only MIR operation vocabulary, separate from operand and control-flow structures |
| `bootstrap/src/mir/verify_collections.c` | 0 | 158 | D079 exact receiver, operands, and prelude Option/Result layout verification |
| `bootstrap/src/bytecode/bytecode_opcode_effects.c` | 0 | 141 | register-definition effects consumed by assignment and provenance analyses |
| `tests/c-unit/test_collection_native_failures.c` | 0 | 79 | generated-C collection allocation-failure replay with full zero-counter teardown |
| `bootstrap/src/mir/execution_collections.c` | 0 | 470 | independent ordered collection oracle, mutation, and exact G4005 behavior |
| `bootstrap/src/bytecode/bytecode_collections.c` | 0 | 58 | append-only typed collection layout table ownership |
| `bootstrap/src/bytecode/bytecode_verify_collections.c` | 0 | 188 | hostile collection opcode register/layout validation |
| `bootstrap/src/bytecode/vm_collection.c` | 0 | 450 | managed descriptor cache, cell conversion, collection execution, and G4005 mapping |
| `bootstrap/src/native/c_emitter_collections.c` | 0 | 337 | portable-C descriptors, instructions, and iterator panic emission |
| `runtime/common/managed_map.c` | 0 | 1,205 | D079 coherent SipHash keys, insertion order, failure-atomic growth, mutation generation, and precise tracing |
| `runtime/common/managed_set.c` | 0 | 161 | narrow Set API over the single map-storage implementation |
| `runtime/include/mmay/runtime/map.h` | 0 | 78 | narrow owner API for managed ordered maps |
| `runtime/include/mmay/runtime/set.h` | 0 | 56 | narrow owner API for managed ordered sets |
| `tests/c-unit/runtime/test_managed_map_set.c` | 0 | 704 | dual-seed order, counted UTF-8/NUL and nominal key laws, randomized cyclic GC graph, limits, failure atomicity, and exhaustive Map/Set allocation cleanup |
| `tests/c-unit/test_cp36_collections.c` | 0 | 757 | checker/MIR/hostile bytecode/codec/VM/native/formatter/allocation vertical coverage |
| `tests/c-unit/test_cp36_map_set.cmake` | 0 | 56 | public fmt/check/run/test-both/Release artifact acceptance across every key family |
| `std/cli/cli.my` | 0 | 320 | D081 public parser model, factories, validation, limits, and stable messages |
| `std/cli/parse.my` | 0 | 248 | D081 ordered long/short/positional parsing and typed usage results |
| `std/cli/help.my` | 0 | 143 | D081 deterministic byte-width help rendering and bounded builder errors |
| `bootstrap/src/cli/cli_standard_cli_package.c` | 0 | 18 | narrow three-document ordinary-package embedding owner |
| `bootstrap/src/cli/cli_standard_packages.c` | 507 | 539 | generic embedded-package support for a third independently checked document |
| `bootstrap/src/check/checker_imported_nominal.c` | 414 | 440 | recursive imported nominal layout closure over nested Map/Set fields |
| `bootstrap/src/check/checker_match_binding.c` | 1,050 | 1,069 | erased-void wildcard binding without an invalid payload projection |
| `bootstrap/src/bytecode/bytecode_codec_nominal.c` | 1,059 | 1,057 | nominal layout decoding admits the complete verified register-type set |
| `tests/c-unit/test_cp37_cli_parser.c` | 0 | 69 | every standard-package embedding allocation failure with zero-counter teardown |
| `tests/c-unit/test_cp37_cli_parser.cmake` | 0 | 157 | exact help/error/status and heap-limit VM/Release-native process parity |
| `tests/public-alpha/cp37-cli-parser/main_test.my` | 0 | 328 | D081 definition, usage, Unicode, generated, help, limit, and both-engine coverage |
| `std/json/json.my` | 0 | 132 | D082 public cursor/error/limit model and stable messages |
| `std/json/decode.my` | 0 | 593 | strict UTF-8, token, duplicate, container, typed-read, and iterative-skip decoding |
| `std/json/encode.my` | 0 | 274 | bounded poisoned state machine and deterministic compact encoding |
| `bootstrap/src/cli/cli_standard_json_package.c` | 0 | 18 | narrow three-document ordinary-package embedding owner |
| `bootstrap/src/cli/cli_standard_packages.c` | 539 | 544 | generic embedded-package support for the fourth independently checked package |
| `tests/c-unit/test_cp38_json.c` | 0 | 69 | exhaustive embedded JSON-package allocation failure with zero-counter teardown |
| `tests/c-unit/test_cp38_json.cmake` | 0 | 186 | public file transform, capability denial, hostile diagnostics, heap pressure, and VM/native parity |
| `tests/public-alpha/cp38-json/main_test.my` | 0 | 330 | explicit records/enums/collections, policies, Unicode, limits, and deterministic form |
| `tests/fuzz/json_fuzz.c` | 0 | 315 | embedded-package/checker/verified-bytecode/VM JSON fuzz boundary |
| `bootstrap/src/native/c_emitter.c` | 477 | 478 | defer-only suppressed-cleanup global bound accounting |
| `bootstrap/src/native/c_emitter_internal.h` | 332 | 333 | narrow owner-private defer-presence query declaration |
| `tests/c-unit/test_native_c_emitter_baseline.c` | 290 | 293 | call-only generated C excludes defer cleanup stacks |

The earlier mechanical split preserves assertion order within the array suite
and the single external `mmay_test_mir` registration. The service checkpoint
adds an explicit public owner plus shared phase-option validators; it changes
no Marimay language semantics and leaves the public CLI unavailable.
The command evidence is recorded in
[architecture checkpoint 2](evidence/architecture-checkpoint-2-2026-08-14.md)
and
[architecture checkpoint 3](evidence/architecture-checkpoint-3-2026-08-14.md),
followed by the
[checker sibling-module boundary](evidence/architecture-checkpoint-4-2026-08-14.md)
and the first semantic consumer of that boundary in
[V0 source arrays iteration 11](evidence/v0-source-arrays-iteration-11-2026-08-14.md),
followed by
[the compiler-service foundation](evidence/i2-compiler-service-foundation-iteration-36-2026-08-14.md)
and its
[request-local incremental/cancellation checkpoint](evidence/i2-compiler-service-incremental-iteration-37-2026-08-14.md),
followed by
[successful-package definition facts](evidence/i2-checker-facts-iteration-38-2026-08-14.md).
The result-scoped query and exact checker-backed completion foundation is
recorded in
[preview checkpoint 3](evidence/i2-compiler-service-queries-checkpoint-3-2026-08-17.md).
The repository-wide bounded-context enforcement and full production/test
decomposition are recorded in
[A0 checkpoint 0](evidence/a0-bounded-architecture-checkpoint-0-2026-08-14.md).
