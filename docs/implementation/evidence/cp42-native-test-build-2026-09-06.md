# CP42: compile one native executable for a test invocation

Scope: local macOS arm64 / Apple M2 / AppleClang 21 follow-up to
`cee30f5cb5fdc9717ad63dd29066b27863ba3fd7`. This does not close CP42,
the config application's performance/resource acceptance, or a Tier 1 gate.

## Cause and repair

The [previous cold profile](cp42-test-preparation-2026-09-06.md) found seven
C files of 4,045,463 bytes each for the unchanged seven-test config module.
58.6% of sampled main-thread time waited for the C compiler and 29.7% waited
for native test processes. The full MIR function set was emitted and compiled
separately for each entry. This was redundant compilation, not test execution.

The CLI now lazily prepares one canonical symbol table and one native build
per invocation. Each test still starts a new process. The generated host
validates an internal decimal table index and removes it before importing
user arguments. The ordinary host owner still constructs arguments, enforces
limits and capabilities, maps status/panic, and disposes execution state.
The table accepts only unique symbol-sorted void functions taking zero or
one `[]string` parameter; its first symbol is the program's canonical entry.

This implements existing [D051](../../design/decisions.md)
and D066, and the [toolchain contract](../../spec/toolchain.md) allowing shared
compiled code but never mutable execution state. There is no new source syntax,
public CLI option, bytecode format, test-result cache or unverified entry path.
The emitted source participates in the existing native cache key, so the
different entry table cannot reuse a stale single-entry artifact.

`cli_test_native.c` owns the symbol/argument vectors;
`c_emitter_test_entry.c` owns table validation and selection emission behind
a narrow header. The existing coordinators only sequence those owners.
No global cache, compiler copy or worktree was introduced.

## Correctness

`test_cli_test_native.c` covers sorting, non-test exclusion, duplicate/missing
symbols, empty suites, replacement cleanup, pointer identity of user arguments,
the 4096-argument transport boundary, and every deterministic allocation-failure
index of both new allocation-capable helpers. Its CLI fixture compares exact
VM/native reports and fresh arrays across mutation and zero-argument tests.
An output-writer failure after execution checks disposal of the retained build.

`test_native_c_emitter_test_entry.c` covers malformed tables/options/signatures,
missing/empty/noncanonical/overflowing/out-of-range selectors, 4096 arguments,
the exact 1 MiB byte boundary, one byte/count over, and invalid UTF-8. It sweeps
all reachable allocation-failure indices during emission with the borrowed MIR
and output using the same fault allocator. A generated-host harness compiles
once at each of `-O0` and `-O2`, then sweeps host failures for both entry arities.
Every allocator-backed teardown checks both zero live allocations and bytes.
The generated-host harness is POSIX-only; Windows still runs emitter/table and
the cross-platform CLI checks. Local macOS is not Windows execution evidence.

The existing `checkpoint11-prepared` fixture additionally compares exact output,
captured panic/defer behavior and subsequent pristine arguments in VM, native
and both modes. The new boundary fixture also passed on the unchanged `cee30f5`
compiler. Its first authoring attempt used `expect` inside an ordinary helper;
the checker correctly rejected that. The helper now returns a boolean and only
`@test` bodies assert it. No language rule was weakened to pass the fixture.

Release passed the native owner (249.96 s) and all other 67 CTests (181.51 s),
covering all 68 tests, including the staged source distribution. Debug passed
the native owner (326.12 s) and the final five-test CLI/heap/CP31/JSON/local-run
selection (46.29 s). These overlapping correctness runs are not latency evidence.
The unchanged config app passed 32 exact process scenarios across VM/native
debug/native release plus atomic-write, denied-I/O and broken-pipe checks
(115.77 s under concurrent tests).

ASan/UBSan passed the native owner (432.11 s), CP31, CLI heap, typed JSON and
local-execution tests. After refreshing the corrected fixture, the final CLI
test owner passed separately (17.04 s). The initial six-test Debug/sanitizer
commands each reported the same fixture-authoring failure described above;
those failures are not represented as passing runs. This is macOS sanitizer
evidence, not Linux LeakSanitizer or the full supported-platform matrix.

The architecture, C-memory policy, generated artifacts, Markdown links, roadmap,
spec terminology, example syntax, syntax coverage and release-contract checks
passed without exceptions or relaxed rules. `mmay doctor` found the compiler
and both matching native runtime profiles in the sole language checkout.

## Paired performance evidence

After our builds/tests/gates finished, three pairs ran the unchanged five
business-source files in disposable fixture modules. In each pair both
compilers used the identical source path, starting with no `.mmay` directory
and then reusing their own cache twice. Participant order alternated. Ordinary
desktop applications remained running; no OS cache eviction, smaller workload
or changed budget was used. This is local evidence, not a dedicated D056 runner.

| Seven tests, VM + native | `cee30f5` | Shared native executable |
|---|---:|---:|
| Empty native-cache median of 3 | 10.504552 s | 1.909638 s |
| Empty native-cache range | 10.381283–11.690929 s | 1.896439–2.013136 s |
| Repeat median of 6 | 1.253963 s | 0.585065 s |
| Repeat range | 1.205537–1.475096 s | 0.581829–0.600900 s |
| Generated test C files per module | 7 | 1 |

Cold time fell 81.8% (5.5× faster); warm time fell 53.3%. All three cold
candidate samples meet the predeclared 10 s budget. Every invocation produced
the same seven-pass report, exit 0 and empty stderr. Unlike the previous repair,
generated **test** C intentionally changes: one selectable entry scaffold
replaces seven single-entry programs. Harness schema 2 explicitly requires
seven baseline programs and one candidate program; it does not silently drop
the old source-identity assertion. Ordinary `build --emit c` remained identical
byte-for-byte within each of the three pairs.

SHA-256:

```text
0a108ed25f4287e24200c9b2e4ae9fb1eccbba3e423215f0078ba6fa8f673645  baseline cee30f5 Release
b4b5a6c95da1956ef5e696aaacd8d2b9c47a58710669b613efaae16c8cf9e3c2  shared-test Release
053ceeb16f369515ec32a64aa94255b9424d24f2aac4b9ae8293bd3340e410f3  build/paired-native-test-build-20260906.json
```

The raw report and harness are in the external config repository. Business
source hashes remain those in the [prior performance report](cp42-config-performance-2026-09-06.md).
The baseline is one saved binary in ignored `build/dispatch-baseline`, not a
second checkout. Reproduce from the external config directory with new output:

```sh
python3 tools/compare-test.py \
  --baseline ../../marimay/build/dispatch-baseline/mmay-cee30f5 \
  --candidate ../../marimay/build/release/mmay \
  --candidate-native-units 1 --output build/paired-native-test-build-new.json
```

This closes the measured cold test-loop regression, not all application
acceptance. Small repeated VM latency, large-input headroom, first launch of
new native artifacts, resource/OS-permission/symlink coverage and Tier 1 remain
separate obligations. Do not advance to the manifest utility on this result.

A subsequent sequential `tools/measure.py` run retained all workloads and
budgets. Small VM first launch was 0.314758 s; repeat median 0.289899 s, with
two of seven repeats above 0.3 s (0.321958 and 0.311432 s). Large VM input took
5.713600 s with version first and 9.743502 s with version last. Those individual
passes do not establish headroom or erase the previous >10 s outlier.
The first invocation of a newly built native file still took 0.627682 s,
exceeding the 0.1 s budget; subsequent calls were 0.003647–0.010042 s.
No host service or mechanism is newly attributed by this measurement.
Large-input RSS was 139,116,544 bytes VM and 91,652,096 bytes native, under
the unchanged 256 MiB bound; this is process RSS, not managed-heap accounting.
All output/status checks passed. The raw report is
`build/measurements-native-test-build-20260906.json`, SHA-256
`f66122ae72f6f6313a6cf29969484476db0c3f31b28d7a936d234775cad62fa9`.
