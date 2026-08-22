# Task for agent: atomic post-checkpoint-12 follow-ups

## Objective

Audit the problems discovered by the Marimay/Go/Rust algorithm exercise and append
a dependency-ordered set of fully atomic follow-up checkpoints to
`checkpoints.md` after the existing checkpoint 12.

This is a planning and documentation task only. Do not implement language,
compiler, runtime, standard-library, benchmark, or performance changes. Do not
mark any roadmap milestone verified. Do not commit or push unless the user
separately requests it.

The output must leave `checkpoints.md` ready for another implementation agent to
execute sequentially without having to rediscover scope, authority, owners,
tests, or acceptance commands.

## File ownership and edit boundary

The only file you may edit is:

```text
checkpoints.md
```

Read all other files in read-only mode. In particular:

- do not edit `goal-objective.md`;
- do not edit either findings/proposal note;
- do not edit specification, decisions, roadmap, evidence, source, tests, or
  the sibling `mmay-algotithms` repository;
- do not rewrite, renumber, reorder, or silently weaken checkpoints 0–12;
- append the new block strictly after the final line of checkpoint 12.

`checkpoints.md` and `goal-objective.md` may be untracked user-owned files.
Preserve their current bytes outside the appended region. Inspect Git status
before writing and report pre-existing unrelated changes without touching them.

## Mandatory reading by the primary agent

Read these files completely before planning:

1. `AGENTS.md`;
2. `goal-objective.md`;
3. `checkpoints.md`;
4. `docs/research/algorithm-benchmark-findings.md`;
5. `docs/design/array-type-shorthand-proposal.md`;
6. `docs/design/principles.md`;
7. `docs/design/decisions.md`, especially D003, D013, D033, D040, D043,
   D045, D046 and any later decisions that constrain arrays, generics,
   imports, diagnostics, VM/native equivalence, or native builds;
8. `docs/spec/language.md`, especially types, aliases, generics, arrays,
   indexing, iteration, imports, dot calls, and evaluation order;
9. `docs/spec/standard-library.md`, especially `Array<T>`, `Slice<T>`,
   `Option`, `Result`, collection mutation and iteration invalidation;
10. `docs/spec/execution.md`, especially managed arrays, bytecode array type
    tables, memory management, resource limits, safe indexing and native
    equivalence;
11. `docs/spec/toolchain.md`, especially `mmay build`, output profiles and
    release/native behavior;
12. `docs/spec/diagnostics.md`, especially bounds panics and `I0002`;
13. `docs/implementation/compiler-architecture.md`;
14. `docs/implementation/test-strategy.md`;
15. `docs/implementation/roadmap.md`;
16. `IMPLEMENTATION-BRIEF.md`.

Do not delegate interpretation of those documents. Apply the authority order
from `AGENTS.md`. If the notes conflict with accepted decisions or normative
specification, record the conflict in the proposed checkpoint and require the
decision/spec change before implementation. Do not resolve it silently inside
the plan.

## Benchmark evidence to inspect

The comparison repository is the sibling path `../mmay-algotithms`. Inspect at
least:

```text
../mmay-algotithms/README.md
../mmay-algotithms/benchmarks/README.md
../mmay-algotithms/benchmarks/run.py
../mmay-algotithms/benchmarks/results/latest.md
../mmay-algotithms/benchmarks/results/latest.json
../mmay-algotithms/benchmarks/results/debug-runtime-baseline.md
../mmay-algotithms/benchmarks/mmay/core/benchmarks.my
../mmay-algotithms/mmay/
../mmay-algotithms/go/cmd/benchmark/main.go
../mmay-algotithms/go/sorting/
../mmay-algotithms/go/structures/
../mmay-algotithms/go/graphs/
../mmay-algotithms/rust/src/bin/benchmark.rs
../mmay-algotithms/rust/src/sorting/
../mmay-algotithms/rust/src/structures/
../mmay-algotithms/rust/src/graphs/
```

Confirm rather than copy the notes blindly:

- all current benchmark outputs use the same expected checksum;
- current DSU implementations all use path compression and union by rank;
- the latest report names `build/release/mmay` as the Marimay compiler executable;
- compilation is excluded and `mmay run`/VM is not mixed into native timings;
- the Marimay benchmark still has documented inline/cross-package and data-layout
  comparability limitations;
- the archived debug-runtime numbers are not reused as release performance
  acceptance values.

Do not rerun the full multi-minute benchmark unless a fact cannot otherwise be
validated. Read the raw JSON and source first. A small focused read-only probe
is acceptable when necessary.

## Implementation owners and code paths to inspect

Use `rg` to locate the current semantic owners; do not assume this list remains
complete or that work belongs in an existing coordinator. Read the focused
architecture contract and focused tests before assigning any future change to
a file.

At minimum inspect:

```text
bootstrap/src/parse/parser_expression.c
bootstrap/src/check/checker_generic_types.c
bootstrap/src/check/array.c
bootstrap/src/check/                 # import/name/dot-call owners
bootstrap/src/format/
bootstrap/src/mir/
bootstrap/src/bytecode/
bootstrap/src/native/build.c
bootstrap/src/native/c_emitter*.c
runtime/common/managed.c
runtime/common/managed_internal.c
runtime/include/mmay/runtime/core.h
runtime/include/mmay/runtime/array.h
tests/c-unit/                         # focused parser/checker/MIR/VM/native/runtime tests
tools/check-architecture
tools/check-c-memory-policy
```

Verify these observations from current code:

1. `mmay build --release` compiles generated C with release flags but the
   invoking compiler build embeds a particular runtime archive path;
2. generated scalar array get/set paths repeatedly perform managed resolution,
   descriptor, length, bounds, status, and cell conversion work;
3. `MarimayManagedCell` layout is wider than a raw 64-bit scalar on the measured
   64-bit target;
4. the default managed live-byte limit is 64 MiB and array capacity grows
   geometrically;
5. the type parser currently uses bracket syntax for tuple types and has no
   valid empty tuple type, making prefix `[]T` syntactically distinguishable in
   type position;
6. `Slice<T>` is semantically distinct and `[]T` must not accidentally acquire
   slice semantics;
7. current cross-package execution/name-resolution failures are real minimal
   toolchain gaps rather than unsupported language semantics already rejected
   by the specification.

Where a fact is still only a performance hypothesis, label it as such in the
checkpoint. Do not present a plausible optimization as a proven cause.

## Problem inventory that must be mapped

Build an issue-to-checkpoint traceability table before writing the new section.
Every item below must have exactly one primary checkpoint, although a later
integration checkpoint may revalidate all of them. If two items genuinely need
one vertical change, explain why combining them is still atomic. If one item
needs multiple sequential vertical changes, explain the dependency and ensure
each checkpoint independently ends green.

### Toolchain and measurement

- output-profile coherence: `mmay build --release` must not silently depend on
  a debug native-runtime archive embedded by a debug compiler build;
- benchmark fidelity: Marimay must eventually exercise the same reusable package
  boundaries and equivalent algorithmic work as Go/Rust;
- focused performance probes and reproducible metadata before hard budgets are
  introduced;
- final release-native rebaseline on Tier 1 platforms, with raw results,
  dispersion, toolchain paths, and regression policy.

### Cross-package and collection usability

- imported/cross-package `Array<T>` and nested `Array<Array<T>>` execution
  through checker, package metadata, typed MIR, bytecode, VM and native;
- imported generic `Option`/`Result` constructors and imported enum results;
- receiver-directed collection names so ordinary nominal `stack.push()` and
  `stack.pop()` do not collide with compiler-known array operations;
- missing executable collection operations needed by real structures:
  `pop`, `truncate`, `clear`, and non-observable `reserve`, including failure
  and iterator-invalidation contracts.

### Source ergonomics

- compact managed-array type spelling: `[]int` for the existing `Array<int>`
  and `[][]int` for nested managed arrays;
- explicit distinction from `Slice<T>`;
- canonical formatter/diagnostic/IDE spelling and a pre-1.0 migration policy
  that does not leave two permanent canonical spellings;
- contextual empty construction such as `let rows: [][]int = []` without
  bundling unrelated inference changes.

This is a semantic/source change. Its checkpoint must include the decision or
D045 amendment, normative specification, parser/checker/import behavior,
formatter fixtures, diagnostics, user examples, migration/removal gate, and
VM/native proof in the same vertical change. Do not create parser-only or
formatter-only checkpoints for it.

### Native array performance and resources

- redundant exact-type, handle, length and bounds work in hot array accesses;
- MIR range facts and safe bounds-check elimination/hoisting while preserving
  evaluation order, panic identity and source span;
- dense native storage for raw scalar arrays instead of reference-sized union
  cells, without changing source identity, tracing, bytecode or VM/native
  behavior;
- the optimization boundary between generated C and the separately linked
  runtime archive, including a measured narrow fast-path or LTO decision;
- non-observable capacity planning and exact/reserved construction;
- documented configurable heap/isolate budget and actionable `I0002`
  diagnostics that distinguish semantic length, live-byte budget and
  allocation failure;
- nested-array traversal costs after scalar array primitives are improved;
- removal of provably redundant checked integer operations using MIR range
  facts, without weakening release overflow semantics.

### Regression closure

- reusable sorting, stack, queue, heap, DSU, BFS and Dijkstra examples must run
  through `fmt`, `check`, VM run/test, native build/run and `--engine both`;
- exact checksum/stdout/panic/exit equivalence must survive every optimization;
- allocator failure coverage, `live_allocations == 0`, `live_bytes == 0`,
  sanitizers, architecture policy, generated freshness and Tier 1 evidence must
  close the sequence.

## Atomicity rules for the new checkpoints

Append a clearly labelled post-preview follow-up section and number its first
checkpoint `13`. Continue monotonically through `N`.

An atomic checkpoint is one independently reviewable vertical outcome, not one
file or compiler phase. Apply these rules:

1. Do not split one source semantic feature into parser, checker, VM and native
   checkpoints. A semantic feature lands decision/spec/tests/formatter/examples
   and complete engine behavior together.
2. Do not combine unrelated syntax, correctness, resource-policy and
   optimization work merely because they touch arrays.
3. Put correctness and benchmark-fidelity repairs before performance budgets.
4. Put measurement infrastructure before an optimization whose acceptance
   depends on its speed or memory effect.
5. Put general scalar-array work before nested-array or graph-specific work.
6. Put semantic-preserving MIR analysis before backend shortcuts that would
   duplicate source semantics.
7. Each checkpoint starts from green main and ends with its own focused tests,
   VM/native differential coverage, architecture gate, evidence update, commit
   and push contract.
8. A checkpoint may depend only on earlier checkpoints. State those
   dependencies explicitly.
9. Do not invent a performance threshold from three local Apple M2 samples. If
   no justified budget exists, first create a checkpoint that establishes the
   methodology, noise envelope and Tier 1 baseline; later checkpoints may use
   that accepted budget.
10. Optimizations may not change safe Marimay semantics, panic ordering, integer
    overflow behavior, array identity, iteration invalidation, resource
    classification, or VM/native observable results.
11. Any allocation-capable C API change must require cleanup on success and all
    material errors, deterministic allocation-failure coverage, and zero live
    allocation/byte assertions after teardown.
12. No future implementation checkpoint may add feature logic to an excepted
    or over-limit coordinator. Require a focused owner split first when the
    architecture contract demands it.

Checkpoint 12 may remain the original preview integration baseline even though
its title says “Final”. Add a transition explaining that checkpoints 13…N are
mandatory post-integration findings from real 0.1-dev algorithm testing. Do not
edit the content of checkpoint 12 just to rename history.

## Required structure of every appended checkpoint

Every checkpoint 13…N must include enough concrete information to execute it:

- a narrow title and one-sentence user-visible outcome;
- why it is separate and which earlier checkpoint it depends on;
- normative authority and any unresolved decision gate;
- confirmed evidence and the exact limitation being removed;
- in-scope and explicitly out-of-scope work;
- expected production owner modules and focused test owners, phrased so the
  implementer must re-check architecture/line limits before editing;
- complete semantic/toolchain/runtime behavior across all affected layers;
- positive, negative, diagnostic, formatter, import, allocation-failure,
  VM/native differential and regression tests as applicable;
- exact focused acceptance commands;
- the full applicable validation tail, including at least:

```sh
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-generated
./tools/check-doc-links
./tools/check-spec-terms
cmake --build --preset debug --parallel
ctest --preset debug --output-on-failure
cmake --build --preset release --parallel
ctest --preset release --output-on-failure
cmake --build --preset asan-ubsan --parallel
ctest --preset asan-ubsan --output-on-failure
```

- benchmark/profiling commands and acceptance evidence when performance or
  memory is part of the checkpoint;
- required decision/spec/example/roadmap/evidence updates;
- a removal gate for every temporary compatibility path or workaround;
- the small commit and immediate push requirement already used by checkpoints
  0–12;
- an honest completion rule that forbids “implemented” or “verified” when any
  required platform/evidence gate is missing.

Do not paste the entire generic validation block mechanically when a checkpoint
needs additional focused commands. Add those commands explicitly. Conversely,
do not weaken the common gates because a change appears performance-only.

## Required sequence closure

The last appended checkpoint must be a post-optimization integration and
rebaseline checkpoint. It must not introduce new semantics. It must:

- remove benchmark-only Marimay workarounds that earlier follow-ups made obsolete;
- run the reusable algorithms through package boundaries on VM and native;
- compare exact outputs/checksums/panics/exit codes;
- record Debug, true Release, ASan/UBSan and Tier 1 results;
- record runtime/compiler artifact paths so the debug-runtime trap cannot recur;
- publish raw benchmark samples plus median and dispersion;
- compare against the accepted baseline/budget without making broad language
  performance claims;
- update roadmap/evidence honestly and list any remaining limitations.

## Self-review before finishing

Before returning, perform all of the following:

1. Verify that the original checkpoints 0–12 are byte-for-byte unchanged.
2. Verify that the new numbering is contiguous from 13 through N.
3. Produce an issue-to-primary-checkpoint matrix and confirm every inventory
   item above appears exactly once as a primary outcome.
4. Check that no checkpoint is merely “parser support”, “write tests”, “add
   docs”, or another incomplete horizontal slice.
5. Check that syntax/API changes carry decision/spec/formatter/examples and
   full differential coverage in the same checkpoint.
6. Check that performance hypotheses are labelled and receive measurement
   before optimization acceptance.
7. Check that checkpoint dependencies form an acyclic forward-only sequence.
8. Run:

```sh
./tools/check-doc-links
./tools/check-spec-terms
./tools/check-architecture
git diff --check
```

If `checkpoints.md` remains untracked and ordinary `git diff` cannot show its
change, compare the saved original prefix with the final file and report the
method used to prove that only an append occurred.

## Final response format

Return a concise report containing:

1. the appended checkpoint number range and titles;
2. the issue-to-checkpoint mapping;
3. the dependency order and why it is safe;
4. files inspected and the only file changed;
5. validation commands with exit codes;
6. unresolved semantic decisions or evidence gaps;
7. confirmation that no implementation, roadmap verification, commit, push, or
   modification of checkpoints 0–12 occurred.
