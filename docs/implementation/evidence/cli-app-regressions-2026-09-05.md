# CLI application regression repair — local validation

Main-line integration note: this repair is also applied independently
to the main baseline `5963290`, without the intervening CP39/CP41 commits.
The absent CP39 native-run header is not imported, and main's CP39 roadmap
availability statement is retained. The results below describe the original
repair worktree. Main-line validation passed all 12 selected tests in 621.27
seconds, including all three CLI workloads, the notes model, imported records,
void matches, formatting, checker, bytecode, cli-run and cli-test:

```sh
ctest --test-dir build/debug -R '^(mmay_c_unit_(checker|bytecode|cli-run|cli-test)|mmay_record_imports|mmay_match_void|mmay_cli_(notes|notes_model|frequency|config_migrate)|mmay_format_.*)$' --output-on-failure --parallel 4
```

The architecture, memory-policy and roadmap gates also passed on this baseline.

This is a working-tree validation record, not CP39/CP40 verification or an alpha
release claim. Work is isolated on `fix/cli-alpha-integration`, based on
`efb0f56`, in the `marimay-cli-alpha` worktree. The original `marimay` checkout
contains independent uncommitted changes and has not been overwritten.

## Repair scope

- Three compositional CLI workloads: persistent JSON notes, stdin frequency
  counts encoded as JSON, and strict typed JSON configuration migration.
- Bytecode array feature detection must include retained array layout tables;
  encoding must not produce flags rejected by the decoder.
- CLI bytecode failures must retain a useful codec reason.
- Standalone match expressions must be checked without permitting accidental
  discard of a Result (D074). Void-valued arms must join normally without a
  result register, rather than being treated as diverging (D067).
- Imported record construction must obey ordinary nominal typing and package
  visibility (D004), rather than require application-level factory workarounds.
- Record identity must accept imported records under the existing identity
  contract, including concrete generic records.

## Evidence obtained on macOS arm64

The following commands passed locally on the September 5 working tree after
the imported-construction and identity repairs:

```sh
cmake --build --preset debug --parallel 8
ctest --test-dir build/debug -R '^(mmay_c_unit_checker|mmay_record_imports)$' --output-on-failure --parallel 2
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --parallel 8
ctest --test-dir build/asan-ubsan -R '^mmay_c_unit_(checker|bytecode)$' --output-on-failure --parallel 2
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-roadmap
git diff --check
```

The checker regression sweeps successful and rejected imported constructions
through deterministic allocation failure and asserts both zero live allocation
count and zero live bytes after teardown. The VM/native fixture checks aliases,
generic fields, evaluation order, Unicode, and record identity. This is focused
ASan/UBSan evidence, not a full sanitizer suite or Linux LeakSanitizer result.

The seven notes model tests also passed `mmay test --engine both` after replacing
the exhaustion-test JSON workaround with a direct `notes.Database` constructor.
They are now registered as `mmay_cli_notes_model` so CI exercises them as well
as the process-level workload.

## Full application replay and handoff

After the compiler repairs and added filesystem rejection scenarios, all four
tests selected by the following command passed (653.63 seconds overall):

```sh
ctest --test-dir build/debug -R '^mmay_cli_(notes|notes_model|frequency|config_migrate)$' --output-on-failure --parallel 3
```

Notes replay covers add/list/show/remove, non-reused IDs, Unicode/tags, rejected
mutations and malformed databases. Additional paired VM/native executions cover
missing parent directories, directory targets, denied reads, and denied writes.
They compare process status/stdout/stderr and verify that protected databases
remain byte-for-byte unchanged. Frequency and migration fixtures compare their
VM/native outputs and rejection cases independently.

The frontend, bytecode, cli-run, cli-test and native C-unit groups passed after
the final compiler changes. The notes model also passed ASan/UBSan replay:

```sh
ctest --test-dir build/debug -R '^mmay_c_unit_(frontend|bytecode|cli-run|cli-test)$' --output-on-failure --parallel 2
ctest --test-dir build/debug -R '^mmay_c_unit_native$' --output-on-failure
ctest --test-dir build/asan-ubsan -R '^mmay_cli_notes_model$' --output-on-failure
```

The independent `marimay-notes` repo matches the integration fixture's three
application source files byte-for-byte. Its fmt/check/seven tests (both engines)
and release native build passed. A fresh temporary database exercised native
add/list/show/remove with Unicode and a tag; its final JSON was exactly
`{"version":1,"nextId":2,"notes":[]}`. The README creates the output directory
before building and documents build-time filesystem capabilities.

The changes remain isolated in the named worktree; the original checkout's
unrelated changes were not overwritten. No push, tag, publication or
product-gate closure is claimed. This local result is not the full Tier 1
matrix, Linux LeakSanitizer evidence, or CP40 release audit.

## Void-match follow-up

Returning `void` arms now create normal control-flow joins without allocating a
result register. Bool, integer, string, enum, tuple and record cases pass the
VM/native `mmay_match_void` fixture, including guards, explicit void block tails
and early return. Mixed void/value arms and reachable blocks missing a tail
remain errors. The focused checker suite sweeps successful and rejected paths
through deterministic allocation failures and validates teardown counters.

After these changes the following checks passed:

```sh
ctest --test-dir build/debug -R '^mmay_c_unit_checker$' --output-on-failure
ctest --test-dir build/debug -R '^mmay_match_void$' --output-on-failure
ctest --test-dir build/asan-ubsan -R '^(mmay_c_unit_(checker|bytecode)|mmay_match_void)$' --output-on-failure --parallel 2
ctest --test-dir build/debug -R '^mmay_format_' --output-on-failure
```

The external notes application's source files have been synchronized, its
seven model tests passed in both engines, and its README now selects the
compiler worktree containing these fixes. The old compiler checkout is not
silently presented as having the repairs.
