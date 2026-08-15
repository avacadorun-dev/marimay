# V0 nominal engines — checkpoint 1 — 2026-08-15

Gate: preview checkpoint 1, record/enum/non-generic-interface execution through
the common typed-MIR spine. Checkpoint status: closed against the configured
Gitea Linux x86_64 preview gate by
[run 29](http://ci.example:3000/OS/Ruda/actions/runs/29). I2, I3, I4, and I5
remain `in-progress`.

Baseline: `efab86a9d2bee463abf5e3a4295df4c93215bb0d`
(`test: dispose decoded bytecode program`).

Authority: language §§5.4–5.5, §5.8, and §9; execution §§2, 5, 9, and 13;
accepted D003, D004, D005, D007, and D040; the diagnostic and test-strategy
record, enum, identity, interface, malformed-program, rooting, allocation, and
differential contracts. This checkpoint implements accepted semantics; it does
not introduce a new source-language decision.

## Delivered vertical slice

- Records execute as managed nominal values in the MIR oracle, encoded VM, and
  generated C17. Construction, projection, mutable fields, alias-preserving
  mutation, and exact same-nominal `identity.same` are covered.
- Algebraic enums carry scalar, string, record, enum, and interface payloads.
  Nested exhaustive patterns lower to explicit false-edge CFG, preserve guards
  and useful witnesses, and execute consistently in all three engines.
- Structural non-generic interfaces use the existing D003-owned free/dot-call
  symbol set. The checker publishes exact interface and concrete payload
  layouts; MIR and bytecode verifiers validate construction and call slots;
  the VM and native emitter dispatch the selected ordinary function without a
  second source-level method system.
- D040 source order is preserved independently of declaration order for named
  record fields and named enum payloads. The public fixture also proves that
  identity operands are evaluated exactly once, left to right.
- `examples/nominal_cp1.ruda` is parsed and formatted as public source and is
  executed by the conformance harness. The same checked package passes explicit
  MIR verification, the MIR oracle, bytecode lowering, encode/decode and
  program verification, collect-before-allocation VM execution, and strict
  generated C17 at `-O0` and `-O2`; every engine returns exact `i64` value `7`.

## Security, ownership, and hostile-input closure

- Program verification canonicalizes record, enum, and interface schemas by
  nominal symbol across functions and rejects cross-kind or structurally
  inconsistent reuse before execution. Recursive schemas are compared with a
  bounded worklist.
- The bytecode verifier has focused invalid-register, uninitialized-value,
  bounds, exact-layout, mutability, variant-provenance, construction, call-slot,
  and signature cases for every new opcode. Interface-call function scans are
  charged to the verification work budget.
- VM descriptor caches use canonical nominal symbols, stable owned storage,
  publish-then-wire recursion, and failure-atomic rollback. Record and enum host
  arguments require the exact canonical descriptor; wrong kind/layout,
  stale-generation, foreign-heap, and forged-interface payloads are rejected
  before receiver substitution.
- Precise roots cover every managed nominal/interface register across calls and
  allocation. String payload conversion, recursive descriptors, cross-function
  identity, interface values, checker publication, MIR-to-bytecode lowering,
  codec, verifier, and managed runtime paths have deterministic failure sweeps.
  Each complete teardown asserts both `live_allocations == 0` and
  `live_bytes == 0`; failed recursive cache construction is retried on the same
  initialized execution.
- The public interface diagnostic matrix covers missing or mismatched methods,
  unknown methods, arity, ineligible D003 candidates, visibility, and stable
  codes/spans.

## Checkpoint-0 architecture preservation

The feature was added through independently compiled semantic owners rather
than reopening the former monoliths. The current tree contains 115 handwritten
production/platform and 99 unit/fuzz C translation units. The largest
production owner is 1,186 lines and the largest test owner is 1,175 lines; the
largest owner-private header is 270 lines, the largest owner API header is 139
lines, and the largest handwritten public owner header is exactly 160 lines.
The architecture exception registry remains empty.

The architecture gate now discovers all handwritten public headers below both
`bootstrap/include/ruda/` and `runtime/include/ruda/`, not only bytecode
headers. Its built-in policy regression proves the exact 160/161 boundary and
discovery of compiler and runtime public-header families.

## Local validation

Environment:

- macOS Darwin 25.3.0, arm64;
- Apple Clang 17.0.0;
- CMake 4.4.2.

Commands and results:

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | all C owners, headers, dependencies, registration, and exception policy clean |
| `./tools/check-c-memory-policy` | 0 | no direct production heap calls outside the allocator boundary |
| `./tools/check-generated` | 0 | generated opcode and syntax artifacts current |
| `./tools/check-doc-links` | 0 | documentation links valid |
| `./tools/check-spec-terms` | 0 | normative terminology valid |
| `./tools/check-example-syntax` | 0 | public examples satisfy the syntax registry |
| `./tools/check-syntax-coverage` | 0 | 74 syntax kinds and 115 grammar productions covered, 0 open |
| `git diff --check` | 0 | patch whitespace clean |
| `cmake --preset debug && cmake --build --preset debug --parallel && ctest --preset debug --output-on-failure` | 0 | full C suite passed, 1/1, 57.89 s |
| `cmake --preset release && cmake --build --preset release --parallel && ctest --preset release --output-on-failure` | 0 | optimized full C suite passed, 1/1, 39.20 s |
| `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan --parallel && ctest --preset asan-ubsan --output-on-failure` | 0 | ASan+UBSan full C suite passed, 1/1, 78.03 s |

Independent read-only audits found and drove closure of canonical schema,
recursive cache, interface-valued payload, forged host value, MIR-verifier,
MIR-oracle, allocation-failure, architecture-gate, and source-differential
gaps before this evidence was recorded.

## Pushed Linux validation

The first pushed checkpoint run exposed undefined behavior from passing the
null storage of an empty nominal construction to `memset`, even with a zero
byte count. Commit `bcf7c37` guards all optional nominal buffers before
initialization. Its follow-up Linux LSan run then found that the checker
resolution engine differential did not dispose its successfully lowered
bytecode program. Commit `6781ef7` adds the missing success and early-cleanup
teardown.

[Gitea run 29](http://ci.example:3000/OS/Ruda/actions/runs/29) passed all seven
configured jobs: documentation/architecture invariants, Clang and GCC Debug
and Release, ASan+UBSan+LSan, and TSan. The sanitizer job passed the complete
C unit executable without an undefined-behavior report or leak report.

## Remaining verification and boundary

This evidence closes preview checkpoint 1 against its configured pushed Gitea
Linux x86_64 gate. It does not mark any roadmap gate `verified`: the broader
Tier 1 Linux arm64, macOS arm64/x86_64, and Windows x86_64 jobs still require
linked CI evidence before a milestone can be called verified across Tier 1.

Generic payloads and general `Array<T>` remain checkpoint 4. Package/import
resolution and D043 invariance remain checkpoint 2. Public `ruda check`,
`ruda run`, `ruda build`, and `ruda test` wiring remains later preview work.
