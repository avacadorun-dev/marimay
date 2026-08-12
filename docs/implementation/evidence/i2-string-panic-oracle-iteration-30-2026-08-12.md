# I2 string literals and explicit-panic MIR oracle — iteration 30 — 2026-08-12

Iteration status: complete; gate status is `in-progress`.  
Scope: exact immutable string literals and explicit `panic(message)` through
the current source checker, typed MIR verifier, and executable MIR oracle.

## Normative contract fixed and implemented

- D024/D032 remain the authority for ordinary/raw string decoding and canonical
  spelling. The checker and formatter now consume one exact-byte decoder rather
  than maintaining independent escape semantics.
- D036 fixes `panic(message)` as an expression requiring exactly one `string`,
  having type `never`, and producing runtime code `G4000` with the exact message
  bytes and complete call-expression span.
- Language §10.2, diagnostics §9, execution §13.1, LANG-026, and the consistency
  audit record the same observable contract.
- The typed MIR represents explicit panic as a terminator consuming an assigned
  string register. It is not encoded as a return, process abort, host-language
  exception, or backend-specific trap.

## Implemented

- Added a shared bounded ordinary/raw string-literal decoder that preserves
  exact UTF-8 and embedded zero bytes and distinguishes invalid input from
  allocation failure.
- Replaced formatter-local decoding with the shared decoder, so canonical
  spelling and checker runtime values cannot drift through duplicated escape
  handling.
- Added immutable string constants, string registers, owned execution strings,
  bounded string pools, and exact-byte inspection to the MIR oracle.
- Added a typed `PANIC` terminator and verifier rules for the assigned string
  operand, invalid IDs, type mismatches, malformed UTF-8 pool entries, and
  bounded verification work.
- Added caller-owned panic observation containing `G4000`, exact message bytes,
  source function symbol, and call span. Nested direct calls preserve the
  callee that executed the panic.
- The checker accepts string literals in the implemented expression subset,
  lowers `panic(message)` as `never`, and propagates bottom behavior through
  matches, conditions, ordinary calls, and argument evaluation already covered
  by the current CFG slice.

## Coverage added

- Shared decoder tests cover ordinary and raw spelling, simple escapes, a
  Unicode scalar escape, embedded zero, invalid spelling, and allocator
  failure without leaks.
- Checker conformance covers literal and computed panic messages, ordinary/raw
  strings, Unicode and embedded-zero results, nested calls, panic in match/if
  and argument positions, and all-panic matches.
- Checker negative fixtures cover missing/extra arguments, a non-string
  argument, and a bare `panic` name with stable type/arity diagnostics.
- MIR adversarial fixtures cover the wrong panic operand type, missing register,
  invalid string ID, malformed UTF-8 constant data, verification-work limits,
  execution string-byte limits, invalid API input, and failure atomicity.
- The checker fail-allocator corpus now includes string decoding, pool
  ownership, and panic lowering and remains leak-free through injected failure
  points until success.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on decoder, formatter, checker, MIR, and affected C-unit sources | 0 | zero findings |
| `./tools/check-generated` | 0 | diagnostic, Unicode, and syntax artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D036 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2/I4 work

This iteration provides an internal, owned string representation only for the
unoptimized MIR oracle. It is not the production generational GC, bytecode
object format, native ABI, string standard library, interpolation lowering, or
serialized-MIR representation. String patterns and top-level string constants
remain outside the current checker slice.

Declared `never`-returning calls, proven divergence, and semantic diagnostics
inside unreachable statements remain open. Explicit panic also still requires
the I3 bytecode-VM and I4 native differential evidence before LANG-026 or I2 can
be considered fully verified; neither production engine exists in this gate.
The roadmap therefore retains that work as a removal condition rather than
claiming engine equivalence from the oracle alone.

Under D034, stable panic facts are language/runtime semantics, while human or
JSON rendering, trust policy, authorship, CI admission, sandbox policy, and
release decisions remain responsibilities of tooling and process.
