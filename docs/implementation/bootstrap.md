# Building the Stage 0 bootstrap

Status: I0 implementation instructions.

The current slice builds the `ruda` C17 executable, central owned storage and containers, UTF-8/source snapshots, structured diagnostics, portable paths and hashing, platform-isolated file/process/directory operations, a lossless lexer, an immutable structural recovery tree with bounded top-level incremental reuse, manifest parsing, and the canonical formatter including D032 string spelling. The parser and formatter are reusable library surfaces, and the CLI exposes formatting; semantic checking and execution are not implemented yet.

## Prerequisites

- CMake 3.25 or newer;
- a conforming C17 compiler (Clang, GCC, or MSVC);
- the platform's ordinary build driver selected by CMake.

Configure, build, and test without downloading dependencies:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The equivalent maintained presets are:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug

cmake --preset release
cmake --build --preset release --parallel
ctest --preset release
```

Clang and GCC environments can run the current memory/undefined-behavior preset:

```sh
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --parallel
ctest --preset asan-ubsan
```

Dedicated AddressSanitizer and ThreadSanitizer presets are also maintained:

```sh
cmake --preset asan
cmake --build --preset asan --parallel
ctest --preset asan

cmake --preset tsan
cmake --build --preset tsan --parallel
ctest --preset tsan
```

The MSVC build uses `/std:c17`, `/W4`, and `/WX` through the same Debug/Release presets, and the `asan` preset selects MSVC AddressSanitizer. UBSan and TSan are not available in the MSVC presets.

## Frontend fuzz targets

The persistent I1 fuzz targets require a Clang distribution that includes the
libFuzzer runtime. Apple Command Line Tools Clang may omit that runtime; the
`fuzz` configure preset detects this at configure time and reports the missing
prerequisite rather than failing at the final link. A full LLVM distribution
is sufficient; Docker is not required.

```sh
CC=/path/to/full-llvm/bin/clang cmake --preset fuzz
cmake --build --preset fuzz --parallel
cmake -E copy_directory tests/fuzz/corpus/frontend build/fuzz/corpus/frontend-run
cmake -E copy_directory tests/fuzz/corpus/formatter build/fuzz/corpus/formatter-run

build/fuzz/tests/fuzz/ruda_fuzz_frontend \
  build/fuzz/corpus/frontend-run -max_total_time=600
build/fuzz/tests/fuzz/ruda_fuzz_formatter \
  build/fuzz/corpus/formatter-run -max_total_time=600
```

The frontend target accepts arbitrary bytes and checks bounded lex/parse plus
lossless reconstruction. The formatter target admits diagnostic-free programs
and checks parse/format/parse semantic-AST equality, string-value preservation,
import-order equivalence, and byte-exact idempotence. Campaigns intended as
evidence use writable copies of the committed seed directories so discovered
corpus entries and crash artifacts do not mutate repository fixtures.

## Platform adapters

OS APIs live under `runtime/platform/`; common code does not select or include an OS API directly. The current adapters provide:

- direct child-process execution from an argv vector, without a command shell, with bounded combined output;
- current-directory lookup, non-following file-kind inspection, and deterministic directory iteration inputs;
- exclusive temporary-file creation and complete writes;
- content sync followed by atomic replacement of a sibling destination;
- UTF-8 to UTF-16 conversion at the Windows boundary.

The atomic writer takes an explicit unique temporary path so library code does not need hidden process-global name state. Its operation table is injectable; C unit tests fail create, every short write, sync, close, replacement, and adapter allocation while asserting that the prior destination remains intact and temporary files are cleaned up.

## Generated registries

Bootstrap/release builds consume committed generated output and never run Python or access the network. During development, all current registries are checked together:

```sh
./tools/check-generated
```

The parser fixture matrices are checked independently against both the
generated syntax-kind registry and the machine-readable production grammar:

```sh
./tools/check-syntax-coverage
```

The matrices live in `tests/syntax/parser/coverage.tsv` and
`tests/syntax/parser/production-coverage.tsv`. An `open` row is an explicit I1
gap, while a `covered` production must name existing positive and formatter
fixtures plus concrete negative, recovery, and parser test owners. The check
fails on registry drift or invalid evidence; it does not mistake tracked open
work or complete syntax-kind coverage for a completed grammar-production gate.

Explicit regeneration commands are:

```sh
./tools/generate-bytecode
./tools/generate-diagnostics
./tools/generate-unicode
./tools/generate-syntax
```

The sources of truth are the diagnostic TSV, the pinned Unicode 17.0.0
`DerivedCoreProperties.txt`, the token TSV, the syntax-kind TSV, and the grammar
production TSV. Generated outputs include C headers/data plus the public
diagnostic, Unicode, syntax-token, syntax-node, and source-grammar references.
The check validates the Unicode input SHA-256 and fails when any committed
artifact is stale.

## Continuous integration

`.github/workflows/ci.yml` configures warnings-as-errors Debug and Release builds for GCC, Clang, AppleClang, and MSVC across the Tier 1 host shapes. Separate jobs cover ASan+UBSan on Unix, TSan on Linux, and MSVC ASan on Windows. The checkout action is pinned by full commit hash and does not retain credentials.

The workflow is configured but is not evidence until it runs from a versioned checkout. Current local results and the remaining platform gap are recorded in the linked I0 evidence entries in the roadmap.

## Current executable surface

```sh
build/debug/ruda
build/debug/ruda --help
build/debug/ruda --version
build/debug/ruda help build
build/debug/ruda version
build/debug/ruda version --json
build/debug/ruda --diagnostic json buid
```

Help names the specified top-level workflows. Commands beyond `help`, `version`, and `fmt` return the stable `U0003` bootstrap-stage diagnostic until their roadmap slice lands. `ruda fmt` accepts stdin, an explicit `.ruda`/`ruda.mod`/`ruda.work` file, an explicit directory tree, or no target. With no target it discovers the nearest module and any enclosing workspace manifest. Tree traversal is bounded and deterministic, ignores `.git` and `.ruda`, does not follow symbolic links, and does not cross into nested modules. `--check` and `--diff` are non-writing modes. Invalid input is diagnosed without write-back; each in-place update uses a flushed, permission-preserving sibling temporary followed by atomic replacement. Per-file replacement is transactional, while a multi-file invocation may have committed earlier files before a later failure. The frontend and formatter do not imply that `ruda check`, `ruda run`, or `ruda build` accepts Ruda programs yet.
