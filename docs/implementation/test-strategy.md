# Ruda implementation and conformance test strategy

Status: normative implementation requirement.

Testing is not a final phase. Every accepted feature is represented across the layers it affects: lexer/parser, formatter, resolver/type system, diagnostics, MIR, verifier, VM, C backend, runtime, public `ruda test`, documentation, and compatibility.

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
prove balance only for calls routed through `RudaAllocator` with correct size
arguments; they do not track pointer identity and do not replace LeakSanitizer
or Valgrind. All error paths run under sanitizers.

Production code under `bootstrap/` and `runtime/` must not reference raw
process-heap APIs outside `bootstrap/src/base/allocator.c`;
`tools/check-c-memory-policy` enforces that boundary across compiled C headers,
sources, and include fragments after C line-splicing and removal of comments
and literals. Linux x86_64 LSan
with `detect_leaks=1` is a stable required CI contract for the current commit
SHA. A scheduled/manual Ubuntu Valgrind job independently checks the parent
C-unit process, reports all of its leak kinds, and fails on definite or
indirect leaks. Child-process tracing is deferred until system compiler
children can be excluded without hiding Ruda children. Sanitizer suppressions
require a committed rationale and review.

C token pasting is rejected in production code unless a future exact allowlist
is separately reviewed, because parameterized macros can otherwise manufacture
a forbidden heap identifier after this policy check.

Evidence wording remains bounded: a green commit means covered paths were
leak-clean under the named LSan, allocator-balance, and Valgrind checks. It does
not prove that no leak exists in unexecuted behavior.

### 2.2 Syntax and formatter fixtures

Each syntax case has:

- `.ruda` input;
- expected lossless tree or concise structural dump;
- expected formatted output;
- expected diagnostics for invalid forms;
- semantic-AST equality assertion across format;
- comments/trivia ownership assertion where relevant.

Record and named enum-payload construction fixtures deliberately write fields
out of declaration order with effectful initializers. Formatting may reflow the
brace group but must preserve the exact field sequence; no formatter test may
use semantic name resolution to justify a reorder.

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

Every public `ruda` code fence in README is extracted and parsed by the I1 frontend; D025 declaration separators and all other public syntax therefore fail executable tests rather than only prose scans. At I2 those fences and versioned documentation examples become compile-pass/fail fixtures as soon as their referenced library surface exists. I8 adds execution, output expectations, and the public `ruda doc --check` runner; it does not postpone syntax/type validation until then.

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
- left-to-right evaluation with side effects;
- strings, Unicode, collection ordering, equality/hash laws;
- nominal-record identity: aliases compare equal, distinct allocations with
  equal fields compare unequal, operands evaluate once left-to-right, moving
  collectors preserve the result, and forged handles/layouts are rejected;
- enum layout independence and nested match;
- interface/closure direct and dynamic calls;
- GC roots through every value and call shape;
- cleanup on normal/error/panic/cancel paths;
- explicit fallible close, idempotent best-effort fallback, and cleanup panic
  precedence on normal/`Err`/panic/cancel paths;
- deterministic capability virtualization.

### 2.8 CLI end-to-end tests

Launch the real `ruda` binary in disposable directories. Cover root discovery, script mode, all commands/help/errors, program argument separation, exit statuses, JSON schema, atomic formatting, stale locks, offline cache, missing C compiler, corrupt bytecode/cache, interrupted operations, read-only trees, symlinks, paths with spaces/Unicode, and Windows path/console behavior.

Native-install fixtures start from clean Tier 1 images and assert the D030 path: official installation/preflight, machine-readable `ruda doctor`, native `ruda build` without user-selected compiler/linker/SDK variables, offline repeat build, and a single actionable diagnostic when a platform-owned prerequisite is genuinely unavailable.

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
mapping or the `limit` and assert wrong-error or arity diagnostics. These are
parser-backed now and become checker-backed immediately when generic
`Result`/`std/fs` imports exist; parser success is not compile-pass evidence.

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

Generated-identifier fixtures use every C17 keyword that is a valid Ruda
identifier as each user-visible Ruda name class, plus synthetic internal
double-underscore/underscore-plus-uppercase collision attempts, standard-header
external names, Unicode normalization pairs, long common prefixes, symbol-hash
prefix collisions, runtime-helper-like names, and names differing only by case.
The emitted C must use injective compiler-owned identifiers, compile with every
Tier 1 C toolchain, and preserve distinct Ruda symbols. Explicit `extern "c"`
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

- Run all C and generated-C suites under available ASan, UBSan, MSan, TSan/platform analyzers.
- Maintain static-analysis configurations and justify suppressions in source.
- Fuzz all untrusted parsers/verifiers with resource limits.
- Test capability path canonicalization against symlinks, traversal, alternate separators, case folding, races, and network endpoint ambiguity.
- Test package provenance rollback, equivocation, hash/signature mismatch, malicious archives, and dependency confusion.
- Review unsafe/FFI packages and generated C before each RC.
- Maintain a private-to-public security regression workflow that lands a test with every fix.

ThreadSanitizer primarily covers runtime carrier/blocking/foreign threads; safe Ruda isolate code does not expose shared-memory threading.

## 8. Bootstrap testing

For each release candidate:

1. build Stage 0 with at least two supported C compiler families;
2. build Stage 1 from the same Ruda compiler source;
3. build Stage 2 using Stage 1;
4. normalize and compare Stage 1/Stage 2 artifacts;
5. compile/run the conformance suite with both;
6. rebuild from the source release archive offline on independent machines;
7. record all input/tool/artifact hashes and differences.

Formatter, diagnostic code registry, standard library metadata, bytecode tables, and package resolver must be included in bootstrap parity, not only the core compiler executable.

## 9. Compatibility testing

Maintain released-source, `.rudab`, `ruda.mod`, `ruda.lock`, diagnostic JSON, test-event, registry, and trace corpora according to each format's compatibility promise.

Every current toolchain builds/runs supported old source-language versions. Old toolchains must reject newer mandatory bytecode/manifest features clearly. Package semver tests verify that additions such as public enum variants are classified as breaking.

Formatter migration is tested version-to-version and never occurs during build.

## 10. Performance testing

Performance suites are divided into correctness-sensitive budgets and exploratory benchmarks.

Release budgets cover cold/warm run, check, format, test, clean/incremental native build, compiler peak memory, VM verification/startup, binary size/startup, generic code size, GC throughput/pause, task spawn/park/wake/cancel, parked-task bytes, channels, race/select registration and cancellation, isolate copy/transfer, HTTP throughput/tail latency, and package resolution/cache. D007 additionally records heap object count, allocated bytes, retained bytes, and GC work for JSON DTO graphs, immutable UI-style trees, request/response models, and collection-heavy transformations, with escape-analysis-disabled and explicit representation alternatives where available.

E0 usability evidence is quantitative where possible but not reduced to benchmark timing: external task transcripts count boundary-local `mapError` sites, completion-assisted dot-call discoveries, manual native-toolchain setup steps, race/select plumbing, completion time, and abandoned/incorrect attempts. Semantic findings block I10 until resolved through decisions/spec/tests.

Use fixed reference hardware plus noisy public CI trends. Store raw distributions and environment metadata. A statistically meaningful regression over budget blocks release unless an accepted decision records the user benefit and new budget.

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
ruda fmt --check
ruda check --all-targets
ruda test
```

Project CI:

```sh
ruda fmt --check
ruda check --all-targets
ruda test --engine both
ruda vet --strict
ruda doc --check
ruda audit
```

Compiler release CI additionally runs C unit tests, all native compiler/optimization combinations, sanitizers, fuzz campaign thresholds, compatibility, bootstrap, reproducibility, installer, package-security, load/fault, and performance gates.

Skipped, quarantined, flaky, or platform-excluded tests are release failures unless a time-bounded decision names owner, reason, affected claim, and removal gate. Automatic reruns may collect evidence but cannot turn a flaky failure into a pass.
