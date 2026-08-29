# Checkpoint 35: environment and process context

Date: 2026-08-29.

Status: verified local checkpoint evidence.

Verified subject: source base `aeb2baf8644963a8e84cf816c352b4c286dc0f8a`
plus the single CP35 change described here. No commit, push, tag,
publication, external identity operation, parent product-gate claim, or
cross-platform claim is authorized by this report.

Host: macOS arm64 (macOS 26.3.1), CMake 4.4.2, Apple Clang 17.0.0.

## Authority and delivered contract

Accepted [D078](../../design/decisions.md#d078-named-environment-reads-and-explicit-process-context)
and the corresponding normative changes in
[standard library](../../spec/standard-library.md),
[execution](../../spec/execution.md),
[toolchain](../../spec/toolchain.md), and
[diagnostics](../../spec/diagnostics.md) define this boundary:

- `std/env.get(name)` returns a `Result<Option<string>, Error>` and performs
  one exact lookup only after validating the name and finding an explicit
  `--allow-env NAME` grant. Missing and present-empty values remain distinct;
  invalid host UTF-8 is a typed error.
- Portable names match `[A-Za-z_][A-Za-z0-9_]*`, are at most 255 bytes, and
  grants are bounded to 64. POSIX compares exact bytes; Windows folds only
  ASCII letters for capability and duplicate matching.
- Environment values are bounded to 1 MiB. Programs cannot enumerate the
  environment, denied lookups do not reach the host adapter, and diagnostics
  never include a value.
- `std/process.currentDirectory()` and `executablePath()` return owned,
  normalized absolute `std/path.Path` values through injectable process
  context. Unavailable and invalid-encoding failures are stable typed errors.
- VM and native execution receive the same explicit context. Native generated
  C contains grant names but no environment values, so ambient values do not
  affect emitted source or build-cache identity.

No subprocess, signal, dynamic-library, environment-enumeration, or hidden
environment-driven compiler/runtime policy surface is added.

## Implementation and ownership

`runtime/common/env.c` owns validation, capability matching, lookup bounds,
UTF-8 validation, and cleanup. Narrow POSIX and Windows environment adapters
perform only the host lookup. `runtime/common/process_context.c` owns injected
or host-provided cwd/executable capture and absolute path normalization.

Focused checker ownership recognizes only the exact imported `std/env` and
`std/process` functions. Typed MIR carries closed environment-read and process-
context instructions; dedicated MIR verifier/oracle, bytecode lowering,
bytecode verifier/VM, and native C17 emitter owners consume them. Generated
bytecode opcodes 138-140 cover environment lookup, current directory, and
executable identity and reject invalid operations, registers, or nominal
layouts before execution.

The CLI accepts repeatable `--allow-env NAME` only for `run`, `test`, and
`build`, with separate and `=` spellings, an exact 64-grant limit, portable
validation, duplicate rejection, help text, and stable diagnostics. Generated
native C embeds only validated grant names and uses the same runtime contract
as the VM. Architecture ownership remains below every hard ceiling; no
architecture exception was added.

## Test inventory and exact observations

Focused C tests cover:

- portable name validation, non-NUL-terminated counted names, POSIX exact and
  Windows ASCII-fold comparison, invalid/duplicate names, and denial before
  any host call;
- missing, empty, Unicode, invalid UTF-8, bounded-value, host-error, and
  defensive host-payload cleanup paths;
- injected cwd/executable paths, normalization, unavailable/invalid UTF-8
  errors, and defensive process-error cleanup;
- every reachable deterministic allocation-failure index for environment
  initialization/read and process-path capture. After every full teardown,
  both `live_allocations == 0` and `live_bytes == 0` are asserted;
- exact standard-package signatures, hostile MIR kinds/registers/layouts,
  hostile bytecode registers, opcode inventory, encode/decode, and equivalent
  injected execution through the MIR oracle and bytecode VM;
- exhaustive allocation-failure cleanup for the MIR oracle, VM, and native
  emitter, including both live-allocation counters after teardown.

Process integration tests prove VM/native byte-for-byte environment output for
missing, empty, Unicode, and denied values; no secret leakage; byte-identical
generated C for different host values under the same grant; changed generated
C for a different grant; Unicode/space cwd handling; and executable identity
after relocating the native artifact. CLI tests cover missing, empty, invalid,
duplicate, excessive, and command-inapplicable grants plus help discovery.

The public corpus passes four tests under both engines. A VM run returned exact
`héllo world`, while the separately built Release artifact returned exact
`native runtime ü` from a different runtime environment value. This directly
proves that the environment value was not baked into the native artifact.

## Validation

The final local validation passed on the same tree:

| Command | Result |
|---|---|
| CP35 public `fmt --check`, `check`, `test --engine both`, granted VM run, Release native build and artifact run | passed; 4/4 public tests and exact runtime values |
| `ctest --preset debug --output-on-failure` | 36/36 passed in 703.50 s; CP35 unit and process coverage included |
| `ctest --preset release --output-on-failure` | 36/36 passed in 481.42 s; CP35 unit and process coverage included |
| `ctest --preset asan-ubsan --output-on-failure` | 36/36 passed in 2133.19 s; CP35 unit and process coverage included |
| roadmap, architecture, C-memory-policy, generated-file, doc-link, spec-term, example, syntax-coverage, release-contract, and diff checks | passed |

Final artifact SHA-256 values:

- Debug compiler: `a77a9727e2da51366c1f81eb35734a3c685f1c07f1707b6a003b8186e695c2b1`;
- Release compiler: `7dda428bfb4d9283e8d985cfd080ca3f640b854babeee95c931418cbcaac49f2`;
- ASan/UBSan compiler: `653161886c5df7ee2c6ab0e94ddfdd33c0d0ad670939b4b5b4f33afb369ebb87`;
- public CP35 Release artifact: `a8e8838ba721d898e9e7f8010f8c5f2d5e2cadbb6170d3dfb948681f0157b69a`.

## Validation scope and remaining limitations

This is local macOS arm64 evidence only. The POSIX environment and process
adapters ran on this host; conditional Windows owners are source- and build-
covered but were not executed here. Apple sanitizer success does not substitute
for Linux LeakSanitizer or the complete claimed Tier 1 matrix. Environment
enumeration, subprocesses, signals, dynamic libraries, and environment-driven
compiler/runtime configuration remain outside CP35.

CP35 therefore verifies only its local checkpoint scope. CLI0 and all parent
product gates retain their prior states. CP36 becomes the sole ready checkpoint.
