# Marimay design constitution

Status: accepted design baseline.

These principles decide trade-offs when individual syntax preferences conflict. A feature that violates them needs evidence, an explicit decision record, and a migration plan.

## 1. Complexity belongs in one compiler, not every program

Marimay accepts implementation complexity when it removes recurring application complexity. Compiler-inferred task suspension is the canonical example: generating resumable frames twice is harder for the compiler team, but exposing async colors, futures, pinning, and executor compatibility would charge every user forever.

This principle is not a license for hidden magic. The compiler must expose inferred behavior in diagnostics, docs, MIR dumps, tracing, and `mmay explain`.

## 2. One source program has one meaning

VM and native are execution strategies, not dialects. They share parsing, name resolution, typing, exhaustiveness, typed MIR, evaluation order, overflow rules, panic behavior, task semantics, and standard-library contracts. Engine-specific source branches are forbidden in safe portable code.

## 3. Make invalid states hard to represent

Absence uses `Option`. Recoverable failure uses `Result`. Closed alternatives use algebraic enums. Destructuring uses exhaustive matching. Safe code cannot forge enum tags, dereference null, read uninitialized storage, or invoke an unchecked C pointer.

## 4. Make the common path visually linear

Normal application code should read top to bottom. `?`, `defer`, direct-style I/O, first-parameter dot calls, and structured task scopes support this. Hidden non-local exceptions, callback pyramids, and compulsory combinator chains do not.

## 5. Data and behavior are separate, navigation is direct

`type` declares nominal data. `interface` declares a callable contract. An ordinary top-level function declares behavior. When its first parameter is an exact locally owned nominal type, dot notation is an equivalent call form. There is no fourth declaration whose sole job is to state that the other three belong together.

## 6. Structural at boundaries, nominal in data

Records and enums are nominal: accidental field equality must not erase domain meaning. Interfaces are structural: a type that already exposes the exact required dot-call signatures can be used without editing it or naming the interface. Interface compatibility is exact and callable-only; record field shape never creates conformance.

## 7. Safe by default, unsafe visible and narrow

Memory is managed. Expected failures are typed. Mutable fields are explicit. Raw pointers, unchecked casts, ABI calls, and manual layout operations require `unsafe`. An unsafe function documents its invariants, and the standard library wraps unsafe internals with safe contracts.

## 8. Concurrency is structured; parallelism is isolated

Lightweight tasks within an isolate share one cooperatively scheduled heap and cannot outlive their lexical task scope. CPU-parallel work uses separate isolates with no shared mutable heap. Values cross isolate boundaries by checked copy or explicit transfer. This makes data races absent by construction in safe Marimay 1.x without adding ownership syntax.

## 9. Predictability beats clever coercion

Evaluation is left to right. Numeric conversions are explicit. Integer overflow has defined checked behavior. Boolean conditions require `bool`. Imports do not execute code at build time. Macro expansion cannot rewrite arbitrary syntax because user-defined macros do not exist in 1.0.

## 10. The happy path has no tool assembly step

Installing Marimay installs `mmay run`, `mmay build`, `mmay check`, `mmay fmt`, `mmay test`, documentation, package operations, linting, fuzzing, and the runtime. Defaults work without configuration; CI flags are small extensions of the same commands.

## 11. Fast feedback and production speed are separate optimizations

The VM minimizes edit/run latency. Native AOT minimizes deployment overhead and improves steady-state performance. Neither is allowed to weaken semantics. Generic code shares machine representations by default; specialization is an optimizer decision with measured benefit, not a source-level commitment.

## 12. Diagnostics are part of the language

Every rejected program should answer: what failed, where, why, and what concrete change is likely intended. Error ordering is deterministic. Machine-readable diagnostics and stable error codes are required. A new rule without a diagnostic test is incomplete.

## 13. Compatibility is a product feature

Accepted 1.x source continues compiling throughout 1.x unless it relied on explicitly experimental features. Adding a case to a public enum is a breaking API change. Formatter output is versioned with the language toolchain. Package artifacts are immutable and checksummed.

## 14. Features pay rent continuously

Every feature adds grammar, type interactions, diagnostics, formatter behavior, VM lowering, AOT lowering, debugger behavior, documentation, and tests. A proposal must show that its recurring user value exceeds this permanent interaction cost. Syntactic aliases and overlapping abstractions are rejected by default.

## 15. Production claims require evidence

“Production-ready” means published platform tiers, conformance suites, sanitizer-clean runtime tests, fuzzing history, compatibility tests, deterministic package resolution, recovery documentation, benchmark budgets, and at least one reference service operated under load. It does not mean a feature checklist is complete.

## 16. Semantic facts, tools, enforcement, and trust are separate layers

A property belongs to the language only when it changes the meaning or static
validity of a Marimay program. The compiler proves those rules and publishes
stable facts such as typed trees, symbol graphs, effect summaries, MIR, and
diagnostics. Formatter, semantic-diff, dependency-analysis, test, property,
fuzz, and vet tools consume those facts without turning their policies into
source semantics.

Runtime and operating-system boundaries enforce granted resources and limits.
CI and project governance decide required checks, acceptable coverage,
attestation, review authority, and release eligibility. Author identity,
whether code was generated, and whether an agent participated are not Marimay
language concepts. Shipping all first-party workflows through one `mmay`
executable is a cohesive product decision, not a collapse of these
responsibilities.
