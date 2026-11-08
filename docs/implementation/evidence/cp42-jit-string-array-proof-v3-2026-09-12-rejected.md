# Combined certified string reads and metadata reuse: rejected early

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected at the fixed early gate**, parent `5ca3cf2d994d605f8ce8a1be0c900d2a6fc86248`.
The original VM title pair takes **0.648538% longer**,
geometric median latency ratio **1.006485379209**. This misses
the 3% improvement minimum; both individual median/P90 guards below 1.05 cannot
rescue it. The higher dispersion is retained, with no favorable rerun or changed
threshold. Candidate production, tests, normative and architecture text are
restored. Accepted [string publication V2](cp42-string-publication-v2-2026-09-12.md)
and its [all 32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
remain current. Original Native/VM parity, complete Tier1 and Linux LSan remain open.

[Protocol, results and analyses](cp42-jit-string-array-proof-v3-2026-09-12-rejected-summary.json),
[exact sources, commands, counters and emitted bytes](private-artifacts.md#artifact-023),
[replayable candidate patch](private-artifacts.md#artifact-022).
Private-index replay reproduces tree `ff7b3476a476e7842d3b8ff759ae9b184b39186a` with `git apply --unidiff-zero`.
Every archived text and binary was roundtrip verified. The original 114
source/harness hashes and frozen compilers match.

## Candidate and validation

The rejected [inline certificate V2](cp42-jit-string-array-proof-v2-2026-09-12-rejected.md)
and [isolated metadata reuse](cp42-jit-string-metadata-2026-09-12-rejected.md) patches
replay cleanly only as candidate material. Accepted binaries remain the controls.
The adaptive nonallocating whole-array proof follows twice the length in checked
reads. Full parent identity/lifetime and live mutation/length guard certified
projection of the current selected Cell. Invalid unvisited children deny reuse
without causing an early failure. The checked miss preserves its Cell argument.

Both emitted and checked C publication always write the complete child reference,
bytes/length and VM destination. Six invariant binding metadata stores may be
skipped only when a nonzero native-entry epoch matches the target binding epoch.
Zero or mismatched epochs retain complete publication. The emitted path preserves
both reference words through its metadata branches. All native entries, cold
reentry and call returns advance the epoch; wrap clears every binding. No
allocation, safepoint, source-fuel change, instruction packing or earlier
local-view/alias optimization is added. Binding stride remains 136 versus accepted
104 bytes. D111/spec allowances compose; architecture prose saying unchanged C
bridge/all stores is reconciled to checked bridge/complete values before freezing.

Preparation passes on its first attempt: **19/19 focused ON tests**, **5/5 OFF**,
all 32 Native/ON/OFF outputs and byte-identical Native executables. The new
emitter's compile command includes `SLJIT_HAVE_CONFIG_PRE=1`. Initial and final
six architecture/documentation/memory/generated gates pass. Frozen inventories
contain 106 ON and 89 OFF tests. Full CTest, 23 focused ASan/UBSan and Rosetta
checks with all 32 outputs, full paired 32 timing and a fresh five-way comparison
are not run following the fixed early rejection.

The combined metadata fixture independently reproduces accepted complete C
publication and compares real native results. It proves an existing array
certificate while exercising metadata reuse, poisoned destination epochs and new
entry misses. Existing focused owners cover guard denial, malformed children,
parent identities, mutation/growth/alias, relocation, successful/failed GC,
saturation, exact fuel/PC, recursion/entry wrap and all-index zero allocation and
byte ledgers. Focused local results do not establish complete platform acceptance.

## Fixed early timing

Three warmups and eleven rotating/alternating fresh-process pairs retain original
source preparation, arguments, outputs and 64MiB. Five-second monitoring reports
no known foreign compute or gaps; host isolation is not independently established.
P90 is the predeclared tenth ordered observation. Reported noise fractions are
8.90%/6.95% for title10k control/candidate and 7.34%/9.25% for title100k. This limits
causal precision, does not prove a regression of exactly 0.65%, and does not
supply evidence for the required improvement.

| Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 57.572583 | 57.309000 | 0.995421727 | 0.854098189 |
| title-search-100k | 146.790583 | 149.384666 | 1.017671999 | 0.991190303 |

## Direct combined counters

Private counter copies run the original workloads once per process. All 128 runs
pass: 32 workloads, two trials of plain and allocator-ledger builds. All four
replicas per workload have identical proof and metadata counters, and all 64
ledger returns have zero live allocations and bytes. Counter runs are diagnostic;
none of their durations enters acceptance timing. The private metadata C owner
uses the candidate's current binding layout. Emitted counters preserve saved
reference words and count both metadata branches directly after their guards.

C calls plus inline successes reproduce every original logical read; old proof
scans, checked reads, resets and discarded reuse match on all 32 workloads.
Complete plus reused metadata publications account for every logical read.
Disabled and failed reads are zero.

| Workload | Logical reads | C calls | Inline successes | Inline metadata reused | All metadata reused |
| --- | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | 814,304 | 20,034 | 794,270 | 784,083 | 804,079 |
| title-search-100k | 8,094,304 | 200,034 | 7,894,270 | 7,794,086 | 7,994,079 |
| normalize-title | 152,618 | 132,636 | 19,982 | 0 | 0 |
| word-frequency | 500,090 | 400,099 | 99,991 | 0 | 0 |

For title100k, 7,894,270 reads bypass C and 7,794,086 of those also reuse metadata.
The checked C remainder reuses metadata 199,993 times. Only 100,225 of 8,094,304
logical reads require full publication. Both proposed mechanisms execute;
failure to activate them does not explain the absent end-to-end improvement.
Normalize-title and word-frequency require complete metadata on every read,
including every inline certified success.

## Emitted code and changed direction

Private capture of the uninstrumented generated title100k code preserves the
original output and closes all 47 publication/disposal lifetimes. The mapped
function's symbol, typed registers and operations match accepted capture; code
grows from 3,372 to 3,552 bytes. Assembler-wrapper text is byte-identical before
disassembly. Captures, mappings, disassembly and exact counted paths are archived.

The already-bound accepted ARRAY_GET_IN_BOUNDS caller plus C helper has 67 static
instructions, 21 loads, 8 stores and 10 conditional branches. The candidate's
certified/reused path has 42 instructions, 20 loads, 6 stores and 6 branches.
The later STRING_EQUAL still occupies 588 bytes. These counts exclude surrounding
operations, cold paths, CPU cycles and cache effects and are not a speed model.

Repeated isolated string-bridge variants have not delivered the required gain.
Stop this micro-optimization series. Before another production candidate, explain
the complete original search loop and source/JIT startup costs using the accepted
baseline. Relate phase attribution and emitted dataflow to end-to-end timing;
reduced instruction counts alone are insufficient motivation for another patch.

The current mapped loop executes nine source instructions per failed comparison:
range check/branch, checked index capability, string read, equality/branch,
index add/copy and backedge. Six registers change. A generic checked loop
composition is a hypothesis for reducing those repeated materializations,
following D109 exact-prefix rules. It must match typed CFG/dataflow only, retain
every selected child check, borrow parent/immutable target only inside a
nonallocating interval, and publish full state at exact fuel/failure exits.
No source/workload/symbol selector, source rewrite, retained certificate or
rejected metadata patch is required. Admission, fuel, failures and lifetimes must
have independent ordinary-VM/native tests before fixed early/full gates. No
performance claim or accepted implementation follows from this proposed direction.

Stage began 2026-09-12T13:12:04.353824+00:00; due 2026-09-12T16:12:04.353824+00:00; evidence prepared
2026-09-12T13:43:25.149977+00:00; deadline met: **True**. Commit/push time is recorded separately.
