# Evidence base for Ruda

Status: design input, non-normative.  
Snapshot date: 2026-08-10.

This document separates observed evidence from Ruda design inferences. A survey answer is not automatically a feature request, and a feature in another language is not automatically a good fit. Each inference is tested against Ruda's stated audience: production application developers who want static safety without systems-language ceremony.

## Method

Priority was given to official project surveys, specifications, design documents, and peer-reviewed or author-hosted empirical studies. Informal anecdotes were not used as primary evidence. Percentages from surveys describe their respondents, not every programmer; they indicate persistent friction, not universal preference.

The comparison asks four questions:

1. What do users consistently value?
2. What repeatedly consumes attention without expressing application intent?
3. Which guarantees prevent expensive classes of defects?
4. Which costs are inherent to a goal, and which are historical or architectural accidents?

## Go: evidence and implications

The 2025 Go Developer Survey had 5,379 analyzed respondents and reported 91% overall satisfaction. The project attributes that satisfaction to Go as a holistic platform, especially the domains covered well by its standard library and built-in tooling. Respondents nevertheless asked for more modern language/tool capabilities and repeatedly needed help recalling core `go` commands. Source: [Results from the 2025 Go Developer Survey](https://go.dev/blog/survey2025).

The 2024 H1 survey reports that open responses most often asked for a stronger type system, specifically enums, option types, or sum types. It also records error handling and learning/best-practice friction among the most common challenges. Source: [Go Developer Survey 2024 H1 Results](https://go.dev/blog/survey2024-h1-results).

The Go team's 2025 error-handling retrospective calls verbosity one of Go's oldest persistent complaints. It shows how repeated `if err != nil` branches can dominate the visible function body and notes that repeated syntax proposals failed to achieve broad consensus. The important design lesson is not merely “add `?`”; it is that error propagation must remain visible, composable, and semantically small. Source: [On syntactic support for error handling](https://go.dev/blog/error-syntax).

Go's concurrency model demonstrates the usability of cheap tasks, blocking-looking code, and channels. Official guidance also states that goroutines can leak while blocked and that in-flight goroutines can cause hard-to-diagnose failures and unpredictable memory use. Sources: [Effective Go: concurrency](https://go.dev/doc/effective_go#concurrency), [Go Code Review Comments: goroutine lifetimes](https://go.dev/wiki/CodeReviewComments#goroutine-lifetimes).

Go makes formatting, tests, documentation, vetting, coverage, and related workflows discoverable through one tool. Fuzzing is integrated into the standard toolchain and test model. Sources: [Go command documentation](https://go.dev/doc/cmd), [Go fuzzing](https://go.dev/doc/security/fuzz/).

### Keep from the Go world

- a small, coherent language surface;
- quick builds and a short edit/run loop;
- one obvious first-party command;
- garbage collection for ordinary application memory;
- lightweight tasks with direct-looking control flow;
- structural behavior contracts and composition instead of inheritance;
- a serious standard library and backwards-compatibility discipline.

### Repair rather than copy

- replace integer-shaped pseudo-enums with closed algebraic enums;
- require exhaustive matching over closed data;
- eliminate `nil` from safe source code;
- replace repeated propagation branches with typed `Result` and visible `?`;
- make task lifetime lexical so leaked background work is not the default;
- make command help task-oriented and keep the top-level command set shallow;
- make package provenance, lock data, and auditability first-class.

## Rust: evidence and implications

The 2024 State of Rust Survey reports that users value runtime performance, correctness, compiler stability, documentation, security, and safety. Its leading productivity limitation was slow compilation, followed by debugging support and compiler-artifact disk usage. Open responses also mentioned async struggles and resource-intensive tooling; complexity remained a leading concern about Rust's future. Source: [2024 State of Rust Survey Results](https://blog.rust-lang.org/2025/02/13/2024-State-Of-Rust-Survey-results/).

The 2025 survey reports that slow compile times and storage usage remain prominent, debugging remains a concern, and worry about increasing language complexity persists. Source: [2025 State of Rust Survey Results](https://blog.rust-lang.org/2026/03/02/2025-State-Of-Rust-Survey-results/).

The official Async Rust book explicitly describes async Rust as harder to use and maintain than synchronous Rust. It cites sync/async incompatibility, runtime compatibility constraints, greater exposure to lifetimes and pinning, state-machine-heavy stack traces, and additional failure modes. Its pinning chapter calls pinning notoriously difficult and notes that a new language can choose better foundations. Sources: [The State of Asynchronous Rust](https://rust-lang.github.io/async-book/01_getting_started/03_state_of_async_rust.html), [Pinning](https://rust-lang.github.io/async-book/part-reference/pinning.html).

An empirical study combining observation of Rust learners with analysis of 646 frequently viewed Stack Overflow questions found problems beyond ownership alone: `?` interpretation, block-terminal expressions, automatic dereferencing, late or opaque diagnostics, standard-library use, data structures, and abstraction mechanisms. Source: [A Multimodal Study of Challenges Using Rust](https://mhicks.me/papers/rust-challenges.pdf).

A 2025 empirical study of 301 rustc issues attributes many Rust-specific compiler bugs to the type/lifetime system, trait solving, HIR/MIR processing, and optimization interactions. This is evidence for treating language feature interaction count as a reliability cost, not evidence that advanced types are categorically bad. Source: [An Empirical Study of Rust-Specific Bugs in the rustc Compiler](https://arxiv.org/abs/2503.23985).

### Keep from the Rust world

- closed algebraic data types with payloads;
- exhaustive, destructuring pattern matching;
- `Option` and `Result` in ordinary APIs;
- explicit unsafe boundaries;
- strong diagnostics with spans, explanations, and actionable fixes;
- reproducible packages, unit/integration/doc testing, and hermetic build intent;
- correctness as a language feature rather than a style guide.

### Repair rather than copy

- do not require ownership or lifetime annotations for ordinary application code;
- do not expose `Future`, pinning, executor choice, or async function coloring;
- do not combine structural behavior with a second nominal `impl` ceremony;
- do not make monomorphization of every generic instantiation the only code-generation model;
- do not permit arbitrary build scripts and procedural macros to become an invisible second language;
- keep compiler feature interaction intentionally bounded.

## Familiar syntax without borrowed identity

Cross-language familiarity is useful only while it reduces reading cost. Common C-family forms such as braces, `const`/`let`, `name: Type`, generics, and explicit calls are widely recognizable. Record declarations, construction, named enum payloads, interpolation, visibility, and structured task scopes carry Ruda-specific semantics and therefore use forms that expose those semantics directly.

The inference is to prefer a small coherent grammar over surface compatibility with any existing language. Familiar punctuation must not imply coercion, open object-shape assignment, prototype behavior, nullish values, implicit numeric/enum compatibility, or a dynamic escape tier. User tests evaluate whole Ruda examples rather than asking whether the syntax resembles a named predecessor.

## Structured concurrency evidence

OpenJDK's structured-concurrency design treats related tasks as a single unit. It confines child lifetimes to the parent's lexical block, coordinates join/cancellation/error handling, and makes the task tree observable. This directly addresses the lifetime hazards documented by Go while retaining lightweight direct-style tasks. Source: [JEP 428: Structured Concurrency](https://openjdk.org/jeps/428).

Ruda adopts the invariant, not Java's API: a task may not outlive its nearest `scope` block, and leaving the block cancels and joins unfinished children.

## Pain-point decision matrix

| Evidence | Failure mode to avoid | Ruda response | Required verification |
|---|---|---|---|
| Go users request enums/sum types | invalid integers, non-exhaustive switches, manual tag/payload coupling | nominal algebraic `enum`; exhaustive `match` | compile-fail missing-arm tests; representation fuzzing |
| Go error propagation obscures intent | repetitive branches; weak typed context | `Result<T,E>` and postfix `?`; no exceptions for expected failure | propagation typing and diagnostic tests |
| Go `nil` is pervasive | latent absence and typed-nil surprises | no null value; `Option<T>` only | lexer/type/runtime negative tests |
| Goroutine lifetime can be unclear | leaks, runaway work, delayed cancellation | lexical `scope`; scoped `Task<T>` | cancellation/leak/stress tests |
| Rust ownership gives safety at learning cost | source-level lifetime and borrow fights | tracing GC, safe references, no lifetime syntax | GC/rooting/sanitizer tests |
| Rust async exposes complex machinery | colored APIs, pinning, runtime fragmentation | compiler-inferred suspension; one bundled runtime | direct-style I/O and ABI conformance tests |
| Rust builds can be slow and large | long feedback loops, generic code bloat | VM dev loop; shape-shared generics; optional specialization | clean/incremental budgets and size gates |
| Rust traits are powerful but interact deeply | coherence/associated-type/solver complexity | structural method-only interfaces; no `impl` | conformance and ambiguity tests |
| Familiar syntax can import misleading expectations | accidental complexity or assumed compatibility | compact C-family core with Ruda-native semantic forms | whole-example readability tests and compile-fail safety corpus |
| Package trust is recurring ecosystem work | hidden build execution and dependency uncertainty | immutable registry artifacts; checksummed lock; no install scripts | hermetic/offline/supply-chain tests |

## Design hypotheses that require measurement

The following are hypotheses, not facts, and must be validated with prototypes and user tests:

- `case Pattern => expression` is more immediately readable than Rust-style bare arms for the target audience.
- `fn` and `scope` reduce common-path typing cost while remaining immediately legible in whole programs.
- ordinary first-parameter functions preserve dot-call ergonomics without a second behavior declaration system.
- compiler-inferred suspension is understandable if diagnostics and observability expose suspension points.
- VM-first `ruda run` materially improves feedback time without encouraging VM-only behavior.
- shape sharing provides acceptable generic performance while controlling compile time and artifact size.

Each hypothesis has a gate in the implementation roadmap. Failure of a hypothesis requires a recorded design revision, not quiet semantic drift.

## Local algorithm experiment

The 2026-08-18 Ruda/Go/Rust algorithm repository adds local evidence about
native array-heavy workloads, runtime resource limits, benchmark comparability,
and release-profile selection. The observations and optimization hypotheses are
recorded in
[`algorithm-benchmark-findings.md`](algorithm-benchmark-findings.md).

The same exercise exposed recurring source verbosity for nested managed arrays.
The non-normative `[][]int` design proposal and its interaction with tuple
types, `Slice<T>`, formatting, and migration are recorded separately in
[`array-type-shorthand-proposal.md`](../design/array-type-shorthand-proposal.md).
