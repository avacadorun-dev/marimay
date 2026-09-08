# CP42: predicate array search and Notes integration

D099 implements exact free/dot `findIndex` returning `Option<int>`. The receiver
and predicate are evaluated once before length/generation are captured. One typed
MIR loop invokes the D098 function value in order, checking array generation
before each visit and after every normal callback return, including a match.
It reuses existing array and interface operations; no backend-specific predicate,
callback AST expansion, new wire operation or runtime allocation API is added.

Mutation through an alias gives G4005 before Some/None is published. A callback's
own panic and deferred cleanup take precedence; completed effects are not rolled
back. Reserve, no-op mutations and mutations of child objects retain D053 rules.
Existing iterators remain valid when search itself does not mutate the source.

## Verification

The [machine-readable packet](cp42-array-search-2026-09-08.json) records final
commands, local results and source/log hashes. Complete local Release passed
80/80 in 325.13 s, including source packaging; macOS ASan/UBSan passed 79/79
in 689.35 s. Architecture, C memory policy and roadmap gates pass. The new
frontend and formatter fuzz seeds were replayed; no new fuzz campaign is
claimed. Config regression passes seven source tests on both engines.
This packet does not close CP42.

- Ten public tests cover first/last/absent/duplicate results, empty input,
  exact argument order, predicate factories, frozen receiver/callee, generic
  enclosing calls, nominal overloads and ordinary-name collisions. They also
  cover nested searches/iterators, reference identity, reserve/no-op mutations,
  all dense scalar/reference layouts, runes, tuples and function elements.
  Local arrays shadow import namespaces, including inside a capturing closure.
- The process fixture checks formatting, checking, VM/native tests and native
  Debug/Release builds. Nine mutation/panic cases run on all three engines,
  including false/true/final returns, equal-value stores, clearing, pushing,
  removal in a callback defer, nested mutation and original-panic precedence.
- Focused checker tests sweep reachable allocation failures for valid and invalid
  calls and exercise call/argument/block/register/type ceilings. One test budget
  was corrected from invalid max_calls=0 to a valid exhausted budget of one.
- D098 and D099 now share one C test owner for execution/wire/allocation/GC
  machinery, with separate semantic fixtures. MIR and VM execution, lowering,
  encoding/decoding, source locations, native emission and generated native code
  check every reachable deterministic allocation-failure index. Forced GC covers
  callback captures and reference arrays whose capacity grows inside the callback.
  Every full teardown checks `live_allocations == 0` and `live_bytes == 0`.

A local retry of the old callback process test overlapped another invocation in
its disposable build directory and failed an atomic output write. An isolated
rerun passed. Subsequent complete suites run once per build directory. The rune
layout fixture uses the existing std/text API; source rune literals remain a
separate implementation limitation and are not added here.

## Notes consumer

Notes' existing `find` and functional `remove` now share `findIndex` and a captured
ID predicate. Removal still validates its input, makes a shallow copy, preserves
surviving reference identities, and returns the replacement database. The new
`search ТЕКСТ` CLI command prints all literal text-substring matches in database
order, with case sensitivity and no Unicode normalization or tag matching. Empty
queries are errors; other whitespace is literal. No match is a successful result.
Search never writes the database or creates a missing database file.

Sixteen source tests pass on both engines. The process adapter passes 36 scenarios
across VM/native Debug/Release and 21 closed-pipe checks, including search results,
no-match output, exact errors, ordering, unchanged files and output failures.
Existing scenarios remain in the adapter. The exact task delta is preserved in
[this patch](cp42-array-search-notes-2026-09-08.patch) against a safe source-only
snapshot taken before editing. The zero-context patch applies with
`git apply --unidiff-zero -p2`; its input hashes are recorded, and application
was checked against that exact snapshot. The external repository already
contained user changes; these are not automatically committed. No private Notes
database was read or copied. Concurrent correctness runs establish no
performance claim; the separate paired measurement below ran after them.

## Paired consumer cost

A separate sample ran after local correctness/build work completed. The two
source snapshots use the same final Release compiler and 6,098 synthetic Unicode
notes (1,047,786 bytes, SHA-256
`2c48707982e285d3253064a0c886224e5685b16aa07fcf4e8e353b84f6d2a3ad`).
For each operation/engine, one excluded warm-up precedes three measurements per
version, alternating the version order. Each run restores the input and verifies
status, exact output and full resulting JSON bytes. Native build time is excluded;
VM time includes `mmay run` checking/execution. `/usr/bin/time -l` supplies macOS
process peak RSS. The raw packet embeds all 48 runs, source hashes and the driver.

| Operation | VM median, before → after | Native Debug median | Native Release median | Native Release median peak RSS |
|---|---:|---:|---:|---:|
| Show last ID | 2730.30 → 2739.93 ms | 782.68 → 780.43 ms | 143.48 → 143.62 ms | 61.17 → 61.17 MiB |
| Remove last ID | 5551.32 → 5553.03 ms | 1396.09 → 1401.55 ms | 236.16 → 235.40 ms | 70.95 → 70.94 MiB |

Across these six comparisons, median elapsed changes range from -0.32% to +0.39%.
The end-to-end sample shows no material latency change from integrating the
callback API. It does not establish the cost of isolated callback dispatch or
identify the dominant program cost. This compares the complete consumer delta,
including the new command, and is a small warm-process sample, not cold-start,
cross-platform or CP42 performance acceptance. Named resource/performance budgets
and profiling remain open.

## Remaining acceptance

This is local macOS evidence, not Linux LeakSanitizer or completed Tier 1 evidence.
D098's escaping-mutable warning, stored-function integration in Result.mapError,
application resource/performance budgets, the rest of the portfolio and parent
alpha gates remain open. CP42 stays the sole in-progress checkpoint.

No new idea from the Vladimirov book was used, so no book page is claimed.
