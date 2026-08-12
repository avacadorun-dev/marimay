# Ruda compiler and production implementation brief

Status: implementation handoff.  
Audience: an autonomous implementation/documentation/test agent or compiler team.  
Initial implementation language: portable C17.

## 1. Mission

Build Ruda from an empty repository into a production-credible language implementation while preserving the normative contracts under `docs/spec/`.

Canonical target positioning:

> “Ruda is a statically typed, garbage-collected programming language for reliable native applications—from CLIs and network services to desktop software. It combines algebraic data types, structural interfaces, direct-style structured concurrency, and a unified VM/native toolchain, without null, async coloring, or exceptions for expected failures.”

> «Ruda — статически типизированный язык программирования с управляемой памятью для создания надёжных нативных приложений: от CLI и сетевых сервисов до десктопных программ. Он сочетает алгебраические типы данных, структурные интерфейсы, прямой стиль структурированной конкурентности и единый VM/native toolchain — без null, async-окраски и исключений для ожидаемых ошибок».

Ruda means ore: source material transformed by a clear language, strict types, and a cohesive toolchain into reliable native software. This is the intended product position, not a present-tense implementation claim. At the date of this brief, the C17 Stage 0 provides the `ruda` executable, lossless frontend, and canonical formatter; type checking, VM/native execution, networking, and desktop support remain roadmap deliverables.

The assignment is not complete when a parser prints an AST or when a toy program runs. Completion requires:

- a portable C17 bootstrap compiler and runtime;
- the bytecode VM and native C17 AOT backend from one typed MIR;
- algebraic enums, exhaustive match, structural interfaces, first-parameter dot calls, `Option`, `Result`, and `?`;
- direct-style suspending I/O, `scope`-owned `spawn`, cancellation, isolates, and managed memory;
- first-party `ruda fmt`, `ruda test`, packages, docs, vet, fuzzing, diagnostics, and audit;
- reproducible self-hosting with a retained C17 root of trust;
- platform, security, compatibility, performance, and operational evidence for 1.0;
- a reference production service built and operated with the public toolchain.

The everyday user path must remain:

```sh
ruda fmt
ruda test
ruda run .
ruda build --release
```

Do not satisfy the checklist by exposing compiler/runtime machinery in application syntax. Complexity belongs behind these commands.

## 2. Authoritative inputs

Read before implementation:

1. `AGENTS.md` for repository invariants and change protocol;
2. `docs/design/principles.md` for trade-off policy;
3. `docs/design/decisions.md` for accepted/provisional/rejected choices;
4. `docs/spec/language.md` for source semantics;
5. `docs/spec/execution.md` for VM/native/runtime semantics;
6. `docs/spec/toolchain.md` for CLI, formatter, tests, and packages;
7. `docs/implementation/roadmap.md` and `test-strategy.md` for gates.

When these disagree, `AGENTS.md` defines precedence. Resolve a conflict in design/spec/tests before coding around it. No implementation quirk silently becomes language law.

## 3. Constraints that cannot be traded away

- C17, not C++, for Stage 0 compiler/runtime/VM/formatter/test harness.
- No LLVM, Cranelift, JVM, JavaScript, or WebAssembly dependency in the required bootstrap path.
- No dynamically typed “fast mode”. Both engines consume verified typed MIR.
- Core source forms are `fn`, `scope`, `type Name { ... }`, `Name { ... }`, `$"...{value}..."`, and one `public` visibility modifier; do not add compatibility aliases.
- No `impl`, `async`, `await`, `Future`, `null`, exceptions, classes, lifetime annotations, or shared-memory user threads.
- No parser-only feature claims: syntax, types, formatter, both engines, diagnostics, and tests land together.
- D001 accepts the repository identity `Ruda`, executable `ruda`, source extension `.ruda`, and related first-party artifact names. N0 still blocks I7 public namespace commitments and every public preview/alpha until legal review and external domains, accounts, registry namespaces, signing identity, and command/PATH coexistence have direct evidence. No external identity is claimed or reserved by this brief.
- No arbitrary macro, install hook, build script, or compiler plugin execution in 1.0.
- No safe-code dependence on C undefined behavior.
- No VM-only or native-only standard-library semantics.
- No formatter style configuration in 1.x.
- No separate test framework needed for ordinary, integration, doc, property, or fuzz tests.
- Do not move repository trust policy into source semantics. The language owns
  program meaning, the compiler publishes stable semantic facts, tools consume
  them, runtime/OS layers enforce grants and limits, and CI owns provenance,
  coverage, review, and release policy under D034. One `ruda` executable is a
  product boundary, not an excuse to add author/agent/trust markers to Ruda.

Prototype-gated decisions D007, D010, D012, and D013 require measured evidence. If evidence rejects one, amend the decision and all affected specs before changing the implementation direction.

## 4. Required repository deliverables

The implementation should converge on this structure; names may change only through a recorded repository decision:

```text
/
├── AGENTS.md
├── README.md
├── ROADMAP.md -> docs/implementation/roadmap.md
├── CMakeLists.txt
├── cmake/
├── bootstrap/
│   ├── include/ruda/
│   ├── src/
│   │   ├── base/
│   │   ├── source/
│   │   ├── lex/
│   │   ├── syntax/
│   │   ├── parse/
│   │   ├── resolve/
│   │   ├── type/
│   │   ├── match/
│   │   ├── effect/
│   │   ├── mir/
│   │   ├── bytecode/
│   │   ├── vm/
│   │   ├── cgen/
│   │   ├── runtime/
│   │   ├── format/
│   │   ├── package/
│   │   ├── test/
│   │   └── cli/
│   └── main.c
├── compiler/                 # self-hosted Ruda compiler when Stage 1 starts
├── runtime/
│   ├── common/
│   ├── platform/
│   └── include/
├── std/
├── tests/
│   ├── c-unit/
│   ├── syntax/
│   ├── format/
│   ├── compile-pass/
│   ├── compile-fail/
│   ├── run/
│   ├── differential/
│   ├── bytecode/
│   ├── gc/
│   ├── task/
│   ├── package/
│   ├── cli/
│   ├── fuzz/corpus/
│   └── bootstrap/
├── examples/
├── docs/
└── tools/                    # development-time generators; outputs committed
```

Generated Unicode tables, opcode tables, and diagnostic registries are checked in with provenance and regeneration instructions. A release/bootstrap build must not require Python, Node.js, network access, or an uncommitted generator output.

## 5. Bootstrap build system

CMake is the canonical Stage 0 developer build system because it can express C17, sanitizers, platform probes, generated test data, and Windows builds without shell-only logic. CMake is a compiler-development dependency, not a dependency of Ruda user projects.

Baseline commands:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Presets must cover debug, release, ASan+UBSan, and platform-supported thread/memory sanitizers. Warnings are errors in project code for supported compilers. The project pins its minimum CMake version and never downloads dependencies during configure.

Once `ruda` can build and test the compiler module, the release gate runs both bootstrap C tests and public `ruda test --engine both` tests.

## 6. C17 engineering rules

### 6.1 Portability

- Compile under current supported Clang, GCC, and MSVC C modes.
- Use `<stdint.h>`, `<stddef.h>`, and explicit-width wire/runtime types.
- Isolate OS APIs under `runtime/platform/`.
- Do not depend on compiler extensions in common code; guarded intrinsics require a portable fallback and equivalence test.
- Detect endianness, pointer width, and platform capabilities at configure time, never through undefined probes.
- Under D039, generated C identifiers are injective compiler-owned names; never copy a Ruda source name into C or reserve C keywords in Ruda merely for backend convenience. The emitter must avoid C keyword, implementation-reserved, library, runtime-helper, Unicode, truncation, and case-folded collision spaces. Explicit `extern "c"` link names remain separately validated ABI contracts.

### 6.2 Memory and error discipline

- Use checked allocation helpers; never multiply/add allocation sizes without overflow checks.
- Use arenas for source-lifetime CST/HIR/MIR data and explicit owners for cross-phase artifacts.
- Every public C function documents ownership of input/output pointers.
- Vectors, maps, strings, spans, and diagnostics have central tested implementations; do not proliferate bespoke containers.
- No hidden process-global mutable compiler state. A `RudaSession` owns source, interners, diagnostics, cache, and options.
- OOM and internal invariant failures produce a controlled internal diagnostic where possible; they do not continue with corrupt state.
- Cleanup paths are sanitizer tested, including parse/type errors and cancellation.

### 6.3 Defensive input handling

Source, manifests, locks, registry metadata, bytecode, debug data, CLI strings, and FFI descriptors are untrusted inputs. All parsers have bounded recursion, allocation, nesting, diagnostic count, and recovery work. Integer conversions and slices are checked centrally.

## 7. Compiler architecture requirements

### 7.1 Source manager and diagnostics

The source manager retains exact bytes and normalized line indices, supports overlays for editors/stdin, assigns stable file IDs, and maps byte offsets to Unicode columns lazily. A span always names one source snapshot.

Diagnostics are structured data first and human rendering second. Each has stable code, severity, primary span, labels, notes, help, and zero or more machine-applicable edits. Ordering is deterministic by file/span/code. Error recovery must prevent cascades through invalid placeholder types/nodes.

### 7.2 Lexer and syntax tree

Implement a hand-written bounded lexer with full trivia and source spans. The lossless syntax tree retains tokens/comments for formatting and IDE operations. Invalid UTF-8, unterminated literals/comments, numeric overflow, confusable identifiers, and malformed escapes receive targeted diagnostics.

### 7.3 Parser

Use recursive descent for declarations/statements/types and Pratt or precedence climbing for expressions. Recovery synchronizes on grammar-specific delimiters and declaration starters. The parser returns a tree even with bounded errors but never invents a semantically valid declaration silently.

Maintain three test surfaces:

- concrete syntax tree snapshots;
- semantic AST/HIR snapshots after trivia removal;
- compile-fail diagnostic fixtures.

### 7.4 Formatter

Format from the lossless syntax tree/document model, not by printing the semantic AST. Preserve comments and source meaning. Implement group/indent/softline primitives with deterministic width decisions. Parser recovery nodes prevent write-back unless the file meets the formatter's accepted-recovery threshold.

Under D040, record and named-payload construction fields retain their written
order because that order controls initializer evaluation. Formatting this
surface is syntax-only and cannot consult declaration order or name resolution.

D031 braced lists use that same width oracle: imports, record construction/patterns, and named enum payload declarations stay compact only when the whole group fits and otherwise break canonically. Statement/declaration bodies remain multiline blocks.

The formatter ships as a library used by CLI, tests, editor service, and self-hosted formatter parity checks.

### 7.5 Resolution and packages

Resolve package declarations in phases: collect names, validate duplicates/visibility, resolve imports, resolve type signatures, then bodies. A function enters a nominal type's dot-call set only when its exact first-parameter type is owned by the declaring package. Same-name declarations are accepted only when all candidates are eligible and their first nominal types are distinct; all general overloading is rejected.

Package public interfaces and effect summaries are serialized in a versioned cache artifact. Cache invalidation uses content and semantic interface hashes, never timestamps alone.

### 7.6 Type checking

Represent nominal types by stable declaration IDs and instantiated type arguments. Use an explicit union-find/inference context for local generic inference; do not introduce global Hindley–Milner inference. Public signatures remain explicit.

Interface conformance is an exact dot-call-set lookup with generic substitution, visibility, first-parameter erasure, and parameter invariance. It must not evolve into an unrestricted trait logic engine. Cache conformance results by canonical type/interface pair.

`Task<T>`, borrowed FFI views, and transferred buffers are compiler-known restricted types with targeted escape analysis. Keep these rules isolated from ordinary managed values to preserve the no-general-ownership design.

### 7.7 Exhaustiveness

Implement pattern usefulness/exhaustiveness with a constructor-matrix algorithm over finite enum/bool/tuple/record products. Guards do not cover constructors. Produce missing witness patterns and redundant-arm spans. Apply explicit recursion/work limits with a diagnostic rather than exponential failure.

### 7.8 Effects

Compute `maySuspend` after name/type resolution over direct calls, dynamic-call conservatism, intrinsics, and imported summaries. Store a shortest causal path for diagnostics. Recompute affected strongly connected components incrementally.

### 7.9 MIR

Lower all implicit source behavior before backends: left-to-right temporaries
including written construction-field order, exact-error `?` branching, match
decision trees, D042 checked division/remainder, root lifetimes, D041 `defer`
cleanup and panic precedence, scope exit, cancellation edges, calls, and
suspension points.

Implement an unoptimized MIR interpreter early. It is the semantic oracle for VM/AOT optimization and dramatically reduces backend debugging ambiguity.

Every optimization has unit tests, randomized differential tests against unoptimized MIR, and invalidation rules. Start with constant folding, unreachable-block removal, copy propagation, and bounds-check elimination based on proofs; do not block correctness on an advanced optimizer.

## 8. Backend and runtime work

### 8.1 Bytecode and VM

Define opcodes from a single machine-readable table that generates C enums, decoder/verifier metadata, disassembly names, and documentation. Decode with checked cursors. Verify before execution. Keep interpreter dispatch portable; computed-goto acceleration is optional and must retain switch dispatch.

The VM exposes deterministic hooks for scheduler, time, random, I/O, allocation limits, tracing, and snapshots. It must be embeddable without process-global singleton state.

### 8.2 Native C backend

Emit boring, auditable C17 with explicit temporaries and helper calls. Separate generated translation units by stable package/function partitions for incremental C compilation. Write depfiles and use content-addressed object caching.

Generated code never relies on C expression evaluation order, signed overflow, unchecked pointer arithmetic, union type punning, or conservative stack scanning. Compile-pass/run fixtures run at `-O0` and release optimization under every Tier 1 C compiler.

### 8.3 Garbage collector

Stage 0 begins with a precise non-moving mark/sweep collector per isolate. Build root shadow stacks and resumable-frame descriptors before optimizing allocation. Required tests include cycles, deep/wide graphs, interface/closure roots, every enum payload shape, mutation during allocation, nested task suspension, FFI handles, repeated collection at every allocation, and cancellation cleanup.

D007 is not accepted from GC microbenchmarks. Measure allocation count/bytes, retained graph size, collection work, and end-to-end throughput on JSON encode/decode DTO graphs, immutable UI-style trees, request/response models, and collection-heavy transformations, both with and without escape/scalar replacement. Amend the decision before 1.0 if managed-reference records cannot meet an honest workload budget.

No user finalizers. Implement leak reporting for OS resources independently of GC.

### 8.4 Scheduler and I/O

Implement the scheduler behind a platform-neutral poller interface. Start with deterministic in-memory events, then timers, pipes, TCP, DNS/file blocking adapters, HTTP, and TLS adapters. A bounded blocking pool has explicit queue limit, metrics, cancellation, and shutdown.

The C backend and VM call one runtime scheduling contract. Do not maintain semantically separate runtimes.

Task stacks/frames, cancellation masks, cleanup stacks, task tree, and source traces must be present before advertising concurrency. The syntax-freeze prototype gates in the execution spec are mandatory.

### 8.5 Isolates

Each isolate owns heap and scheduler state. Cross-isolate messages use serialized/copy descriptors generated from types; transfer is limited to audited compiler-known buffers/resources. Fuzz message decoding and abort an isolate cleanly on malformed internal protocol rather than corrupting another heap.

## 9. Standard library minimum for production

The standard library is part of the language release and follows the same compatibility policy. Production 1.0 requires coherent packages for:

- prelude: `Option`, `Result`, `Error`, collections, strings, numeric operations;
- `std/io`, `std/fs`, `std/path`, `std/env`, `std/process`;
- `std/time`, timers, cancellation/timeouts;
- `std/task`, bounded channels, isolates;
- `std/net`, DNS, TCP, UDP;
- `std/http` client/server, routing primitives, streaming bodies;
- TLS through supported audited platform/library backends;
- `std/json` with streaming and typed codecs;
- cryptographic hashes and secure random access;
- logging with structured fields and context propagation;
- `std/test`, property generators, benchmark and fuzz integration;
- `std/c` and documented FFI handles.

An API is not production-ready without cancellation, timeouts, resource bounds, partial I/O handling, structured errors, tests on every Tier 1 platform, and observability hooks.

Database drivers may begin outside `std`, but the reference service must exercise at least one production database package through the public package/FFI/runtime model.

## 10. Toolchain implementation

The `ruda` CLI is a thin dispatcher over reusable libraries. Subcommands must not fork separate parsers, resolver configurations, cache keys, or diagnostic renderers.

Implement in this order:

1. `ruda version`, `ruda help` and stable diagnostics;
2. `ruda fmt`, `ruda check`, and the minimum overlay/compiler-service completion owner used to validate dot-call discoverability;
3. `ruda run --engine vm`;
4. `ruda build` and native run;
5. `ruda test`, then `--engine both`;
6. project manifest/lock and offline cache;
7. `ruda doc`, `ruda vet`, `ruda fuzz`, `ruda bench`;
8. registry operations and `ruda audit`.

This order is a vertical dependency order, not permission to finish the whole
I2 language before exercising execution backends. Once the minimally useful
pure subset reaches typed verified MIR, begin the thin I3/I4 path immediately:
source → typed MIR → bytecode VM → generated C → identical observable result.
New I2 surface work must either extend that path or close a documented semantic
blocker; it must not indefinitely postpone the first engine-equivalence slice.

Test every command from temporary directories with spaces, Unicode, read-only files, interrupted writes, stale locks, missing compilers, corrupt caches, and denied capabilities. Official Tier 1 installation must reach native `ruda build` without per-project/user-selected compiler, linker, or SDK configuration; installation/preflight owns the D030 dependency and `ruda doctor` explains it before code generation.

## 11. Package security

N0 naming/distribution identity is a prerequisite for public registry/module namespace implementation. Internal resolver prototypes use replaceable test namespaces such as `ruda-lang.example` and cannot reserve, publish, sign, or document them as a cleared public identity.

The package resolver is deterministic and lock-driven. Implement SHA-256 or stronger content verification using a reviewed implementation; signature verification must use a well-maintained audited dependency or platform facility rather than novel cryptography.

No dependency code executes during fetch, resolution, build configuration, or compile-time evaluation. Archives are checked for path traversal, case collisions, symlink escape, compression bombs, duplicate entries, and size/count limits before extraction.

Registry protocol and trust roots are versioned documents with replay/downgrade protection. Offline builds prove they need no network through integration tests.

## 12. Self-hosting

Self-hosting is a reproducible chain, not a ceremonial rewrite:

```text
trusted C compiler
  -> Stage 0 compiler/runtime written in C17
  -> Stage 1 compiler written in Ruda, built by Stage 0
  -> Stage 2 compiler, built by Stage 1
```

Requirements:

- Stage 0 remains buildable from committed source and documented tools;
- Stage 1 implements the same compiler pipeline through a versioned internal API, not by shelling out to hidden services;
- Stage 1 and Stage 2 compile the conformance suite identically;
- a reproducibility process compares normalized artifacts and investigates every divergence;
- the bootstrap records compiler hashes, source hashes, C compiler identity, and generated C/MIR where applicable;
- the C17 implementation is maintained as the bootstrap root for supported releases, even after day-to-day development moves to Ruda.

Do not begin the full rewrite until the roadmap's bootstrap-language subset, modules, diagnostics, GC, and both engines are stable enough to compile the compiler without privileged syntax. E0 external ergonomics pilots over the working I4–I6 toolchain must also be verified first: D007 allocation behavior, exact-error-mapping noise, dot-call discovery, structured race/select, and native-toolchain installation findings are resolved before expensive self-hosted code locks in an untested surface.

## 13. Documentation deliverables

Maintain alongside implementation:

- language, execution, bytecode, package, FFI, runtime, and CLI specifications;
- generated grammar, opcode, diagnostic-code, and standard-library references;
- tutorial and “effective Ruda” guidance derived from evidence, not folklore;
- compiler architecture and contributor guide;
- unsafe-code and FFI safety guide;
- operations guide for task dumps, GC, profiling, graceful shutdown, and crash reports;
- compatibility and deprecation policy;
- platform tier policy and release checklist;
- every public example compiled by `ruda test` or doc tests.

Documentation code that is not executable must be explicitly tagged illustrative and minimized.

## 14. Production reference application

Before 1.0, build and operate a public reference service using only released Ruda tooling. It must include:

- HTTP/TLS server and client calls;
- JSON request/response streaming;
- database pool and transactions;
- concurrent fan-out with deadlines, D029 structured race/select, and cancellation;
- isolated multi-core workers;
- configuration/env capabilities;
- structured logs, metrics, tracing, and task dumps;
- graceful shutdown with zero leaked tasks/resources;
- unit, integration, fuzz, load, failure-injection, and VM/native tests;
- reproducible container/native deployment and SBOM/audit output.

Run sustained load, connection churn, slow-client, dependency timeout, database restart, disk-full/log failure, signal shutdown, memory pressure, and malformed-input scenarios. Publish latency, throughput, memory, GC, task, startup, and binary-size evidence with hardware/environment.

## 15. Agent execution protocol

For each roadmap gate:

1. read the relevant normative sections;
2. list concrete acceptance cases and negative cases;
3. add failing tests/fixtures and diagnostic expectations;
4. implement the smallest complete vertical slice across required layers;
5. run focused tests, then sanitizers/differential/full gate commands;
6. update specs and generated references if the accepted behavior changed;
7. record command output, platform, hashes, and benchmark evidence in the roadmap;
8. mark the gate `verified` only after every criterion has direct evidence.

Never mark a milestone verified from a narrower smoke test. Never delete or weaken a conformance test merely to align it with one backend. Minimize failures and fix the semantic owner layer.

## 16. Reporting contract

At the end of every iteration report:

- roadmap gate and exact status;
- source/spec/test files changed;
- commands executed with exit status;
- sanitizer/fuzzer/differential evidence;
- performance movement against budgets;
- known risks and tracked removal gates for temporary code;
- next highest-risk vertical slice.

An unfinished gate remains visibly unfinished. “Works on my machine”, unchecked TODOs, parser acceptance, or a green subset cannot be presented as production progress.
