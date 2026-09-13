# Checked string-search loop composition: rejected in the full gate

Status: **rejected**. The fixed early VM title pair improved
9.33%, but the full unchanged all32 series confirmed
an OFF record-create P90 regression: 8.30% initially and
12.19% in the single prescribed confirmation. Both exceed the
predeclared 5% allowance. There is no favorable timing retry or retrospective
acceptance. Production is restored to the parent implementation.

[Protocol, results and hashes](cp42-jit-string-search-loop-2026-09-13-rejected-summary.json),
[raw evidence](cp42-jit-string-search-loop-2026-09-13-rejected-raw.json.gz), [exact candidate](cp42-jit-string-search-loop-2026-09-13-rejected-candidate.patch),
[fresh accepted-production comparison](cp42-jit-string-search-loop-2026-09-13-rejected-accepted-comprehensive.md).
The published patch uses zero context; `git apply --unidiff-zero` reproduces
the archived candidate tree. Original context-patch bytes remain in the raw
archive. This formatting repair changes neither candidate code nor timings.
The fresh five-way table uses unchanged production control
`613b70329a18e024a1a0527ef0ae6ee480b65dc8`, not the rejected candidate.
All original32 inputs, arguments, expected stdout, 114 corpus/harness hashes,
the 64MiB heap and opt-in JIT policy are retained. Native executables are
byte-identical to control and the previous accepted comparison.

## Candidate and correctness

A separate checked-search owner recognizes typed three-block CFG/dataflow,
independent of source identity, symbols, block order or register numbering.
It retains parent/target views only through complete nonallocating iterations,
validates every selected child with the original runtime validator, and commits
the exact original register/PC/fuel prefix. A miss costs nine instructions, a
hit six, an exhausted head two. A failed iteration remains uncommitted; short
fuel uses ordinary dispatch. No whole-array certificate, allocation or GC is
introduced inside the borrowed region. The candidate D109/spec/architecture
amendments are in the archived patch and are reverted with the implementation.

Full **105/105 ON** and **89/89 OFF** tests pass. Focused macOS ASan/UBSan
passes **22/22**, plus all32 outputs; macOS leak detection is disabled and is
not Linux LSan evidence. A fresh x86-64 build passes **22/22**, including
native-generation tests, and all32 VM outputs under Rosetta. The same AppleClang
executes on the ARM host with an explicit x86-64 target; produced executables
and an emitted smoke executable are inspected as x86-64. Only after that full
replacement passed was the older slow supplementary Rosetta run interrupted.
Its exact signal, partial results and raw exit status remain archived; no old
test result is needed for the final x86 claim.

Focused tests use independent original-instruction dispatch and generated
machine entry, comparing registers, PC, finite/shared fuel, panic spans and
late failures. They cover malformed/permuted CFGs, embedded NUL/Unicode,
empty values, stale references, GC/growth/replacement and every reachable
allocation-failure index, with both allocator teardown ledgers at zero.
Architecture, C memory policy, roadmap, links, spec terms, generated artifacts
and diff gates pass locally. Complete Tier1 and Linux LSan remain open.

## Fixed measurements

Each series uses three warmups and eleven fresh-process samples, rotating rows
and alternating control/candidate. Quiet preflight and five-second host
monitoring found no disallowed foreign compute or observation gaps. Builds,
tests and profiles did not overlap acceptance timing. The early minimum is
3% title-pair improvement; the separate 10% aspiration was missed. Full ON sum
must not regress; OFF sum permits 1%. Every row median/P90 beyond 5% receives
exactly one confirmation, and recurrence of the same metric rejects.

| Full ON workload | Control VM ms | Candidate VM ms | Time change |
| --- | ---: | ---: | ---: |
| title-search-10k | 60.394 | 56.471 | -6.49% |
| title-search-100k | 166.966 | 142.735 | -14.51% |
| normalize-title | 134.329 | 133.440 | -0.66% |
| word-frequency | 255.131 | 261.856 | +2.64% |

on: sum 6069.629 → 6068.275ms, ratio 0.999777; initial flags `{"title-search-10k": ["p90_ratios"], "map-string": ["p90_ratios"], "record-reference-copy": ["p90_ratios"], "catalog-pipeline": ["p90_ratios"], "bfs": ["p90_ratios"]}`; confirmed regressions `{}`.

off: sum 22374.239 → 21943.143ms, ratio 0.980733; initial flags `{"normalize-title": ["p90_ratios"], "word-frequency": ["p90_ratios"], "record-create": ["p90_ratios"], "bfs-int": ["p90_ratios"], "bfs-record": ["p90_ratios"], "unique-set": ["median_ratios", "p90_ratios"], "rune-frequency": ["p90_ratios"]}`; confirmed regressions `{"record-create": ["p90_ratios"]}`.

The OFF record-create median changes by +0.33% initially and −0.40% in
confirmation. Rejection follows the predeclared P90 rule; it is not a claim
that the typical record-create invocation slowed down or that a causal
mechanism for this tail has been established. Raw dispersion and every sample
remain available. The measured title gain does not override the failed gate.

## Recovery and next dependency

The initial stage directory and controls were absent after interruption;
candidate sources and a completed x86 build remained. The disappearance is
unexplained. Exact controls and manifest-verified SLJIT inputs were rebuilt;
conversation-only timings were excluded. CTest 4.4's successful-summary syntax
required a runner parser repair after 105 successful ON tests. Identical
completed commands were retained only after checking their log hashes and frozen
inputs. The x86 target-inventory filter also required a runner-only correction
before tests. Original scripts, setup failures and repairs are preserved.
No production code was repaired during recovery. A separate archive outside
build/ preserves raw evidence and all frozen timing executables.

Private original-control instrumentation finds 358 verifier calls for 67 catalog
functions, with about 13–14ms process CPU in five observations per title/text
workload. All64 census runs retain stdout and exact call sequences; 32 allocation
ledgers are zero. These observations overlap functional tests and do not claim
isolated wall-time savings. No public verification or fresh D049 bytecode
integrity boundary is removed.

A separate observer finds repeated short live string content in 96.8% of
normalize-title and 90.0% of word-frequency VM constructions, and 96.6%/87.5%
for Native. All64 VM and 12 Native diagnostic runs retain stdout; 36 allocator
ledgers are zero. These are opportunities, not speedups. The next title-family
dependency is shared runtime string construction: investigate bounded weak
storage reuse while preserving fresh references, complete logical charges,
budget precedence and exact collector ownership. An unapplied draft in the raw
archive is not tested or accepted production. Do not continue isolated search
bridge variants or silently close the active title family. All32 Native/VM
Go/Rust parity and complete platform evidence remain open.

Recovery prepared after 95.0 minutes against four hours; original-stage deadline compliance remains unproven.
