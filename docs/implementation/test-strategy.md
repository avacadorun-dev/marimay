# Marimay implementation and conformance test strategy

Status: normative implementation requirement.

Testing is not a final phase. Every accepted feature is represented across the layers it affects: lexer/parser, formatter, resolver/type system, diagnostics, MIR, verifier, VM, C backend, runtime, public `mmay test`, documentation, and compatibility.

## 1. Evidence model

Each normative requirement receives a stable requirement ID during implementation, for example:

```text
LANG-MATCH-EXHAUSTIVE-001
EXEC-TASK-SCOPE-EXIT-004
TOOL-FMT-IDEMPOTENT-002
```

Test manifests map fixtures/properties/fuzz targets to requirement IDs. A release audit fails when a normative **must** has no evidence mapping, when mapped tests are skipped on a claimed platform, or when a fixture exercises only a narrower condition.

A green test proves only its named behavior. Code coverage is diagnostic evidence, not a substitute for requirement coverage.

## 2. Test layers

### 2.1 C unit tests

Directly test bootstrap base modules, source positions, UTF-8, arenas, checked arithmetic, vectors/maps, hashing, paths, diagnostics, lexer, parser utilities, type canonicalization, pattern matrices, MIR builders, bytecode codecs/verifier, GC marking, scheduler queues, capability matching, archive safety, and CLI parsing.

Every allocation-capable C API is exercised on the successful path, all
material error paths, and under deterministic allocation failure at every
reachable allocation index. After full teardown, every such scenario asserts
both `live_allocations == 0` and `live_bytes == 0`. These aggregate counters
prove balance only for calls routed through `MarimayAllocator` with correct size
arguments; they do not track pointer identity and do not replace LeakSanitizer
or Valgrind. All error paths run under sanitizers.

An expensive deterministic allocation sweep may be split into fixed CTest
shards only when their index sets are disjoint and their union is the original
failure range. Changing the shard count must not reduce that range. Each shard
independently measures the successful-path allocation count and asserts clean
teardown; any late-phase probe is assigned to exactly one shard. Only shards
whose fixtures and outputs are read-only may run concurrently. Tests that
create shared build-tree artifacts remain serial.

The C-unit coordinator is registered in CTest as fixed owner-aligned suite
shards plus named focused checkpoint owners. Every leaf test function is
reachable from exactly one CTest owner, while the direct runner executes the
union of both owner families. `check-architecture` expands the coordinator call
graph and rejects duplicate or orphaned leaf owners. Each owner receives a
private copy of the fixture tree and a private working directory, so generated
native outputs and temporary files cannot collide; a focused name is never a
reason to repeat the same leaf function in a broad shard. Local CTest presets
use a bounded four-process pool; sanitizer CI may use up to eight isolated
processes. Fresh-build `COST` values are calibrated from the slowest supported
instrumented profile so the two-worker CP38, CP37, and CP39 integration owners
occupy the critical path first instead of serializing behind one-worker tests.
The ASan CP38 owner retains its proven whole-pool memory exclusion and is
therefore scheduled first. After that exclusion releases the pool, the long
native owner overlaps the two-worker CP37/CP39 chain rather than waiting behind
allocation shards. Shared filesystem resources use a narrow CTest resource
lock rather than forcing unrelated tests to run serially.
Process-integration tests whose artifacts are private reserve the number of
CTest workers needed by their compiler and native subprocesses.

CP39 lifecycle/native-run coverage remains in every Debug, Release, and
sanitizer profile. Archive creation, reproducibility replay, offline source
configuration, and archive relocation have a separate Release-only owner. That
owner exercises the public `mmay_package` target, compares it with an
independent packager invocation, and leaves the verified target output for CI
upload. Debug and sanitizer jobs therefore do not repeat source archiving that
cannot observe sanitizer instrumentation.

Gitea jobs pin action commits and the documentation runner-image digest. C17,
sanitizer, and fuzz jobs use the host-local image derived from the pinned
`.gitea/images/c17-linux-x86_64.Dockerfile`; its full Dockerfile SHA-256 is the
image tag, and `tools/build-gitea-ci-image --check` rejects workflow drift.
Rebuild that image on every Gitea runner host before pushing a Dockerfile tag
change. The shared `tools/ci-apt-install` bootstrap then proves the declared
commands or packages are already present without network access; its bounded
apt fallback remains only for an incompletely provisioned runner. A package
mirror outage must fail setup promptly; it must not consume the checkpoint test
budget or masquerade as a test timeout. Long-lived credentials are not retained
by checkout, and artifact transport uses pinned action revisions.

A path-classification job compares the pushed or pull-request base revision
with the exact head. Revisions changing only `docs/**` and top-level Markdown
run the complete documentation, generated-artifact, architecture, memory, and
release-contract gate while skipping compiler, sanitizer, and fuzz work whose
inputs are unchanged. A missing comparison base fails safe to the complete
matrix, and every workflow, build, source, test, fixture, or tool change is
classified as code. This keeps evidence-only commits fast without suppressing
any affected executable test.

Clang/GCC sanitizer presets compile host compiler and test targets with `-O1`
while retaining Debug symbols and assertions. This keeps exhaustive
allocation-failure coverage practical without changing its index range or
error-path assertions. Generated and native runtime artifacts retain the exact
optimization level of the requested debug (`-O0`) or release (`-O2`) profile.

Production code under `bootstrap/` and `runtime/` must not reference raw
process-heap APIs outside `bootstrap/src/base/allocator.c`;
`tools/check-c-memory-policy` enforces that boundary across compiled C headers,
sources, and include fragments after C line-splicing and removal of comments
and literals. Linux x86_64 LSan
with `detect_leaks=1` is a stable required CI contract for the current commit
SHA. A scheduled/manual Ubuntu Valgrind job independently checks the parent
C-unit process, reports all of its leak kinds, and fails on definite or
indirect leaks. Child-process tracing is deferred until system compiler
children can be excluded without hiding Marimay children. Sanitizer suppressions
require a committed rationale and review.

C token pasting is rejected in production code unless a future exact allowlist
is separately reviewed, because parameterized macros can otherwise manufacture
a forbidden heap identifier after this policy check.

Evidence wording remains bounded: a green commit means covered paths were
leak-clean under the named LSan, allocator-balance, and Valgrind checks. It does
not prove that no leak exists in unexecuted behavior.

### 2.2 Syntax and formatter fixtures

Each syntax case has:

- `.my` input;
- expected lossless tree or concise structural dump;
- expected formatted output;
- expected diagnostics for invalid forms;
- semantic-AST equality assertion across format;
- comments/trivia ownership assertion where relevant.

Record and named enum-payload construction fixtures deliberately write fields
out of declaration order with effectful initializers. Formatting may reflow the
brace group but must preserve the exact field sequence; no formatter test may
use semantic name resolution to justify a reorder.

D054 fixtures cover scalar, nested, qualified, generic, tuple, and function
element types; incomplete `[]`, `[][]`, `[][`, and generic-element input;
comment preservation; canonical prefix adjacency; semantic round trip; and
second-pass formatter byte identity. Checker tests separately cover contextual
empty literals, aliases/imports, the targeted legacy diagnostic, and an
ordinary local/imported nominal type named `Array`.

D055 resource-policy coverage calibrates accounted object/slot/cell bytes,
permits the exact configured boundary, rejects one byte beyond it before a
host allocation, and exercises raw, reference, and nested array construction,
`push`, `reserve`, collection retry, and failure atomicity. Separate
fail-every-allocation sweeps classify host exhaustion and assert both allocator
counters return to zero. CLI tests cover both option spellings, min/max,
zero/sign/empty/non-decimal/overflow/duplicate/missing negatives, argument
locations, help, human/JSON schemas, default/selected native embedding and
cache separation, fresh test heaps, and byte-exact VM/native stderr/exit
observations.

Fixture families cover valid edge cases, one-token deletions/insertions, delimiter nesting, Unicode, CRLF, empty files, maximum limits, and recovery followed by valid declarations.

`tests/syntax/parser/coverage.tsv` is indexed one-for-one by the generated
syntax-kind registry. The independent
`tests/syntax/parser/production-coverage.tsv` is indexed one-for-one by the
machine-readable source grammar. `check-syntax-coverage` fails on a
missing/duplicate kind or production, missing fixture, nonexistent C test
owner, or a `covered` row without positive, negative, recovery, formatter, and
parser-owner evidence. An explicitly `open` row remains visible and cannot be
reported as gate completion. Syntax-kind completeness is therefore not
mistaken for fine grammar-production completeness.

Every public `mmay` code fence in README is extracted and parsed by the I1 frontend; D025 declaration separators and all other public syntax therefore fail executable tests rather than only prose scans. At I2 those fences and versioned documentation examples become compile-pass/fail fixtures as soon as their referenced library surface exists. I8 adds execution, output expectations, and the public `mmay doc --check` runner; it does not postpone syntax/type validation until then.

### 2.3 Compile-pass fixtures

Small programs prove accepted static behavior. Required families include:

```text
bindings_and_definite_assignment
numeric_types_and_explicit_conversion
record_construction_and_visibility
flat_nominal_record_match
mutable_field_rules
enum_unit_positional_named_payload
nested_exhaustive_match
exact_string_literal_match
match_guards_or_patterns_and_yield
option_result_and_question
defer_cleanup_edges
generic_inference_and_constraints
dot_call_and_free_call_same_symbol
dot_call_overload_by_owned_first_nominal_type
structural_interface_conformance
closure_capture_and_iteration_binding
task_scope_wait_and_result
non_escaping_ffi_views
constant_evaluation
contextual_words_as_identifiers
all_contextual_construct_name_collisions
ordinary_test_fuzz_panic_and_yield_calls
construction_source_order
signed_division_remainder_boundaries
dot_call_import_form_invariance
println_string_result_and_formatters
```

Every compile-pass fixture is formatted, type checked twice under perturbed file/import order, lowered to verified MIR, and eventually run on all engines when it has behavior.

### 2.4 Compile-fail fixtures

Each invalid program asserts stable diagnostic code, primary span, essential labels, and absence of misleading cascades. Exact prose snapshots are used only where copy is the feature.

Required families include:

```text
null_undefined_reserved_and_any_not_a_dynamic_type
hard_keyword_identifier_rejection
invalid_test_fuzz_attributes
implicit_numeric_conversion
println_non_string_argument
uninitialized_and_same_block_shadow
unknown_duplicate_missing_record_field
record_pattern_nominality_fields_and_guard_coverage
mutate_immutable_field
unqualified_foreign_enum_constructor
non_exhaustive_and_unreachable_match
guard_does_not_make_exhaustive
or_pattern_binding_mismatch
question_outside_result_or_wrong_error
defer_non_void_or_result_call
invalid_general_function_overload
dot_call_for_foreign_first_parameter
interface_signature_variance_or_visibility
generic_constraint_failure
task_outside_scope
task_copy_store_return_or_escape_capture
channel_endpoint_escape
ffi_view_live_across_suspension
suspending_c_callback
unsafe_operation_outside_unsafe
runtime_top_level_initialization
question_exact_fs_error_mapping
package_cycle_and_private_api_leak
```

Mutation testing periodically deletes/reverses checker conditions to verify the negative corpus detects seeded faults.

### 2.5 MIR tests

Golden MIR tests focus on semantic lowering, not incidental block numbering. A normalizer assigns stable local/block IDs. Assert:

- source evaluation becomes explicit left-to-right temporaries;
- `?` branches to error cleanup/return;
- `defer` is present exactly once on every exit edge, preserves LIFO order,
  masks cancellation while running, and records primary/suppressed cleanup
  panic precedence;
- match payload projections follow checked tags;
- task scopes cancel/join on all outward edges;
- suspension points list exactly the live roots/locals;
- bounds/overflow checks are explicit unless a proof removes them.

Optimization tests compare unoptimized and optimized MIR for generated inputs and assert verifier validity after every pass.

### 2.6 Bytecode tests

For every opcode test valid encoding, boundary operands, invalid register/type, truncated encoding, invalid branch, root-map mismatch, forbidden unsafe/capability placement, disassembly, execution, and round-trip serialization.

Bundle tests cover section reordering, duplicate sections, integer overflow, hash mismatch, signature mismatch, decompression/archive limits, incompatible versions, missing mandatory features, and no partial initialization on failure.

### 2.7 Run and conformance fixtures

Run fixtures record typed result, stdout, stderr, exit, panic, source frames, capability calls, allocations where specified, and task trace. Required semantic families include:

- integer boundaries and every explicit overflow operation;
- signed division/remainder signs, quotient identity, zero divisors, and
  `MIN / -1` plus `MIN % -1` at every signed width;
- `f64` decimal nearest-even conversion and raw-bit preservation across
  constants, copies, arguments, results, and returns; IEEE bit patterns,
  subnormals, NaN/infinity/signed zero; arithmetic NaN canonicalization,
  unary-NaN sign toggle, ordered/unordered comparisons, and IEEE division by
  signed zero;
- managed arrays cover exact homogeneous construction, empty contextual and
  explicit-type construction, alias-visible mutation, length and failure-atomic
  growth, nested reference tracing/cycle collection, signed bounds including
  `INT64_MIN`/`INT64_MAX`, exact G4002 bytes/spans/frames, and D045 indexed-RHS
  ordering. A loop-based rectangular f64 matrix fixture must agree bit-for-bit
  in the MIR oracle, encoded VM, and optimized/unoptimized native output;
- D053 array mutation covers raw/reference/nested arrays; empty and nonempty
  `pop`; valid, negative, expanding, equal, partial, and full `truncate`;
  empty/nonempty `clear`; zero/below/equal/above/negative/overflowing
  `reserve`; alias identity; removed-reference reachability; non-wrapping
  generation changes; `G4005` iterator invalidation; failure-atomic storage
  growth; every deterministic allocation/resize failure; and exact MIR,
  encoded VM, generated-C debug/release, stdout/stderr/exit/panic equivalence;
- left-to-right evaluation with side effects;
- strings, Unicode, collection ordering, equality/hash laws;
- D076 text/bytes: randomized valid and hostile UTF-8 round trips, every byte
  boundary, CRLF/zero-width alpha grapheme partitions, exact ASCII trim,
  split/join identities and part limits, Bytes copy/alias mutation, iterator
  invalidation, builder snapshots/overflow, binary stream partial operations,
  malformed MIR/bytecode, and every reachable allocation failure;
- nominal-record identity: aliases compare equal, distinct allocations with
  equal fields compare unequal, operands evaluate once left-to-right, moving
  collectors preserve the result, and forged handles/layouts are rejected;
- enum layout independence and nested match;
- interface/closure direct and dynamic calls;
- GC roots through every value and call shape;
- cleanup on normal/error/panic/cancel paths;
- explicit fallible close, idempotent best-effort fallback, and cleanup panic
  precedence on normal/`Err`/panic/cancel paths;
- deterministic capability virtualization;
- D046 preview stdout: `println` accepts only `string`; `formatF32`/`formatF64`
  produce the specified unique ASCII for finite values, both zero signs,
  infinities, and signed NaN; each successful `println` publishes exact UTF-8
  plus one LF, including empty lines, payload newlines, and embedded NUL;
  `"io.stdout"` denial writes no bytes and returns `CapabilityDenied`;
  `WriteFailed` is classified without host `errno` text; VM, native, and
  `expect.output` capture agree on successful bytes. The internal `ok:f64:`
  hexadecimal harness is not compared as user stdout. A loop-based matrix
  program must print `415.0\n` through `println(formatF64(...))?`. These run
  and type fixtures become executable in the engine checkpoint that implements
  `std/io`/`std/convert`; parser and formatter fixtures own the public source
  shape now.

### 2.8 CLI end-to-end tests

Launch the real `mmay` binary in disposable directories. Cover root discovery, script mode, all commands/help/errors, program argument separation, exit statuses, JSON schema, atomic formatting, stale locks, offline cache, missing C compiler, corrupt bytecode/cache, interrupted operations, read-only trees, symlinks, paths with spaces/Unicode, and Windows path/console behavior.

Native-install fixtures start from clean Tier 1 images and assert the D030 path: official installation/preflight, machine-readable `mmay doctor`, native `mmay build` without user-selected compiler/linker/SDK variables, offline repeat build, and a single actionable diagnostic when a platform-owned prerequisite is genuinely unavailable.

D052 profile fixtures build each output profile from both debug- and
release-built compiler executables. They inspect an archive-owned profile
marker, relocate a complete installed prefix, and cover paths with spaces,
missing/partial/stale/wrong-profile manifests and archives, content-hash cache
invalidation, `--emit c`, requested outputs, and bounded diagnostics that do
not leak raw linker output. Runtime artifact discovery and hashing are swept
through every deterministic allocation failure with zero live allocations and
bytes after disposal.

### 2.9 Compiler service and editor fixtures

Versioned overlay tests edit files without disk writes and assert cancellation, stale-version rejection, deterministic diagnostics, definition, symbol search, and completion. For every nominal type fixture, dot completion is compared directly with the checker-owned eligible first-parameter function set under imports, visibility, same-name candidates, generics, and incomplete syntax. Each item exposes its owning package and equivalent free-call spelling. I8 protocol/LSP tests wrap this owner API and cannot maintain independent lookup logic.

The import matrix includes a named type-only import, named type plus function
import, and namespace import for the same foreign nominal type. Each case
passes the value to an interface parameter, proves identical conformance and
dot completion, and separately asserts the free-call spellings that are or are
not visible. E0 records whether users discover this behavior without prior
knowledge of the free function.

The D006 documentation pair is explicit: the pass case uses
`fs.readText(filePath, limit).mapError(configError)?`; fail cases omit the
mapping or the `limit` and assert wrong-error or arity diagnostics. The
checkpoint-4 checker fixture now covers the pass-side generic `Result` and
`std/fs` namespace import through an external signature; parser success alone
is not compile-pass evidence, and runtime filesystem behavior remains a later
standard-library checkpoint.

I1 parser overlay tests additionally assert green-node pointer identity for
unchanged top-level prefixes and same-offset suffixes, safe shared-storage
lifetime when the previous parse result is disposed first, exact lossless bytes
against the new snapshot, invalid edit-range rejection, and allocation failure
at every incremental parse allocation.

## 3. Differential testing

The oracle ladder is:

```text
source semantic expectations
    -> unoptimized MIR interpreter
    -> optimized MIR interpreter
    -> bytecode VM
    -> generated C at -O0
    -> generated C at release optimization
```

A deterministic fixture must agree across every available rung. Comparison includes value/output/panic/task/capability traces, not merely exit code.

Run native C with supported Clang/GCC/MSVC and at least debug/release optimization. A discrepancy is minimized by syntax-aware delta reduction while retaining type validity when possible. The minimized program and its original seed become permanent fixtures.

Generated-identifier fixtures use every C17 keyword that is a valid Marimay
identifier as each user-visible Marimay name class, plus synthetic internal
double-underscore/underscore-plus-uppercase collision attempts, standard-header
external names, Unicode normalization pairs, long common prefixes, symbol-hash
prefix collisions, runtime-helper-like names, and names differing only by case.
The emitted C must use injective compiler-owned identifiers, compile with every
Tier 1 C toolchain, and preserve distinct Marimay symbols. Explicit `extern "c"`
link names are tested separately as ABI strings and are never silently passed
through the internal mangler.

For concurrent programs, record external completions and use the deterministic task scheduler. Separately run schedule exploration over bounded task programs to find order-dependent bugs; valid different interleavings must still preserve memory/task-scope safety invariants.

## 4. Property and metamorphic testing

Required properties include:

- format idempotence and parse/format semantic preservation;
- encode/decode identity for bytecode, lock, manifest, diagnostic JSON, and trace events;
- import/file enumeration order does not change semantics or diagnostics;
- adding unreachable private declarations does not change public interface hash;
- alpha-renaming locals does not change execution;
- safe optimization preserves MIR oracle behavior;
- map iteration preserves insertion order through resize/delete/reinsert rules;
- D089 collection cursors cover independent nested/interleaved progress, sparse
  storage, end checks, aliases, break/continue and always-terminating bodies;
  all six raw/reference shapes preserve rooting and zero-allocation traversal
  at an exact heap budget, including deterministic setup-failure teardown;
- forged but well-typed signed cursor values cover negative, zero, removed,
  out-of-range and exhausted positions in oracle/VM/native O0/O2; independent
  bool inputs/destinations and Set value reads test the focused verifiers;
  legacy wire IDs 151/152 retain ordinal results through codec round trips;
  the bytecode fuzz corpus includes the valid and hostile D089 cursor programs;
  all 42 native cases retain O0/O2 and separate-process execution while sharing
  six compilations per optimization level, with both allocator live counters
  zero after each child's teardown;
- equality is reflexive/symmetric/transitive where defined and hash-consistent;
- GC before/after any allocation does not change results;
- task cancellation at any safepoint runs each registered cleanup exactly once;
- VM/native under the same event recording agree;
- locked offline dependency builds match fetched builds.

Generators produce both arbitrary bytes for robustness and grammar/type-directed valid programs for semantic depth.

## 5. Fuzzing program

Persistent fuzz targets:

1. UTF-8/source line mapping;
2. lexer and parser with recovery;
3. parser/formatter round trip;
4. manifest/workspace/lock/registry parsers;
5. archive extraction and path normalization;
6. type checker and exhaustiveness on generated bounded ASTs;
7. MIR verifier and every optimization pass;
8. bytecode decoder/verifier/disassembler/VM;
9. diagnostic human/JSON renderers;
10. GC graph mutation and root descriptors;
11. scheduler event/cancellation sequencing;
12. inter-isolate message decode;
13. HTTP, JSON, TLS wrapper protocol parsing;
14. FFI descriptor and callback bridge.

The I1 acceptance budget for targets 2 and 3 is at least 600 CPU-seconds each
under ASan+UBSan with the committed corpora, bounded one-megabyte frontend and
512-KiB formatter inputs, and no unresolved crash, sanitizer finding, hang,
limit-contract failure, idempotence failure, or semantic-AST mismatch. Evidence
records the exact compiler/runtime, target command, executions, corpus growth,
coverage counters, peak RSS, and artifact disposition. Later sustained-release
campaigns increase this budget; passing an I1 campaign is not P3/P4 evidence.

CI configures and builds all fuzz executables once per revision and host, then
fans the immutable executable bundle out to isolated target campaigns. Every
campaign still receives its full time budget, corpus, sanitizer environment,
failure-artifact owner, and independent exit status; sharing compilation does
not share mutable corpus state or reduce executions by running targets in one
contended process pool.

On a bounded three-executor validation host, the C17 compiler/configuration
matrix depends on the short documentation/architecture gate. The shared fuzz
build runs in the initial wave and the C17/fuzz work fills free executors after
the gate. TSan depends on completion of ASan/LSan because concurrent sanitizer
instrumentation on the same host measurably increases both critical paths; it
executes every `c-unit` and allocation owner, including the runtime, CLI,
native-emitter, generated-C, and focused checkpoint owners. External process
integration, install, capability, and packaging wrappers create no shared
threads before I6 and remain in every Debug/Release row plus ASan/UBSan/LSan;
repeating them under TSan cannot observe an additional race boundary. These
dependencies and the explicit TSan owner selection are fail-fast resource
control rather than semantic-test filtering: every applicable matrix row still
runs after its gate passes.

Corpora and dictionaries are committed when legally/operationally appropriate. Every found failure is minimized and promoted to a deterministic regression test. Release reports state engine, sanitizer, target, corpus size, total executions/core-hours, unique coverage movement, and unresolved findings.

## 6. Runtime stress and fault injection

### 6.1 Garbage collector

Run tiny-heap, collect-every-allocation, random-collection, deep graph, wide graph, cyclic graph, mutation, task-frame, closure, interface, enum, channel, FFI-handle, and isolate teardown suites. Verify root balance and heap integrity after each collection in debug mode.

### 6.2 Tasks and cancellation

Systematically inject cancellation at every MIR safepoint in bounded programs. Assert:

- no child survives scope exit;
- `defer` runs once in LIFO lexical order;
- cancellation does not become an ordinary catchable value;
- a child panic cancels siblings and retains suppressed failures;
- waiting consumes the affine handle once;
- fairness tests make progress;
- shutdown drains or reports every I/O and blocking-pool operation.
- a structured race cancels and joins every losing nested operation before return;
- readiness selection unregisters each losing channel/timer/I/O wait exactly once without cancelling independently owned tasks;
- simultaneous readiness follows declared order in deterministic mode and replays identically in VM/native;
- cancellation and panic at every registration/winner/cleanup transition cannot double-resume or leak a wait registration.

Stress nested scopes, one million parked tasks, channel backpressure, timer storms, connection churn, blocking-pool saturation, repeated isolate creation/teardown, and task dumps during transitions.

### 6.3 I/O and services

Inject partial reads/writes, EINTR/platform equivalents, slow peer, half-close, DNS delay/failure, TLS truncation, expired certificates, accept exhaustion, file descriptor exhaustion, disk full, clock jumps, process termination, and signal races. Tests must use public direct-style APIs and cover both engines.

## 7. Security testing

- Run all C and generated-C suites under available ASan, UBSan, and MSan.
  TSan runs every in-process production C owner and every test that creates or
  shares thread-visible state; single-threaded external lifecycle wrappers stay
  under the memory/undefined-behavior sanitizers instead of being replayed.
- Maintain static-analysis configurations and justify suppressions in source.
- Fuzz all untrusted parsers/verifiers with resource limits.
- Test capability path canonicalization against symlinks, traversal, alternate separators, case folding, races, and network endpoint ambiguity.
- Test package provenance rollback, equivocation, hash/signature mismatch, malicious archives, and dependency confusion.
- Review unsafe/FFI packages and generated C before each RC.
- Maintain a private-to-public security regression workflow that lands a test with every fix.

ThreadSanitizer primarily covers runtime carrier/blocking/foreign threads; safe Marimay isolate code does not expose shared-memory threading.

## 8. Bootstrap testing

For each release candidate:

1. build Stage 0 with at least two supported C compiler families;
2. build Stage 1 from the same Marimay compiler source;
3. build Stage 2 using Stage 1;
4. normalize and compare Stage 1/Stage 2 artifacts;
5. compile/run the conformance suite with both;
6. rebuild from the source release archive offline on independent machines;
7. record all input/tool/artifact hashes and differences.

Formatter, diagnostic code registry, standard library metadata, bytecode tables, and package resolver must be included in bootstrap parity, not only the core compiler executable.

## 9. Compatibility testing

Maintain released-source, `.myb`, `mmay.mod`, `mmay.lock`, diagnostic JSON, test-event, registry, and trace corpora according to each format's compatibility promise.

Every current toolchain builds/runs supported old source-language versions. Old toolchains must reject newer mandatory bytecode/manifest features clearly. Package semver tests verify that additions such as public enum variants are classified as breaking.

Formatter migration is tested version-to-version and never occurs during build.

## 10. Performance testing

Performance suites are divided into correctness-sensitive budgets and exploratory benchmarks.

Release budgets cover cold/warm run, check, format, test, clean/incremental native build, compiler peak memory, VM verification/startup, binary size/startup, generic code size, GC throughput/pause, task spawn/park/wake/cancel, parked-task bytes, channels, race/select registration and cancellation, isolate copy/transfer, HTTP throughput/tail latency, and package resolution/cache. D007 additionally records heap object count, allocated bytes, retained bytes, and GC work for JSON DTO graphs, immutable UI-style trees, request/response models, and collection-heavy transformations, with escape-analysis-disabled and explicit representation alternatives where available.

E0 usability evidence is quantitative where possible but not reduced to benchmark timing: external task transcripts count boundary-local `mapError` sites, completion-assisted dot-call discoveries, manual native-toolchain setup steps, race/select plumbing, completion time, and abandoned/incorrect attempts. Semantic findings block I10 until resolved through decisions/spec/tests.

Use fixed reference hardware plus noisy public CI trends. Store raw distributions and environment metadata. A statistically meaningful regression over budget blocks release unless an accepted decision records the user benefit and new budget.

D056 is the checkpoint-21 array/native measurement contract. Its focused
probes separately cover cache-hot scalar get/set, sequential proven and dynamic
unproven bounds, growth with and without `reserve`, dense scalar traversal,
nested-array traversal, and checked integer loops. The faithful reusable
algorithm corpus is measured separately. Tier 1 reports retain every sample and
order event, use at least 3 warmups and 11 odd-count measured rounds, publish
median/MAD/Tukey-IQR/min/max/noise, and carry compiler/runtime/source/schema/
toolchain/resource hashes and metadata. Validators reject profile, checksum,
sample-count, metadata, finite-number, source-manifest, platform, and baseline
incompatibility. Reviewed budgets apply only to Marimay medians for one workload
and platform; a candidate regression receives one complete cool-down retry and
blocks only when both distributions exceed the same threshold. Debug archives,
three-sample local results, cross-language ratios, and automatic reruns are not
release-budget evidence.

D057 separates sequencing without weakening that contract. Checkpoint 21 uses
the dedicated Linux x86_64 report for all nine probes and the complete
eight-workload Marimay/Go/Rust corpus. A distinct public-alpha registry gate still
requires compatible reviewed reports for all five Tier 1 targets in the table
below and must be revalidated after intervening optimization work. Supplemental
platform reports do not change either target list.

D058 range-proof tests construct checked carriers, literal and true-edge loop
assumptions, including negative true-edge rejection and the nonnegative CFG
fixed point for nested affine loops. They rewrite duplicate reads and stores,
then compare the checked and rewritten MIR/bytecode/native observations. They
also forge missing carriers, wrong indices, direct and interface calls, every
array mutation, and block-boundary flow between proof and access, plus
allocation failures in the pass, verifiers, and bytecode codec. Every forged
in-bounds bytecode sequence must be rejected before VM execution; optimizer
allocation failures retain checked MIR and all failure sweeps leave allocator
counters at zero after teardown.

D059 exact-access tests cover raw and reference cells, aliases, negative and
upper indices, wrong descriptors, stale/reused handles, collection, growth,
mutation generations, and the checked/in-bounds status boundary. Generated-C
inspection asserts one retained exact-access call for a fused barrier/consumer
and absence of the former describe/length/get-set helper chain. Separate direct,
checked-proof, and static-proof programs execute at `-O0` and `-O2`; VM/native
fixtures retain exact result, stderr/exit, G4002 span, and mutation behavior.
Access itself performs no allocation; a fail allocator armed at the next index
must observe no attempt, and full heap teardown returns both live counters to
zero.

D060 storage tests cover every 1/2/4/8-byte raw layout at minimum/maximum and
alternating bit patterns, binary64 signed zero and NaN payloads, odd lengths,
alignment, zero-initialized reserve growth, alias-visible set/get/pop, and the
maximum successful elements under one fixed byte budget. Reference and nested
arrays prove that only initialized `CELL` payloads trace; raw payloads that
look like handles do not. Invalid tag/stride/class descriptors are rejected.
Construction and resize allocation sweeps prove failure atomicity, exact live
accounting, and zero allocator counters after teardown. Generated-C descriptor
inspection covers every scalar-to-layout mapping; bytecode round trips and
VM/native differential fixtures prove unchanged wire identity, results,
panics, and exits.

D061 capacity tests cover exact construction counts 0, 1, 3, 4, 5, and powers
of two for raw, reference, and nested elements; unknown growth and reserve
rounding at the same boundaries; covered no-op requests; maximum clamping and
overflow; and exact allocation/resize counts. Budget and host-allocation
fail-index sweeps preserve the pre-existing classification, requested bytes,
array contents, reservation, generation, and zero allocator counters after
teardown. Bytecode round trips plus VM/native differentials cover aliases,
iterator stability, source evaluation order, and huge/negative reserve paths.

D062 arithmetic-proof tests cover constants and type extrema, positive and
negative intervals, signed/unsigned widening conversions, remainder-derived
ranges, true/false branches, joins, widening loops, nested loops, and retained
overflow cases for add/subtract/multiply. Every rewritten MIR instruction is
compared with its checked form in the oracle, encoded/decoded, independently
verified, run in the VM, and emitted/executed as native C at O0 and O2. Forged
proven opcodes, wrong types, widened facts, altered branches, and work/allocation
failures must be rejected or retain checked MIR. Tests assert the generated C
uses unsigned-domain helpers, exact overflow message/span/order remains on
retained operations, and all fail-index sweeps end with zero allocator counters.

D063 activation-view tests bind exact raw/reference arrays and records, reject
wrong descriptors and stale handles, and keep views valid through rooted
collection and array storage growth. They cover alias-visible access,
generation commit, checked lower/upper bounds, nested record-array fields,
companion copy/reset, iterator mutation, and allocation-free bind/get.
Generated-C inspection requires direct C17 `RAW64` byte copies, eager exact
record-field views, fixed small root-location storage, and transitive
no-safepoint root elision; it rejects the former per-element exact-access calls
on the selected path. A focused relation fixture also requires repeated safe
index expressions and both arms of a safe index selection to omit redundant
checks, while an unproved selection and an operand redefinition retain G4002.
O0/O2 execution preserves checksums, G4002/G4005 spans,
alias-driven record-field replacement forces a rebind, checked numeric Option
construction remains a safepoint, and fail-injection builds preserve the
allocating root path. Inline checked and proven `i64` helpers are compared at
extrema with the out-of-line semantic owner, including overflow and
division-by-zero results.

CP41 hardware-counter reports use six stable logical columns: cycles, retired
instructions, branches, branch misses, cache references, and cache misses.
Every report also retains the physical `perf` event map. Generalized events are
preferred, but a virtual PMU that returns unavailable or nonpositive generalized
cache samples may use one named architecture-specific paired cache-access and
cache-miss profile after a positive preflight. The profile identifies its cache
level and access class, is identical for before and after, and is rejected if
mixed with another profile. This permits honest platform-local cache evidence
without relabeling L1 data-cache loads as LLC observations.

## 11. Platform matrix

Tier 1 target intent for 1.0:

| Platform | Architecture | VM | Native | Bootstrap | Sanitizer/tooling evidence |
|---|---|---:|---:|---:|---:|
| Linux | x86_64 | required | required | required | required |
| Linux | arm64 | required | required | required | required where available |
| macOS | arm64 | required | required | required | required |
| macOS | x86_64 | required | required | required | required where available |
| Windows | x86_64 | required | required | required | required platform analyzers |

Tier claims become final only after runtime/TLS/toolchain investigation. A platform is not Tier 1 because code compiles once; installers, paths, networking, signals/control events, stack traces, FFI, package cache, and sustained tests must pass.

## 12. Release suites

Fast pre-commit:

```sh
mmay fmt --check
mmay check --all-targets
mmay test
```

Project CI:

```sh
mmay fmt --check
mmay check --all-targets
mmay test --engine both
mmay vet --strict
mmay doc --check
mmay audit
```

Compiler release CI additionally runs C unit tests, all native compiler/optimization combinations, sanitizers, fuzz campaign thresholds, compatibility, bootstrap, reproducibility, installer, package-security, load/fault, and performance gates.

Skipped, quarantined, flaky, or platform-excluded tests are release failures unless a time-bounded decision names owner, reason, affected claim, and removal gate. Automatic reruns may collect evidence but cannot turn a flaky failure into a pass.
