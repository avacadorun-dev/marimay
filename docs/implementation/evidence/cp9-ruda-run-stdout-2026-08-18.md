# `ruda run` and D046 stdout — preview checkpoint 9 — 2026-08-18

Checkpoint status: locally complete. I2–I5 remain `in-progress`; I6 remains
`not-started`. This is the closed local VM execution slice, not the native
`ruda build` checkpoint, the task-aware asynchronous I/O runtime, or a public
0.1.0 release claim.

Authority: accepted D046 and D049; language §1; toolchain §§1, 2, and 6.2;
diagnostics §§9 and 13; standard-library I/O contracts; the compiler
architecture contract; and checkpoint 9 in `checkpoints.md`.

## Delivered surface

- `ruda run [target] [-- arguments]` reuses the checkpoint-8 disk target and
  package discovery owner. Standalone files are rooted at `script`; module and
  package targets preserve the selected canonical import path.
- Entry selection accepts only an exact root-package, non-external
  `fn main(): void` or `fn main(): Result<void, E>`. The second form requires a
  root-visible non-external exact `message(E): string` in the D003 dot-call
  set. Same-named functions in other packages, parameters, tests/fuzz targets,
  unrelated `Result` types, and non-void `Ok` payloads are rejected with
  `T0017`.
- The public run path is checker → verified typed MIR → canonical bytecode
  lowering → program encode → fresh decode → complete program verification →
  VM execution. It never executes the pre-encoding object or an unchecked
  fallback.
- Local run grants only the D046 `io.stdout` capability by default. Program
  arguments begin at the first `--`; the bounded slice is retained but is not
  exposed because the preview has no accepted source arguments API.
- The physical `std/io/io.ruda` surface and the embedded bootstrap standard
  package both provide the exact D046 `Error`, `message`, and
  `println(string): Result<void, Error>` declarations. `std/convert` remains a
  separate formatter package.
- `println` is a dedicated checked MIR and bytecode instruction. Its
  verifiers require the exact string operand and structural
  `Result<void, std/io.Error>` layouts. The MIR oracle, decoded VM, and
  generated C17 construct `Ok()` before publication, check the grant before
  staging bytes, append exactly one LF, and construct the precise denial or
  write-failure `Err` value.
- The runtime stdout owner accepts one complete line buffer per publication.
  Empty strings, embedded NUL, and payload newlines retain their exact bytes.
  A denied grant never calls the sink. A rejected sink returns `WriteFailed`;
  the test capture proves it publishes zero bytes for that attempt.
- Root `void`/`Ok()` exits 0. Root `Err` renders `G4003` through the selected
  `message` function and exits 1. Counted diagnostic messages retain embedded
  NUL bytes in human output and encode them as `\u0000` in NDJSON.
  Explicit, arithmetic, and bounds panic identities are `G4000`, `G4001`, and
  `G4002`, all on stderr with exit 1. Pipeline/integrity failures are
  `B0001`/`I0002` and exit 70.
- Public matrix execution now emits exactly `415.0\n`; no `ok:f64:` engine
  harness envelope reaches user stdout.

## Requirements-to-tests coverage

| Checkpoint-9 claim | Executable evidence |
|---|---|
| exact entry and root package | `test_check_entry.c` covers both accepted signatures, missing/wrong-package main, parameters, non-void returns, missing/private cross-package Error contract, and invalid API inputs |
| canonical checked pipeline | CLI process tests run real source; `test_cp9_io_pipeline.c` checks verified MIR, bytecode lowering, encode/decode, decoded-program verification, and VM execution |
| malformed verifier rejection | the focused pipeline mutates the MIR source register and decoded bytecode operand of `IO_PRINTLN`; both verifiers reject the exact type mismatch before execution |
| D046 exact bytes | runtime, MIR, VM, and CLI tests cover empty line, embedded NUL, embedded newline, alias import, full-line callback count, and `stdout-bytes.ruda` exact byte comparison |
| capability and write failure | runtime/MIR/VM tests cover grant, denial, and sink rejection; denial makes zero callbacks, writer rejection returns root `G4003`, and the capture remains empty |
| publish-before-Ok ownership | MIR and VM fail-every-allocation sweeps use a void entry whose final effect is `println`; failed attempts publish no bytes, every teardown has zero live allocations/bytes, and the first success publishes one whole line |
| native semantic sibling | generated C17 contains the same I/O instruction and compiles/runs under strict warnings at `-O0` and `-O2`; a second pair uses `RUDA_G_STDOUT_GRANTED=0` and produces no program line |
| acceptance language families | `acceptance.ruda` executes scalar arithmetic, Fibonacci, factorial, array sorting, records, enums, `Option`, `Result`, and postfix `?` before printing `cp9 acceptance ok\n` |
| matrix | the real process fixture runs `examples/matrix.ruda`, asserts exit 0 and exact `415.0\n`, and rejects the internal hex envelope |
| stable outcomes and separation | in-process writers independently assert stdout and stderr for success, expected error, invalid/missing entry, explicit panic, bounds, overflow, division by zero, invalid shift, JSON, writer failure, and invalid native-engine selection |
| command allocation ownership | `ruda run` fail-every-allocation traverses discovery, embedded standard packages, checking, entry selection, lowering, codec, VM setup, execution, and teardown to first success; every index asserts both counters are zero |
| formatter/public syntax | every `tests/run/cp9` source and `std/io/io.ruda` passes canonical `ruda fmt --check`; the existing stdout syntax/formatter fixtures remain registered |

The allocation sweeps found and closed a target-owner leak: standalone entry
selection and package-view construction could overwrite an already owned
selected import path. The owner now disposes before replacement and the
standalone path is populated once. Both the older `ruda check` sweep and the
new `ruda run` sweep prove zero live allocations and bytes at every reachable
deterministic failure index.

## Observed process outcomes

| Program | Exit | Exact observable line |
|---|---:|---|
| matrix | 0 | stdout `415.0\n` |
| checkpoint acceptance | 0 | stdout `cp9 acceptance ok\n` |
| root expected error | 1 | stderr `error[G4003]: application failed\n` |
| explicit panic | 1 | stderr `panic[G4000]: explicit failure\n` |
| bounds panic | 1 | stderr `panic[G4002]: array index 2 is outside length 1\n` |
| integer overflow | 1 | stderr `panic[G4001]: integer overflow\n` |
| division by zero | 1 | stderr `panic[G4001]: integer division by zero\n` |
| invalid shift | 1 | stderr `panic[G4001]: integer shift count is outside width\n` |
| invalid entry | 1 | stderr `T0017` |
| `--engine native` in checkpoint 9 | 2 | stderr `U0005` |

## Scope boundary and residual risk

- Checkpoint 10 still owns public native artifact construction, the build
  cache, native `main` result adaptation, and public VM/native executable
  stdout/stderr/exit differential. Checkpoint 9 only validates the generated
  C17 I/O semantic sibling through the internal differential harness.
- I6 remains `not-started`. The Stage 0 stdout owner is synchronous and has no
  task parking, scheduler integration, or concurrent-isolate serialization.
  D046 explicitly permits this blocking lowering until I6.
- A host stream that has already accepted a partial low-level write can retain
  that prefix on failure as allowed by D046. The VM/test sink boundary is
  atomic and the deterministic writer-failure fixtures publish zero bytes.
- Program arguments are parsed only at the tool boundary; there is no source
  `args` API. Filesystem, network, process, and dependency capability work is
  not implied by the `io.stdout` grant.
- This is local macOS arm64 evidence. Tier 1 CI and later release gates remain
  separate; no I2–I5 gate is marked `verified` and no public release is
  declared.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | bounded owners and all production/test registrations |
| `./tools/check-c-memory-policy` | 0 | central allocator policy and no direct production heap calls |
| `./tools/check-generated` | 0 | opcode 87 and diagnostic registries/generated artifacts fresh |
| `./tools/check-doc-links` | 0 | documentation links |
| `./tools/check-spec-terms` | 0 | normative terminology and D049 decision registry |
| `./tools/check-example-syntax` | 0 | public example syntax |
| `./tools/check-syntax-coverage` | 0 | 75/75 syntax kinds and 115/115 grammar productions covered |
| `ruda fmt --check tests/run/cp9` | 0 | all run fixtures canonical |
| `ruda fmt --check std/io/io.ruda` | 0 | standard I/O source canonical |
| `git diff --check` | 0 | whitespace validation |
| `cmake --build --preset debug --parallel 4` | 0 | strict debug C17 build |
| `ctest --preset debug --output-on-failure` | 0 | full CTest, 164.86 seconds |
| `cmake --build --preset release --parallel 4` | 0 | optimized strict C17 build |
| `ctest --preset release --output-on-failure` | 0 | full CTest, 155.10 seconds |
| `cmake --build --preset asan-ubsan --parallel 4` | 0 | instrumented C17 build |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | full sanitizer CTest, 220.81 seconds |
