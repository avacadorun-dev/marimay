# Checkpoint 31: program arguments and controlled exit

Date: 2026-08-24.

Status: verified local checkpoint evidence.

Verified subject: source base `49dd7c5cfdeabbccf212e1193d04477bcdc1d768`
plus the single CP31 change described here. No tag, publication, external
identity operation, or broader CLI0/product-gate claim is authorized by this
report.

Host: macOS arm64 (`Darwin 25.3.0`), CMake 4.4.2, Apple Clang 17.0.0.

## Authority and delivered contract

Accepted [D066](../../design/decisions.md#d066-program-arguments-and-exit-status-are-bounded-root-inputs)
and the corresponding normative changes in
[language](../../spec/language.md), [execution](../../spec/execution.md),
[toolchain](../../spec/toolchain.md), and
[diagnostics](../../spec/diagnostics.md) define this boundary:

- root `main` accepts zero parameters or one exact `[]string` and returns
  `void`, `int`, `Result<void, E>`, or `Result<int, E>`;
- the executable name is excluded; empty strings, spaces, Unicode, leading
  dashes, and literal `--` values are preserved after the first tool separator;
- values are counted UTF-8 without interior NUL, with limits of 4096 values,
  262144 bytes per value, and 1048576 bytes total;
- POSIX imports host bytes as strict UTF-8, while Windows uses a wide CRT entry
  and strict UTF-16-to-UTF-8 conversion after CRT quoting interpretation;
- statuses 0 through 255 are returned verbatim only after root cleanup;
  invalid status, `Result.Err`, panic, and allocation failure retain exact
  `G4007`, `G4003`, `G4000`, and `I0002` classifications;
- `@test fn(arguments: []string): void` receives the same values, while the
  existing zero-parameter test form remains valid.

The new traceability rows are LANG-042, EXEC-023, and TOOL-024. Generated
diagnostic registries add G4006, G4007, and the separate tool-host U0007
boundary (8192 values, 262144 bytes per value, and 8388608 bytes total).

## Implementation and ownership

The change keeps argument semantics in focused owners:

- `runtime/common/program_arguments.c` validates counted views and constructs
  failure-atomic managed `[]string` values;
- `runtime/platform/posix/program_arguments.c` and
  `runtime/platform/windows/program_arguments.c` own the two host conversions;
- `bootstrap/src/bytecode/vm_arguments.c` binds the checked array descriptor
  and constructs the VM entry value;
- `bootstrap/src/native/c_emitter_entry.c` emits the host entry, common
  cleanup path, Result/status mapping, and the platform-specific import call;
- `bootstrap/src/cli/cli_program_arguments.c` owns run/test separator-value
  validation and exact diagnostics.

The checker entry owner accepts only D066 signatures. VM and native adapters
consume that checked metadata; neither backend repairs source semantics.
`mmay run` and direct native artifacts publish the same selected status only
after managed execution state is torn down.

## Test inventory and exact observations

Focused runtime tests cover zero values, empty and Unicode strings, strict
encoding, interior NUL, exact and exceeded count/per-value/total limits, POSIX
or Windows host conversion, invalid host encoding, managed construction, every
reachable deterministic allocation failure, and zero live allocations/bytes
after teardown.

The CP31 CLI/differential owner covers:

- exact VM values `alpha`, `two words`, empty, `Мир`, literal `--`, and
  `-dash`, with status 23 and one final `cleanup` line;
- zero arguments/status 0, the status-255 boundary, and status 70 with empty
  stderr to prove that a program-selected tool-overlapping number is preserved;
- direct and Result statuses, `Result.Err`, invalid -1/256 statuses, panic,
  human/JSON diagnostics, and cleanup before every published outcome;
- G4006 for malformed UTF-8 and exceeded count/per-value limits before any
  program stdout;
- identical argument injection into VM/native test isolates, including the
  unchanged zero-parameter test form;
- Release native artifacts with byte-for-byte stdout/stderr and exact status
  comparison for normal, Result, invalid-status, and panic paths;
- a CLI-wide deterministic allocator sweep ending with
  `live_allocations == 0` and `live_bytes == 0` at every failure index.

The public package `tests/public-alpha/cp31-args` passes formatter check,
authoritative check, VM run, `test --engine both`, Release native build, and
direct native execution. Both run paths receive only:

```text
alpha
two words
```

The deeper focused corpus pins empty/Unicode/separator values and nonzero
statuses so the public zero-status smoke cannot hide transport or status drift.

## Validation scope and limitations

The final local validation passed on the same tree:

| Command | Result |
|---|---|
| `ctest --preset debug --output-on-failure` | 5/5, 248.71 s |
| `ctest --preset release --output-on-failure` | 5/5, 250.38 s |
| `ctest --preset asan-ubsan --output-on-failure` | 5/5, 450.20 s |
| public `fmt --check`, `check`, VM `run`, `test --engine both`, Release native build, and direct artifact run | passed |
| argument-injected checkpoint test under `--engine both` | 2 passed, 0 failed |
| roadmap, architecture, memory-policy, generated-file, doc-link, spec-term, example, syntax-coverage, release-contract, and diff checks | passed |

Final artifact SHA-256 values:

- Debug compiler: `31738cb95985a75408e0a33278d20c338906f3bc5c5ba0a90a90f7c3b0719a94`;
- Release compiler: `2173f84014be4bcf57fb94cd9c0b3d5e0e6c9ac2d5861e7b87c45b19c69ab36a`;
- public CP31 Release artifact: `b2ea8a96977422abfe8c413a2eaa36bd8143ef51cedec59c2990c967a0e05ffc`.

The Windows wide-CRT owner is compiled conditionally and covered by the same
source-level platform fixture, but this macOS report is not a Windows execution
claim. Apple sanitizer success does not substitute for Linux LeakSanitizer or
the complete Tier 1 parent-gate matrix. CP31 therefore verifies only its local
checkpoint scope. CLI0 and all parent product gates retain their prior states;
CP32 becomes the sole ready checkpoint.
