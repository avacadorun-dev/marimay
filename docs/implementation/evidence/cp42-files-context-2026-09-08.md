# File/context consumers and named-void pattern diagnostics

Scope: local macOS arm64, 2026-09-08; parent compiler revision `94d1e85`.
[Machine-readable evidence](cp42-files-context-2026-09-08.json) contains exact
source hashes, independent application commits/snapshots, commands, reports,
compiler/runtime identities and raw measurements. CP42 remains `in-progress`.

## Result

The existing D077/D078/D079 filesystem/context/collection surface supports
three additional independent applications. Their logic is ordinary Marimay;
Python only drives processes, creates synthetic inputs and supplies independent
expected results. Acceptance driver v1 is vendored unchanged.

| Application | Commit | Useful behavior | Local tests |
|---|---|---|---|
| marimay-manifest | `b8e7e639a1eb262f99f8b449b909a27225a65bf9` | Generate/verify deterministic JSON metadata inventory; globally sorted UTF-8 relative paths, explicit symlink kinds, atomic output, useful differences | 5 source tests both; 35 process scenarios; 3 closed pipes |
| marimay-project-check | `1e22cd605e92305ec618c1b494301a0dddcdd86e` | Reuse config v1/v2; report all extension/size/required-file/type/read findings in deterministic text/JSON | 4 source tests both; 31 process scenarios; 3 closed pipes |
| marimay-env-paths | `10b9cefd3af436c70bdecad4943aa1b0cd87a444` | Explicit environment requests, absent/empty/denied distinction, lexical path normalization and selected working directory | 3 source tests both; 22 process scenarios; 3 closed pipes |

Repositories are under `Marimay-Examples`, on local `main`, without remotes.
Config, Notes and algorithm-benchmarks histories/working changes were not
committed or modified. Project-check vendors the existing config owner without
edits. Tree package source is identical in manifest/project-check, with a
version/hash and ownership in each PROVENANCE.json. Each app has Russian README,
source tests, fixtures, independent acceptance, budgets and measurement scripts.
The portfolio has **eight implementations of ten**, not eight accepted apps.

The tree owner uses an explicit pending-directory stack, total entry/depth
limits, typed read errors and symlink-free traversal. Global path sorting uses
bottom-up merge sort; path bytes are encoded once, comparisons allocate no
scratch strings. There is no public string ordering/comparator in the current
stdlib. The app-owned implementation and cost are explicitly reviewed in the
findings rather than silently adding source operators or compiler intrinsics.

Project-check preserves the distinction between a confirmed missing file and
an unverified file under incomplete traversal. It combines every applicable
finding; the wider fixture produces exactly 1024 expected findings. Manifest
compares against an independent host filesystem oracle, accepts arbitrary input
entry ordering, rejects malformed inventories, and preserves an existing output
on a failed pre-publication operation. It inventories metadata, not cryptographic
content, and does not claim a transactional filesystem snapshot.

## Compiler correction: existing D092

`checker_match_pattern.c` now applies the existing named-void restriction while
collecting pattern bindings, before MIR register allocation. Positional,
recursive, named-field shorthand/explicit and `_ as name` forms receive T0002
at the binding. Valid `Ok(_)` remains accepted. This corrects the D101 finding
where `Ok(value)` of `Result<void,E>` could fail silently with status 70.
Normative text clarifies the same existing D092 rule; no new syntax or backend
semantics were introduced. The focused nested-pattern test owner checks seven
negative forms, exact diagnostic spans and every reachable allocation-failure
index, with both live counters zero after teardown.

## Verification

- Release CTest: **81/81**, 266.39 s. This invocation omitted `-C Release`, so
  the configuration-restricted packaging test was subsequently run explicitly:
  `ctest --test-dir build/release -C Release -R '^mmay_cp39_packaging$'` — **1/1**,
  7.67 s. All 82 required local tests therefore have passing results; this is
  not represented as one full 82/82 invocation.
- Focused checker: Release **1/1**, 3.69 s; macOS ASan/UBSan **1/1**, 12.95 s.
  The sanitizer build was rebuilt with the fix. No full sanitizer rerun or
  current Linux LSan/TSan result is claimed for this change.
- `mmay check`, `run` and `build` reject the saved named-void source with status
  1, T0002 and its source location. Existing successful void-payload engine
  tests remain covered by the Release suite.
- App source tests: **12**, both engines. Process suite: **88 scenarios**,
  **262 engine executions** (two restricted-grant scenarios use two engines),
  plus **9 closed-pipe executions**. Exact statuses/stdout/stderr are compared;
  per-app oracles separately assert outputs and relevant filesystem effects.
- Unicode names/values, empty and final inputs, invalid JSON/UTF-8, repeated
  fields/paths, malformed shapes, total/depth/output-preservation boundaries,
  missing/denied paths, POSIX symlinks and host directory permission failures
  are exercised. Environment tests install only named synthetic values.
- Final app format/check and provenance checks pass. Ordinary installed `mmay`
  resolves to the tested compiler; doctor reports compatible Debug/Release
  runtime archives. This is not clean-host installed lifecycle acceptance.
- Architecture and C-memory policy pass; roadmap/documentation/release gates
  are recorded in the machine-readable report after the evidence links land.

Allocator/sanitizer results have their stated scope and do not prove absence
of every leak. Required other-host and sustained-fuzz evidence remains open.

## Measured costs

Budgets preceded measurements. Measurement processes were sequential after
compiler verification, with ordinary desktop background and no OS cache
purge. First execution of each produced native artifact directly followed its
build. File inputs use 64 MiB managed heaps; the large tree has 10000 regular
files in 100 directories, explicitly within the 20000-entry invocation limit.

| Workload | manifest native / VM | project-check native / VM |
|---|---:|---:|
| 1000 files, median of 3 | 33.8 / 740.8 ms | 29.7 / 606.8 ms |
| 10000 files, one sample | 295.0 / 3908.3 ms | 237.9 / 2003.5 ms |
| Peak RSS at 10000 files | 49.11 / 78.39 MiB | 41.19 / 70.98 MiB |

Environment workload: 100 explicit names (64 permitted by the existing grant
ceiling, 36 correctly denied) plus 100 paths, three repetitions. Median native
6.7 ms and VM 285.0 ms; peak RSS below 4/25 MiB. Source-test both repeats are
approximately 804/787/390 ms for manifest/project-check/env-paths.

These workloads meet their predeclared throughput, test-loop and memory bounds.
**First native samples remain 567/391/385 ms against 100 ms**, so startup
acceptance still fails. Warm samples do not close it. Raw JSON samples are
authoritative over rounding; no confidence interval, five-platform performance
claim, or improvement over the old algorithm baseline is inferred.

## Remaining route and CI

The named-void diagnostic finding is resolved locally. Escaping-mutable-capture
warning and the uninhabited enum payload removal gate remain open. Delimited
and source-lint are the two still-unimplemented portfolio applications; combined
CP42 coverage/performance/platform acceptance, CP41, CP39, CP40 and N0 remain.

The network path is restored: D100/D101 were pushed and remote `main` was
verified at `94d1e85`. Bounded read-only CI inspection found run 221 for that
exact SHA queued behind run 220; no job had started at that observation. This
is not fresh CI success. New wave push and its runner outcome must be checked
against its eventual exact commit, independently of these local results.

No ideas from Vladimirov's book were used in this wave; no page attribution is
claimed. The work follows existing language/runtime contracts and reproductions.
