# Final local/private developer preview integration — checkpoint 12 — 2026-08-18

Checkpoint status: local implementation and validation complete through the
second pushed fuzz audit. Gitea run 46 is green for documentation, Clang/GCC,
Linux ASan+UBSan+LSan, bytecode fuzz, and TSan; final pushed confirmation of
the formatter and frontend regression fixes is pending. I2–I5 remain
`in-progress`; I6 remains `not-started`. This checkpoint does not announce,
package, sign, or publish Ruda 0.1.0.

Authority: checkpoint 12 in `checkpoints.md`; accepted D003, D006, D040–D046,
D048–D051; language §§5–10; execution §§4, 5, 10, and 13; toolchain §§2,
4–6, and 12; the compiler architecture contract; and the implementation test
strategy.

## Integrated surface

- `tests/preview/checkpoint12` is a runnable multi-package preview corpus. Its
  root program and five `@test` declarations cover checked scalar algorithms,
  generic managed arrays, `Option`, `Result`, `mapError`, postfix `?`, records,
  identity, exhaustive match, D040 construction order, D041 cleanup, and D043
  behavior through type-only, named, and namespace imports.
- `test_final_preview.c` is a process-level integration owner. It invokes the
  real `ruda` executable for `fmt --check`, `check`, VM `run`, default `test`,
  `test --engine both`, debug/release `build`, and the resulting native
  executables. It captures stdout and stderr separately and compares exit,
  signal, stdout bytes, and stderr bytes for the preview program, the f64
  matrix, and five panic classes.
- The process owner is part of every Debug, Release, sanitizer, compiler, and
  Tier 1 CTest matrix. Under sanitizer presets, generated native C inherits the
  configured sanitizer flags, so the differential runs cover both Stage 0 and
  its produced executables.
- Tool-owned `.ruda/` directories are ignored at every nesting depth. Real
  `test --engine both` and native-build cache artifacts therefore leave a clean
  checkout instead of becoming accidental source changes.
- Gitea and GitHub CI now have independent bytecode, frontend, and formatter
  fuzz jobs. Each builds with ASan+UBSan and runs its committed corpus for 600
  seconds with a ten-second per-input timeout and final statistics.

No source semantics changed. The integration uses the previously accepted
decisions and fixes only test/evidence/CI coverage and repository-state
hygiene.

## Required runnable-program coverage

| Checkpoint-12 program | Direct executable evidence |
|---|---|
| scalar arithmetic and boundary panics | `scalarAlgorithmsAndArrays`; process differentials for explicit G4000, overflow/division/shift G4001, and bounds G4002 |
| Fibonacci and factorial | iterative `fibonacci(10) == 55` and `factorial(10) == 3628800` in VM/native test replay |
| sorting/searching `Array<int>` | bubble sort plus binary search for first/middle/missing values |
| f64 matrix and D046 | `examples/matrix.ruda` is run in the VM and as Debug/Release native executables with exact `415.0\n` |
| records, mutation, identity | mutable `Counter`, alias mutation, and `identity.same` |
| generic Option/Result/`mapError`/`?` | generic identity plus successful and error propagation paths |
| exhaustive match | closed `Envelope`, `Option`, and `Result` matches |
| multi-file imports | root, store, and three evaluator packages are discovered from `ruda.mod` |
| D043 import form/dot call/interface | type-only, named-free, and namespace-free forms all retain dot calls and structural `Scored` conformance |
| D040 construction order | `Pair { second: next(...), first: next(...) }` observes `second == 1`, `first == 2` |
| D041 cleanup | LIFO state check (`21`) and exact captured `body\ncleanup\n` |

The existing source-matrix owner independently compares raw binary64 bits
`0x407a500000000000` across the MIR oracle and VM before compiling/running the
same generated C17. Numeric differential owners cover formatter input bits and
O0/O2 execution. The final process owner adds exact public D046 bytes to those
bit-level checks.

Malformed MIR and bytecode remain covered by verifier mutation matrices,
codec truncation/corruption fixtures, provenance checks, invalid control/data
flow cases, and the untrusted-byte bytecode fuzzer. These run in the same CTest
invocation as the final process owner.

## Clean-checkout acceptance

A detached checkout was created from staged implementation tree
`50c96b34100ab384428bcd5d9c5e7820d705fdde` (temporary validation commit
`66ad62a8392c70e3ab15244c65780635a45eabb6`) and configured/built from an empty
build directory with Apple Clang 17.0.0 on macOS arm64.

| Command | Exit | Observable result |
|---|---:|---|
| `ruda fmt tests/preview/checkpoint12 --check` | 0 | canonical, no output |
| `ruda check tests/preview/checkpoint12` | 0 | complete checker-backed graph, no output |
| `ruda run tests/preview/checkpoint12` | 0 | exact `checkpoint12 ok\n` |
| `ruda test tests/preview/checkpoint12` | 0 | five source-ordered VM tests pass |
| `ruda build tests/preview/checkpoint12 --output build/checkpoint12-native` | 0 | native artifact path only |
| `ruda test tests/preview/checkpoint12 --engine both` | 0 | five exact VM/native records match |
| `./build/checkpoint12-native` | 0 | exact `checkpoint12 ok\n` |
| `ruda run examples/matrix.ruda` | 0 | exact `415.0\n` |
| post-run `git status --short --branch` | 0 | detached HEAD only; no source changes |

The clean checkout also passed architecture, C memory policy, generated
freshness, documentation links, spec terms, example syntax, and syntax
coverage.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `cmake --preset debug` + build | 0 | strict C17 build |
| `ctest --preset debug --output-on-failure` | 0 | 1/1 passed, 170.17 s |
| `cmake --preset release` + build | 0 | optimized strict C17 build |
| `ctest --preset release --output-on-failure` | 0 | 1/1 passed, 175.42 s |
| `cmake --preset asan-ubsan` + build | 0 | instrumented Stage 0 and native runtime |
| strict ASan+UBSan CTest | 0 | 1/1 passed, 254.06 s; no sanitizer finding |
| `TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan --output-on-failure` | 0 | 1/1 passed, 242.32 s; no race finding |
| all repository documentation/architecture/generated gates | 0 | clean |
| `git diff --check` | 0 | clean |

The local fuzz configure attempt failed before compilation because the installed
Apple Clang distribution has no libFuzzer runtime. No fuzz-success claim is
made from this host. The sustained campaigns and Linux leak evidence are owned
by the pushed CI jobs described below.

## Pushed CI fuzz audit

[Gitea run 45](http://ci.example:3000/Marimay/marimay/actions/runs/45) passed
documentation/architecture, Clang and GCC Debug/Release, Linux
ASan+UBSan+LSan, and TSan. Its three new sustained jobs then found one bytecode
disassembler defect, one formatter round-trip defect, and one frontend parser
timeout.

The bytecode input is retained byte-for-byte in `test_bytecode_model.c`. It
proved that decoded and verified record, enum, interface, and `u64` operands
were not printable. The disassembler now covers all four operand kinds, with
focused nominal/interface/maximum-`u64` assertions.

[Gitea run 46](http://ci.example:3000/Marimay/marimay/actions/runs/46) independently
passed documentation (33 s), Clang Debug/Release (3 m 24 s / 3 m 26 s), GCC
Debug/Release (3 m 12 s / 3 m 16 s), Linux ASan+UBSan+LSan (5 m 27 s), the
full bytecode campaign (11 m 6 s), and TSan (3 m 56 s). It then supplied exact
Base64 inputs for the two remaining failures:

- formatter input `crash-f17e644abdfedc5475a707dd08609dc54963c06f`
  showed that a wrapped ordinary parenthesized expression received a trailing
  comma and failed reparse. Ordinary parenthesized and index groups no longer
  synthesize commas; binary-operator grouping retains required whitespace.
  The exact 137-byte source is a committed fuzz seed and formatter fixture.
- frontend input `timeout-640f364180fde0a6d5f53c20e456c3962f8827ab`
  reached the diagnostic limit in a dotted type name. `ruda_parser_eat` and
  `ruda_parser_eat_identifier` previously reported success after the terminal
  parser state had stopped cursor movement, so the dotted-name loop could not
  progress. Both helpers now report success only after real consumption. The
  exact 254-byte binary input, including its embedded NUL bytes, is reconstructed
  in the parser limit test; a minimized diagnostic-limit case and sustained
  corpus seed cover the same progress invariant.

The final pushed run must keep the seven already-green Linux jobs green and
complete both repaired formatter/frontend 600-second campaigns before this
checkpoint is reported complete.

## Remaining boundary

- I2, I3, I4, and I5 are deliberately not promoted to `verified`: their full
  roadmap gates include language/runtime surfaces and complete continuous Tier
  1 evidence beyond this bounded preview.
- I6 structured tasks, scheduler/cancellation semantics, deterministic task
  capabilities, and production direct I/O remain `not-started`.
- N0 still blocks public/indexed distribution. There is no public 0.1.0,
  registry identity, installer, signature, or support claim.
- Valgrind remains a scheduled Linux policy job. A green ordinary push cannot
  substitute for the next scheduled/manual nightly run; its latest result must
  be reported separately from sanitizer evidence.
