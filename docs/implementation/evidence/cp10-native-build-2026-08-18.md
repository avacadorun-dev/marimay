# Native `ruda build` — preview checkpoint 10 — 2026-08-18

Checkpoint status: locally complete. I2–I5 remain `in-progress`; this evidence
does not claim native `ruda run`, built-in `ruda test`, cross compilation, or a
public 0.1.0 release.

Authority: D030, D046, D049, and accepted D050; execution §5; toolchain §6.3;
the diagnostic contract; the compiler architecture contract; and checkpoint
10 in `checkpoints.md`.

## Delivered surface

- `ruda build [target]` reuses D048 target discovery, the authoritative
  checker, D049 entry selection, and the same verified typed MIR consumed by
  the VM. Native is the checkpoint default and the only accepted build engine;
  explicit `--engine native` is equivalent.
- The native build owner emits one portable C17 translation unit, invokes the
  CMake-selected host compiler with a direct bounded argv vector, and links a
  separately owned version-matched runtime archive. It does not invoke a shell,
  read project `CC`, or access the network.
- Compiler availability is preflighted before C emission. Missing tools are
  stable `B0002` diagnostics with one platform installation action. C emission,
  host compile/link, output I/O, and unsupported-host failures are respectively
  `B0003`–`B0006`; bounded raw tool output never replaces the primary
  diagnostic.
- Debug uses strict C17 and O0; `--release` uses O2. Both preserve checked
  overflow, division, shifts, bounds, strict floating-point flags, panic
  identity, and managed cleanup.
- The default artifact is content-addressed below
  `.ruda/build/native/{debug|release}/{sha256}/app` (`app.exe` on Windows).
  The key includes generated C, compiler/runtime/language versions, compiler
  and host identities, profile, and the backend flag contract. A regular file
  at that exact key is a cache hit. The successful path is printed with one LF;
  `--quiet` prints nothing.
- `--output PATH` compiles to a sibling temporary and atomically replaces the
  exact requested executable only after successful linking. Source/check/build
  failure preserves a prior destination byte-for-byte. `--emit c` atomically
  publishes the public-entry C translation unit and performs no compiler
  invocation.
- The public native entry is separate from the internal differential harness.
  It emits no `ok:*` or `trap:*` envelope, grants D046 stdout, maps `void` and
  `Result.Ok()` to exit 0, maps `Result.Err` to counted `G4003` bytes and exit
  1, preserves `G4000`/`G4001`/`G4002`, and maps runtime integrity/resource
  failure to `I0002` and exit 70.
- `Result` and extracted error references remain in precise roots while the
  checker-selected `message(E)` executes. Root frames are balanced and every
  initialized heap is disposed on success, expected-error, panic, resource,
  layout-integrity, and host-rejection paths.

## Requirements-to-tests coverage

| Checkpoint-10 claim | Executable evidence |
|---|---|
| canonical MIR and entry ownership | the real CLI process suite builds the checkpoint-9 void, `Result`, acceptance, stdout, matrix, and panic programs through the checker-selected entry and all non-external checked MIR functions |
| native default and options | focused CLI tests cover default native, explicit `--engine native`, `--release`, exact `--output`, output paths containing spaces, `--quiet`, and `--emit c`; VM build and MIR emit are exact `U0005` negatives |
| strict C17 O0/O2 | matrix is compiled and executed in both debug and release profiles; emitted public C is independently compiled with the existing strict warning/FP harness at O0 and O2 |
| VM/native observable equality | the native suite asserts the same exact byte sequences and exits as the checkpoint-9 VM suite; a split-stream differential additionally compares VM/native stdout, stderr, and exit for matrix, binary stdout, expected error, and five panic classes |
| D046 byte preservation | the executable preserves empty output, embedded NUL, embedded newline, and the exact matrix line `415.0\n`; counted `G4003` also preserves embedded NUL on stderr |
| panic parity | explicit, bounds, overflow, division-by-zero, and invalid-shift programs retain `G4000`, `G4002`, and exact `G4001` identities/messages with exit 1 |
| public entry and runtime cleanup | emitted `void` and `Result` adapters are executed; the `Result.Err` path calls `message(E)` under precise roots; public emitted C forced to fail at allocation zero returns exact `I0002` and exit 70 |
| cache identity | two identical builds return the same path and the release profile returns a distinct path |
| atomic publication | a bad source leaves an existing requested output byte-exact; a missing output parent is stable `B0005` in human and NDJSON modes |
| toolchain preflight | an injected definitely-missing compiler returns the missing-toolchain status before dereferencing dummy MIR, proving preflight precedes code generation |
| allocation ownership | the full `ruda build --emit c` command is swept through every deterministic allocation failure to first success; directory creation and executable publication have independent fail-every-allocation tests; all iterations assert `live_allocations == 0` and `live_bytes == 0` |
| platform implementation | POSIX and Windows own UTF-8 directory creation and atomic executable replacement; [Gitea run 41](http://ci.example:3000/OS/Ruda/actions/runs/41) passed the Linux GCC/Clang debug/release and sanitizer matrix for implementation commit `2ac77cc`; the GitHub matrix additionally registers AppleClang and Windows/MSVC jobs |

## Observed VM/native outcomes

| Program | Exit | Exact observable bytes |
|---|---:|---|
| matrix, O0 and O2 | 0 | stdout `415.0\n`; stderr empty |
| checkpoint acceptance | 0 | stdout `cp9 acceptance ok\n`; stderr empty |
| binary stdout | 0 | stdout `\na\0b\na\nb\n`; stderr empty |
| root expected error | 1 | stdout empty; stderr `error[G4003]: application failed\n` |
| counted expected error | 1 | stdout empty; stderr `error[G4003]: a\0b\n` |
| explicit panic | 1 | stdout empty; stderr `panic[G4000]: explicit failure\n` |
| bounds panic | 1 | stdout empty; stderr `panic[G4002]: array index 2 is outside length 1\n` |
| integer overflow | 1 | stdout empty; stderr `panic[G4001]: integer overflow\n` |
| division by zero | 1 | stdout empty; stderr `panic[G4001]: integer division by zero\n` |
| invalid shift | 1 | stdout empty; stderr `panic[G4001]: integer shift count is outside width\n` |
| forced native allocation zero | 70 | stdout empty; stderr `error[I0002]: native runtime allocation failed\n` |

## Scope boundary and residual risk

- Checkpoint 11 still owns `ruda test`, native/both test engines, per-test
  isolation, and deterministic test reporting. The final toolchain's native
  `ruda run` execution wrapper is also not exposed by this build checkpoint.
- Portable `.rudab` build output, `--emit mir`, cross-target packs, installed
  toolchain repair, and `ruda doctor` remain their ordered toolchain work.
- This local host provides Apple Clang only: `/usr/bin/gcc` is the Apple Clang
  driver alias, not GNU GCC, and Windows/MSVC is unavailable locally. The
  pushed Gitea matrix supplied successful GNU GCC and upstream Clang evidence;
  Windows/MSVC remains registered in the GitHub matrix but was unavailable on
  the local/Gitea execution surfaces. I4 therefore remains `in-progress`.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | bounded owner sizes, dependencies, and production/test registration |
| `./tools/check-c-memory-policy` | 0 | central allocation policy and no direct production heap calls |
| `./tools/check-generated` | 0 | diagnostic registry and generated references fresh |
| `./tools/check-doc-links` | 0 | documentation links |
| `./tools/check-spec-terms` | 0 | normative terminology and D050 registry |
| `./tools/check-example-syntax` | 0 | public example syntax |
| `./tools/check-syntax-coverage` | 0 | syntax-kind and grammar coverage registry |
| `ruda fmt --check tests/run/cp9` | 0 | all reused executable fixtures canonical |
| `ruda fmt --check std/io/io.ruda` | 0 | standard I/O source canonical |
| `git diff --check` | 0 | whitespace validation |
| `cmake --build --preset debug --parallel 4` | 0 | strict debug C17 build |
| `ctest --preset debug --output-on-failure` | 0 | full CTest, 160.20 seconds |
| `cmake --build --preset release --parallel 4` | 0 | optimized strict C17 build |
| `ctest --preset release --output-on-failure` | 0 | full CTest, 166.28 seconds |
| `cmake --build --preset asan-ubsan --parallel 4` | 0 | instrumented compiler and native runtime build |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | full sanitizer CTest, 236.40 seconds |
| exact split-stream VM/native differential | 0 | eight programs matched stdout, stderr, and exit independently |
| [Gitea Actions run 41](http://ci.example:3000/OS/Ruda/actions/runs/41) | 0 | pushed `2ac77cc` passed documentation/architecture, GCC and Clang debug/release, ASan+UBSan+LSan, and TSan in 21m59s |
