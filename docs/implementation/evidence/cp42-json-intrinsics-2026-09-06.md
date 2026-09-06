# CP42 JSON intrinsic metadata and external-portfolio start

Scope: local implementation evidence for the commit carrying this report,
based on `348246768c4f917403f7a1e69732df98df5cdc07`, on macOS arm64 with
AppleClang 21 / portable C17. CP42 remains `in-progress`; neither this report
nor the first external application verifies CLI0, Tier 1, or public release.

## Compiler-service contract

D086 now has one immutable registry for the four convenience entry points,
their exact package/name identities, generic-argument policy, value arity,
generic signatures, eligible type family and direct-call-only restriction.
Import validation, call selection and metadata use that registry.

The service publishes file-import spellings, retaining both namespace aliases
and each selected named alias in bytewise order. Type-only, absent, and unrelated
package imports do not expose the calls. Prefix queries allocate nothing; their
wrappers reject stale generations and foreign snapshots. This is file-import
scope, not cursor-local completion or a production LSP. No fabricated external
function, source definition, function type or nominal dot candidate is created.
Ordinary and intrinsic facts share the completion budget. Rollback and teardown
release every owned spelling through the focused owner.

The metadata/import test owner uses signature-only source fixtures, explicitly
not a substitute for real JSON execution. It checks aliases, import/file order,
signatures, prefix filtering, invalid arguments, overlays, stale results,
cleared error results, cancellation, exact/shared limits and every deterministic
allocation-failure index. Complete teardown asserts both live allocation and
byte counters are zero. The ordinary checker JSON fixture separately covers
intrinsic typing/expansion with all-index failure injection.

## Shadowing regression

New negative cases exposed a non-callable local incorrectly classified as an
incomplete feature. The ordinary-call owner now emits the existing `T0002` at
the callee name for both scalar and nominal non-function values; genuine
function-value staging is unchanged. Namespace fallback also respects local
shadowing. D080 and the diagnostic contract clarify that distinction.

The first repair overlooked that primitive types have no dynamic descriptor.
The added scalar tests caught it; using the canonical type-kind query corrected
the repair. There is no JSON-specific diagnostic bypass. A public typed-JSON
test calls an ordinary nominal `decode` method on a local named `json`, proving
the namespace cannot override it in either engine. The public corpus now runs
14 tests; its exact test-count assertion was updated, not removed.

## Local verification

```sh
cmake --build build/debug -j 4
cmake --build build/asan-ubsan -j 4
cmake --build build/release -j 4
ctest --test-dir build/debug --output-on-failure \
  -R '^(mmay_typed_json|mmay_cp42_unit)$' -j 2
ctest --test-dir build/asan-ubsan --output-on-failure \
  -R '^(mmay_typed_json|mmay_cp42_unit)$' -j 2
ctest --test-dir build/debug --output-on-failure -R '^mmay_c_unit_checker$'
ctest --test-dir build/asan-ubsan --output-on-failure -R '^mmay_c_unit_checker$'
ctest --test-dir build/release --output-on-failure \
  -R '^(mmay_cp42_unit|mmay_c_unit_checker|mmay_typed_json)$' -j 2
ctest --test-dir build/release --output-on-failure -j 4
```

Final focused Debug CP42/typed-JSON checks passed in 264.98 seconds; the
corresponding ASan/UBSan pair passed in 289.87 seconds. Checker regression
reruns passed in 15.14 and 16.79 seconds respectively. ASan/UBSan frontend
checks also passed. Release passed all three focused tests in 78.23 seconds.
The real JSON process corpus checks formatting, source checking, all 14 tests
in both engines, native debug/release output, and exact hostile-input parity.
These overlapping runs are correctness observations, not performance baselines.

A Debug sweep excluding CP42 passed 65 of 66 tests; the only failure was the
new scalar-call diagnostic check described above. It is covered by the final
successful checker reruns. Do not describe that sweep as an all-green final
Debug run. The clean final full Release sweep passed all 67 CTests in
254.37 seconds, including the checker regressions, CP42 allocation/metadata,
the 14-test JSON corpus, and native/differential/distribution owners.

Architecture, C-memory policy, generated artifacts, spec terms, documentation
links, example syntax, syntax coverage, roadmap and release-contract gates
passed during integration. The new test owner is wired into the existing CP42
CTest, keeping 67 registered tests and the architecture ownership gate intact.
No allocation index, verifier or sanitizer check was disabled.

## Earlier pushed revision, not current-change CI

[Gitea run 189](http://ci.example:3000/Marimay/marimay/actions/runs/189)
completed successfully for base `3482467`: all 13 jobs passed, none skipped.
This includes Clang/GCC Debug/Release, ASan/UBSan/LSan, TSan, and the bytecode,
formatter, frontend and JSON fuzz campaigns. It verifies the preceding
structured-path change, not this new metadata or shadowing repair. The exact
new pushed revision still needs its own CI and missing Tier 1 evidence.

## Installed command and application scope

The ordinary `/opt/homebrew/bin/mmay` still resolves to the sole language
checkout's rebuilt Release binary. `mmay doctor` passed. In the existing notes
repository, `mmay check .`, all ten tests with `--engine both`, and the reported
`mmay run . --allow-read . -- show 1` succeeded. The user database SHA-256 was
unchanged; no notes data, notes commit or duplicate checkout belongs to this
language change.

The user's ten-application portfolio is now a mandatory
[CP42 acceptance contract](../cli-ergonomics-suite.md), indexed by ROADMAP and
the checkpoint ledger. The external navigation directory is
`/workspace/Projects/Marimay/Marimay-Examples/`. It is not a compiler copy or
monorepo. The existing notes repository is linked without duplication.

The first independent local repository, `marimay-config`, has a useful initial
validator/migrator for a documented project-scanner schema, Russian guidance,
typed JSON, Set duplicate checks, stdin/file input, JSON reports and atomic
explicit output/in-place modes. Seven source tests passed in both engines.
Its final process harness passed 27 exact status/stdout/stderr parity cases
across VM, native debug and native release, plus atomic-preservation, denied
read/write, missing-parent and broken-pipe checks (36.96 seconds). It uses only
synthetic data and cleans its generated temporary fixtures.

That external repository is still an uncommitted working application, not a
completed acceptance row. Its own findings log records void-match boilerplate,
test-helper restrictions, overloaded mapError/context, version-dispatch cost,
localization, mutation/index noise, and the explicit-flush output pattern.
The flush defect was fixed in the application and the process test rerun.
An overlapping seven-test loop took 17.97 seconds; that is not a clean latency
baseline and does not satisfy the declared UX budgets by implication.

Next: finish the config application's hostile/OS/large-data and measured
performance acceptance, then implement the manifest tool and remaining
portfolio in order while resolving CP42 collection/mutation/Option/API findings.
CP41, CP39, CP40, the external pilot, N0 and claimed-platform gates remain
independent obligations. This report does not reduce that scope.
