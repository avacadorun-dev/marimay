# Checkpoint 30: dynamic strings and primitive formatting

Date: 2026-08-23–24.

Status: verified local checkpoint evidence.

Verified subject: source base `56fb0870b976c7ec1e7b2f43bb8bf1985e2ad4f3`
plus the single CP30 change described here. The final commit and push were
separately authorized; no tag, publication, or external identity operation is
authorized by this report.

Host: macOS arm64 (`Darwin 25.3.0`), CMake 4.4.2, Apple Clang 17.0.0. Fuzz
validation used a build-only local Clang resource directory containing the
compiler-rt libFuzzer runtime with ASan and UBSan; no repository toolchain
contract changed.

## Authority and delivered contract

Accepted [D065](../../design/decisions.md#d065-primitive-text-conversion-is-closed-and-locale-independent)
and the corresponding normative changes in
[language](../../spec/language.md), [standard library](../../spec/standard-library.md),
[execution](../../spec/execution.md), and [toolchain](../../spec/toolchain.md)
define a closed, locale-independent surface:

- interpolation accepts `string`, `bool`, all integer widths, and `f64`,
  evaluates expressions once from left to right, and constructs exact counted
  bytes including Unicode and embedded NUL;
- `std/convert` exposes explicit boolean/all-width integer formatting plus
  `parseInt(string)` and `parseIntRadix(string, int)` returning the nominal
  `Result<int, ParseError>` family;
- integer text is host-independent, covers radix 2–36 and exact signed range
  handling, and does not delegate semantics to `printf`, `strto*`, or locale;
- user-defined formatting, locale/collation, streams, and text/byte builders
  remain outside CP30.

## Implementation path

The change adds focused owners rather than extending stable coordinators:

- `bootstrap/src/base/integer_text.c` owns integer formatting/parsing;
- `bootstrap/src/check/checker_expr_string.c` owns interpolation and the closed
  `std/convert` lowering surface;
- MIR execution/verification, bytecode lowering/verification/VM, and native C17
  emission each have separate format/parse owners;
- `runtime/common/managed_string.c` owns checked, budgeted concatenation;
- formatter state preserves interpolation expressions without introducing
  physical line breaks inside their delimiters;
- opcodes 101–104 are generated as `FORMAT_BOOL`, `FORMAT_INTEGER`,
  `STRING_CONCAT`, and `PARSE_INTEGER`.

All new handwritten C owners are below their architecture ceilings. Allocation-
capable APIs cover invalid arguments, checked-size/budget failures, every
reachable deterministic allocator failure, and zero live allocations/bytes
after teardown.

## Test inventory and observations

Focused C owners cover integer extrema/radices/padding and malformed input;
interpolation decoding; managed concat; checker imports, aliases, namespace and
shadowing; exact generic result/error layouts; MIR and bytecode hostile types,
symbols, layouts, truncation and resource limits; VM execution; native emission;
and allocator sweeps. The formatter fixture covers calls, record construction,
arrays, nested interpolation, escaped braces, Unicode, NUL, and idempotence.

The public package `tests/public-alpha/cp30-formatting` passed `fmt --check`,
`check`, VM `run`, `test --engine both`, Release native build, direct native
execution, and byte-for-byte VM/native comparison. Its observable output is
shown with the embedded NUL escaped as `\0`:

```text
true|false|-128|-32768|-2147483648|-9223372036854775808|255|65535|4294967295|18446744073709551615
true|-128|-32768|-2147483648|-9223372036854775808|255|65535|4294967295|18446744073709551615
0.0|-0.0|inf|-inf|nan
1:2:3
ok:-9223372036854775808|ok:9223372036854775807|ok:42|empty|digit:2|range|radix
{unicode λ}|nul:\0|nested true
```

`od` confirmed byte `00` at the escaped position. Final artifact SHA-256:

- Debug compiler: `793b50d48e6261ab7a39111c9290b1d2e067569c60e7ea2d14fad060ee0c49e1`;
- Release compiler: `be32e8294592cb093bbd2a4b2b64af82cd7401fce58fd02d4766756cabf64991`;
- Release CP30 executable: `fe8ddf2539658e718fbccecf66bc8b0b6d4b932ef1cba2dd216116bdab9e122b`.

The complete suites passed after the CP30 product implementation:

| Command | Result |
|---|---|
| `ctest --preset debug --output-on-failure` | 5/5, 330.50 s |
| `ctest --preset release --output-on-failure` | 5/5, 312.57 s |
| `ctest --preset asan-ubsan --output-on-failure` | 5/5, 506.67 s |

This Apple sanitizer result does not substitute for the separately required
Linux LeakSanitizer/Tier 1 parent-gate matrix. The later `test_output_end`
fuzz-oracle repair changed no production or VM source; its rebuilt focused
regression, fixed-input replay, and short sanitizer smoke are recorded below.

## Performance regression and new feature probe

The sibling benchmark corpus was measured at base
`0e998ccd27f7009f0efde490d95bbcdcb57be46b` with the exact Release compiler
above, 3 warmups, 11 retained samples, and the existing 64 MiB heap policy.
The unchanged canonical 9-probe and 8-algorithm matrix passed the reviewed
`macos-arm64-apple-m2` budgets:

- probe report `cp30-existing-probes-local.json`, SHA-256
  `e99621c3cb7043dba6d990d5c7bb955fe1b1a74037c4b1f1498fe35bc082cccc`;
- algorithm report `cp30-existing-algorithms-local.json`, SHA-256
  `6557c05356c5781c8a43e011a9183cac3eca86cfde84c7d7c1e273bfebb0abba`;
- `validate_results.py` reported all 9 probes and all 8 algorithms within
  reviewed budgets, so D056 did not require a retry or budget change.

As exploratory context only, against the saved CP28 local medians 16 of 17
workloads improved (probe reductions 4.1%–94.0%; algorithm reductions
74.8%–95.2%). `checked-loop` moved from 68.357 ms to 70.829 ms (+3.6%), below
the D056 10% minimum regression floor and far below its reviewed 341.839 ms
ceiling. The authoritative conclusion remains the compatible budget-validator
pass; cross-revision ratios are not a replacement baseline.

A separate local-only `dynamic-text` feature probe was added without changing
the historical array manifest or its reviewed hashes. It performs 50,000
deterministic interpolation/format/concat/parse iterations. Its final 11-sample
median was 38.991 ms, MAD 0.169 ms, IQR 0.430 ms, and noise fraction 2%; raw
report SHA-256 is
`bf35ccc07ca02dfac9d2667413ec9fde9122d9be6fd850adda9a9c4bc00d062d`.
This first candidate is not a reviewed regression budget. The sibling full
`verify.sh` passed all language/corpus checks and 64 Python protocol tests;
`make benchmark-validate` also preserved both reviewed checkpoint-21 entries.

## Sustained fuzz closure and `test_output_end` regression

After supplying the build-only libFuzzer runtime, the frontend and formatter
campaigns completed their required 600 seconds with no fuzzer or sanitizer
finding. The bytecode campaign intentionally exercised the hostile verifier/VM
boundary and, at input 45,781,026, produced the reproducible artifact
`bytecode-cp30-crash-2d93021a1cb0c0bce565875263e8b64dbe701b3f`
(47 bytes; SHA-256
`b1ed45ee0b40c469f635836c9032bfbf25a08d334a29bd68c4d02138cb1252e4`).
The failing path was the call-free verified-function assertion in
`tests/fuzz/bytecode_fuzz.c`; the VM itself retained its documented result.

The verifier proves the `test_output_end` operand types, while the VM must
still reject a false or stale `test_output_begin` marker with
`MMAY_BYTECODE_RUN_INVALID_BYTECODE`. That value-dependent runtime precondition
cannot be established by structural bytecode verification. The repair therefore
changes only the fuzz oracle: the call-free invariant admits this status only
for functions containing `TEST_OUTPUT_END`. The production verifier and VM
semantics are unchanged. `test_cp11_test_pipeline.c` now pins the verified
`void <- bool` stale-marker case and its exact runtime status.

The campaign and closure commands were:

```sh
cmake --build --preset fuzz --parallel
build/fuzz/tests/fuzz/mmay_fuzz_frontend build/fuzz-corpus/frontend -max_total_time=600 -artifact_prefix=build/fuzz-artifacts/frontend-cp30-
build/fuzz/tests/fuzz/mmay_fuzz_formatter build/fuzz-corpus/formatter -max_total_time=600 -artifact_prefix=build/fuzz-artifacts/formatter-cp30-
build/fuzz/tests/fuzz/mmay_fuzz_bytecode build/fuzz-corpus/bytecode -max_total_time=600 -artifact_prefix=build/fuzz-artifacts/bytecode-cp30-
build/fuzz/tests/fuzz/mmay_fuzz_bytecode build/fuzz-artifacts/bytecode-cp30-crash-2d93021a1cb0c0bce565875263e8b64dbe701b3f -runs=1
build/fuzz/tests/fuzz/mmay_fuzz_bytecode build/test-output-end-stale-marker.seed -runs=1
build/fuzz/tests/fuzz/mmay_fuzz_bytecode build/fuzz-corpus/bytecode -max_total_time=20 -timeout=10 -artifact_prefix=/tmp/mmay-cp30-bytecode-smoke-
```

Both fixed inputs replayed without abort after rebuilding. The second input is
a minimized 28-byte stale-marker function with SHA-256
`52aa1fe688402cedd417860c47e762faca6f6711c452a4a505000e7a5b37fedf`.
The 20-second ASan/UBSan smoke completed 593,297 runs without a finding. A
focused rebuilt CP11 runner, the public `fmt`/`check`/VM/`test --engine both`/
native smoke, all common documentation/architecture/memory/generated gates,
and `git diff --check` also passed. The pushed runners retain the longer
post-repair repetition; it is confirmation of the unchanged product path, not
a reason to weaken the verifier, VM, or CP30 surface.

CP30 is therefore `verified` with local evidence, CP31 becomes the sole `ready`
checkpoint, and CLI0 plus all parent product gates retain their prior states.
