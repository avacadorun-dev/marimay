# Compiler architecture research for the C17 bootstrap

Status: non-normative implementation research.
Last reviewed: 2026-08-13.

This note records the external evidence used to shape the physical compiler
architecture. It does not define Marimay source or execution semantics. The
authority order in `AGENTS.md` still applies, and recommendations here are
adopted only where they agree with the normative specifications and accepted
decisions.

## Research question

The immediate problem is not merely that some implementation files are large.
The architectural question is how a portable C17 compiler can keep phase
ownership, validation boundaries, VM/native equivalence, and future
incrementality explicit while the language surface grows.

The reviewed systems do not prescribe one universal compiler layout. They do
converge on several useful properties:

- intermediate forms have explicit owners and invariants;
- lowering happens through staged representations rather than backend repair;
- transformations declare their scope and invalidation effects;
- verifiers run at representation boundaries and, during development, between
  transformations;
- function- or item-sized work units make testing, caching, and parallelism
  tractable;
- generated instruction metadata prevents encoder, decoder, verifier,
  disassembler, and interpreter registries from drifting apart.

## Primary-source matrix

| Source | Evidence | Marimay implication |
|---|---|---|
| [Go compiler README](https://go.dev/src/cmd/compile/README) | The production compiler documents distinct parsing, type checking, IR construction, ordering/desugaring, generic SSA, and target lowering phases. Its ordering phase makes evaluation order explicit before backend work. | Keep D040 evaluation order and other implicit behavior explicit in typed MIR before either engine. Name physical modules after phase responsibility. |
| [rustc compiler overview](https://rustc-dev-guide.rust-lang.org/overview.html) | Major computations are item-keyed queries. MIR is used for flow-sensitive checks, optimization, constant evaluation, and code generation. | Give Stage 0 phase APIs stable session-local keys and function/item granularity, without committing to a persistent query engine yet. |
| [rustc query evaluation model](https://rustc-dev-guide.rust-lang.org/queries/query-evaluation-model-in-detail.html) | Query dependencies are recorded, results are memoized, and dependency cycles are diagnosed. | Hidden whole-compiler mutable state would prevent later incrementality. Phase inputs and dependencies should be explicit now. |
| [rustc incremental compilation](https://rustc-dev-guide.rust-lang.org/queries/incremental-compilation-in-detail.html) | Reuse depends on deterministic queries, dependency tracking, stable fingerprints, and careful treatment of stored results. | Do not add a disk cache to 0.1-dev before symbol identity, fingerprints, and invalidation behavior have direct tests and measurements. |
| [rustc MIR guide](https://rustc-dev-guide.rust-lang.org/mir/index.html) and [MIR queries and passes](https://rustc-dev-guide.rust-lang.org/mir/passes.html) | The MIR representation is defined separately from MIR construction, transformation, and dataflow code; useful intermediate states are exposed through distinct queries. | Split Marimay MIR storage/building, verification, interpretation, and future transforms into separate semantic owners while retaining one public MIR contract. |
| [MLIR pass infrastructure](https://mlir.llvm.org/docs/PassManagement/) | Passes operate on an explicit scope, may fail the pipeline, declare analysis preservation, support instrumentation, and can run a verifier after each pass. | A Marimay transform must state its function/module scope and preserved analyses. A failed verifier stops compilation; it is not a recoverable backend hint. |
| [MLIR developer guide: IR verifier](https://mlir.llvm.org/getting_started/DeveloperGuide/#ir-verifier) | Operation verifiers enforce local invariants; verification order and locality are designed to avoid depending on invalid enclosing state. | Separate local instruction/layout validation from CFG/dataflow validation, with focused negative fixtures for each boundary. |
| [LLVM new pass manager](https://llvm.org/docs/NewPassManager.html) | Analyses are cached at an IR scope, and transforms explicitly preserve, update, or invalidate results. | Future MIR dominator/liveness caches belong to a function analysis owner. A transform cannot silently leave stale analysis state. |
| [Cranelift IR reference](https://github.com/bytecodealliance/wasmtime/blob/main/cranelift/docs/ir.md) | Cranelift compiles functions independently, has an in-memory and textual test/debug form, uses explicit block terminators, typed values, and an IR verifier. | Keep Marimay MIR function-local where possible, add a deterministic dump, and use stable IDs/packed tables rather than cross-owner pointer graphs. Backend-specific C details do not enter semantic MIR. |
| [WebAssembly validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html) | Decoding, static validation, and execution are separate. Validation establishes invariants before instantiation/execution. | Treat `.myb` decoding and bytecode validation as an untrusted-input boundary. The VM executes only a fully verified program. |
| [CPython interpreter internals](https://github.com/python/cpython/blob/main/InternalDocs/interpreter.md) | Instruction definitions and generated cases/metadata are maintained from coordinated declarative inputs rather than unrelated handwritten registries. | Continue using Marimay's opcode TSV as the single metadata source. Generated metadata should cover encoding, operands, verifier facts, names, and documentation; handlers remain ordinary reviewable C. |
| [Adapton, PLDI 2014](https://doi.org/10.1145/2594291.2594324) | Demand-driven incremental computation records dependencies and reuses results while preserving consistency. | This supports a future query-shaped compiler service, but it is not evidence that a persistent incremental engine belongs before the preview path is complete. |

## Adopted patterns

### Phase ownership instead of a line-count rule

A source file should have one semantic owner and one reason to change. File
size is a review signal, not an invariant: a cohesive table may be large, while
a small file that mixes parsing, type repair, code generation, and runtime
policy is already architecturally wrong.

For the C17 bootstrap, the useful seams are:

- representation storage and immutable access;
- construction/lowering;
- local invariant validation;
- CFG and dataflow verification;
- semantic oracle execution;
- bytecode encoding/decoding/verification;
- VM execution;
- C17 emission;
- shared runtime semantics.

### Data-oriented internal representations

Arena- or vector-owned tables with typed indices are preferable to mutually
owning pointer graphs in Stage 0. Builders are the mutating boundary. Consumers
receive const views. This makes allocation failure, bounds, serialization, and
deterministic dumps testable with ordinary C17.

This is an engineering conclusion for Marimay, not a claim that every reviewed
compiler uses the same physical representation.

### Verification at every trust boundary

The frontend may recover for diagnostics, but invalid placeholder state cannot
be presented as valid typed HIR. Typed MIR verification is independent of the
builder. Bytecode verification is independent of the MIR emitter. The VM does
not repair either artifact. In debug and test builds, future transforms verify
their output immediately.

### Query-shaped APIs without premature persistence

The compiler service should be expressible in terms such as
`resolve_package(package)`, `type_check(function)`, `lower_mir(function)`, and
`emit_bytecode(function)`. Stage 0 may execute them eagerly. Persistent caching
is a later checkpoint requiring stable identities, deterministic fingerprints,
dependency/invalidation tests, and performance evidence.

## Rejected applications

- Importing an MLIR-sized dialect/pass framework into the C17 bootstrap would
  add more machinery than the preview requires.
- A different HIR-to-backend path for VM and native would violate the typed-MIR
  convergence contract.
- A single mega-verifier would hide whether an error belongs to instruction
  structure, type/layout identity, CFG, initialization dataflow, or serialized
  bytecode.
- One generated file containing full handler implementations would replace one
  monolith with a harder-to-debug generated monolith. Generate registries and
  metadata; keep semantic code in reviewed C modules.
- Splitting files solely at an arbitrary line threshold would create accidental
  private APIs and cyclic ownership without improving correctness.

## Consequence for the preview

The architecture should be evolved by behavior-preserving extraction
checkpoints interleaved with vertical feature slices. Each extraction preserves
the public C API and test order, runs focused ownership/verifier tests, then the
full applicable regression matrix. New source features must still land through
typed MIR, verified bytecode/VM, generated C17/native, and differential
evidence in one vertical sequence.
