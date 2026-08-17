# I2 generic `Option`/`Result` slice — preview checkpoint 4 — 2026-08-17

Gate: constrained generic functions and nominal payloads through checker,
typed MIR, encoded bytecode/VM, and generated C17/native execution. Checkpoint
status: locally complete; I2, I3, I4, and I5 remain `in-progress`.

Authority: language specification §§5.2, 5.7, and 10.1; standard-library
specification §2; accepted D006 and provisional D013; the compiler architecture
contract; and the checkpoint-4 requirements in `checkpoints.md`. This evidence
does not claim a verified I2 gate or a public 0.1.0 release.

## Delivered

- Generic declarations carry canonical type-parameter identities, nominal
  type arguments, recursive substitution, bounded inference, deterministic
  uninferred/type-argument diagnostics, and interface-constraint checking.
  Recursive calls are instantiated once per concrete signature and do not
  recurse through the publication pass indefinitely.
- The checker-owned prelude defines compiler metadata for `Option<T>` and
  `Result<T,E>`. `Some`/`None`/`Ok`/`Err` use contextual expected types where
  required, including `Result<void,E>`. Generic records and enums use the same
  nominal identity and substitution machinery, including nested payload layout
  and nested generic match binding.
- Postfix `?` lowers to explicit variant tests, checked payload access, error
  propagation, and success unwrapping. `mapError` is explicit and requires a
  visible unary mapper whose parameter exactly matches the input error type;
  it constructs a new `Result<T,F>` without implicit error conversion.
- The same typed MIR supports generic enum payload layout, verifier checks,
  encoded bytecode round trips, VM execution, and portable C17 generation.
  Generic managed payload paths retain the existing precise-rooting contract;
  the differential fixture enables collect-before-allocation execution.
- The D006 compile fixture now checks a real local package graph with a
  namespace import and a signature-only external boundary:
  `public extern "c" fn readText(...): Result<string, Error>;`. The application
  checks `fs.readText(filePath, limit).mapError(configError)?` and returns an
  enclosing `Result`. There is deliberately no fake filesystem body or runtime
  I/O claim in this checkpoint.

## Verification coverage

- Checker positives cover generic functions, user generic records/enums,
  `Option`, `Result`, constructors, match payloads, constraints, `?`, and
  `mapError`.
- Negative coverage asserts deterministic diagnostics for explicit argument
  arity, failed interface constraints, mismatched `?` error types, mismatched
  `mapError` mappers, and public generic signatures. Limits cover type
  arguments, specialization count, nested type growth, and generic recursive
  publication.
- Allocation-failure sweeps run through the checker’s generic setup and
  specialization paths; every failed result is uninitialized and teardown
  asserts both `live_allocations == 0` and `live_bytes == 0`.
- The formatter has a generic declaration and `mapError`/`?` fixture. The public
  example [`examples/generics_checkpoint.ruda`](../../../examples/generics_checkpoint.ruda)
  includes generic identity, `Option`, explicit error mapping, and propagation.
- The VM/native differential source checks `Option`, `Result`, both `Err` and
  `Ok` mapping branches, `?`, exhaustive matches, bytecode encode/decode, and
  generated C17. The MIR oracle, VM, and native executable all produce
  `i64:20`; every checked function passes MIR and bytecode verification.
- Focused package-import coverage verifies the external `readText` signature,
  `load` MIR call count, and MIR verification through the same graph checker.

## Scope boundary

This is a strong reusable generic foundation for the 0.1-dev compiler path, not
the completion of all generics. General `Array<T>`/collections, the runtime
implementation of `std/fs`, capability-checked I/O, public `ruda check`,
production LSP generic completion, and I2/I5 gate verification remain later
work. The signature-only external fixture is intentionally the boundary needed
to prove the D006 type contract without inventing I/O semantics.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | owner/dependency/registration policy clean |
| `./tools/check-c-memory-policy` | 0 | central allocator policy clean |
| `./tools/check-generated` | 0 | bytecode, diagnostic, Unicode, and syntax artifacts current |
| `./tools/check-doc-links` | 0 | documentation links valid |
| `./tools/check-spec-terms` | 0 | normative terminology valid |
| `./tools/check-example-syntax` | 0 | public examples accepted |
| `./tools/check-syntax-coverage` | 0 | 74 syntax kinds and 115 grammar productions covered |
| `ctest --preset debug -R ruda_c_unit --output-on-failure` | 0 | 1/1 passed in 45.64 s |
| `ctest --preset release --output-on-failure` | 0 | 1/1 passed in 53.98 s |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed in 77.06 s; no sanitizer report |

The ASan/UBSan preset has platform leak detection disabled; the allocator
failure suites still assert zero live allocations and zero live bytes after
every tested teardown.
