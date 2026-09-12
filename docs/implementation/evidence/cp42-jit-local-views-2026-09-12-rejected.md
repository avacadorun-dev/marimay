# Local JIT views and adjacent booleans: rejected early

Status: **rejected at the predeclared early gate**, against
`da34a10749b5d541ba3d7629c03b9ff3409019e3`. The VM title pair improves by
**2.1031%**, below the required **3%**.
No favorable rerun or threshold adjustment follows. Production and normative
contracts are restored; [accepted string publication V2](cp42-string-publication-v2-2026-09-12.md)
remains current. Original all32 Native/VM parity and complete Tier1 remain open.
JIT remains opt-in.

[Protocol, checks and results](cp42-jit-local-views-2026-09-12-rejected-summary.json),
[raw sources, scripts, commands and observations](cp42-jit-local-views-2026-09-12-rejected-raw.json.gz),
[rejected candidate patch](cp42-jit-local-views-2026-09-12-rejected-candidate.patch).
Private candidate/replay indexes verify that applying the zero-context patch
with `git apply --unidiff-zero` on the parent reproduces tree `4ff32f552c520fe43cd1b5a75ff8ea50995190a2`.
All candidate, accepted-control and 114 source/harness hashes were checked.

## Implemented candidate

The [mapped accepted profile](cp42-title-jit-map-2026-09-12.md) identifies repeated
array/string binding predicates and boolean store/reload pairs in the original
title loop. This VM-only candidate adds a focused emission-time fact owner.
Caller-provided bounded storage remembers successfully bound canonical registers;
it contains no runtime pointer and creates no allocation or binding-layout change.
The first binding remains fully checked. A successful checked string projection
can establish its destination fact after its existing success guard and complete
reference/byte-binding publication. Writes discard their own facts; independent
entries, cold boundaries, native transitions and helper/mutation boundaries reset
facts conservatively. Ordinary checked misses, child validation and exact exits
remain authoritative.

Seven explicitly recognized boolean producers may pass their normalized R0
result into an immediately adjacent branch on that same VM register when the
branch is not an independent entry. The producer's VM-register store remains;
only the following reload is removed. A different destination, nonproducer or
independent entry keeps the original reload. No general register allocation,
deferred materialization, runtime representation or source semantics is added.

## Checks

Focused Release passes **19/19 ON** and **5/5 OFF**. Original stdout for all
**32 workloads** passes in Native and both VM configurations. Every Native
workload executable is **byte-identical to the accepted control**; no Native
speedup or Native paired-timing claim is made by this VM-only experiment.
Six architecture/memory/documentation gates and `git diff --check` pass.

The new view owner enumerates 117,649 mixed six-step binding/copy/overwrite/reset
sequences against an abstract reference-value model. Direct native tests prove
that the second same-view emission is smaller than the first while all original
array/string values remain materialized at the cold exit. Differential fixtures
cover aliases, canonical overwrite, branch joins, embedded-zero strings,
malformed references and cold collection, with every finite/shared fuel boundary.
Boolean fixtures cover all seven producers, true/false results, intervening
scratch clobbers, another destination, independent entry, empty/NUL strings and
the materialized VM result. Deterministic allocation-index sweeps reach zero
live allocations and live bytes; direct-native teardown also reaches zero code
mappings and bytes. Existing focused call/root/epoch/ownership tests pass.

Two initial patch applications failed atomically on unmatched context before
any source mutation. The corrected anchored edit then builds and passes its
first test run. No failed build, changed test expectation or resampled early
series is hidden. Full ON/OFF CTest, ASan/UBSan, Rosetta, full32 paired timing
and a fresh five-way comparison are **not run after early rejection**.
Complete Tier1/Linux LSan are not implied by focused macOS success.

## Fixed early series

Each original title size uses 3 warmups and 11 rotating/alternating fresh-process
pairs, retaining source-run preparation and the 64MiB heap limit. Quiet host
snapshots precede timing; 5-second monitoring reports no known foreign compute
or gap over 15 seconds. Builds/tests/profiling do not overlap measurement. These
remain local Apple M2 observations, not independently isolated host evidence.

| Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 55.978583 | 55.304958 | 0.987966380 | 0.993570904 |
| title-search-100k | 143.933000 | 139.622834 | 0.970054359 | 0.962581475 |

The pair's geometric latency ratio is **0.978969404**.
Both row no-regression guards pass, but they do not replace the 3% minimum.
The separate 10% aspiration also remains unmet.

## Changed next action

This candidate provides a measured but insufficient reduction. The next
material change should investigate destination-view invalidation after an exact
array/record alias copy. The mapped loop currently copies both reference words,
then reloads them to compare the destination's old binding identity, even when
subsequent local accesses use a different canonical register's view.

A possible policy is to clear the destination's binding epoch directly when
the existing alias owner establishes a different canonical register. Keep the
identity-sensitive policy for strings and copies that remain canonical to
themselves. Preserve all complete reference stores, register-fact invalidation,
canonical-overwrite detachment and checked binding on future independent entry.
This is an internal cache-policy hypothesis, not an accepted speedup; cache
behavior elsewhere requires the full32 gate. Avoid duplicating the alias owner's
type/layout rules in the emitter. A new fixed protocol must precede that changed
candidate; this failed series remains rejected.

The current [accepted all32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
continues to supply Go/Rust numbers. Stage began 2026-09-12T10:12:54.503653+00:00, due
2026-09-12T13:12:54.503653+00:00; evidence prepared 2026-09-12T10:34:12.764150+00:00, deadline met:
**True**. Actual commit/push time is recorded separately.
