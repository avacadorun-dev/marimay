# Compiler IPO, shared calls and VM dispatch state

Status: **accepted-local**. Full Go/Rust parity, default JIT enablement, complete
Tier 1 and Linux LeakSanitizer coverage remain open.

[Validation summary and exact inputs](cp42-ipo-shared-vm-2026-09-11-validation.json);
[complete validation, rejected variants and reproduction files](cp42-ipo-shared-vm-2026-09-11-validation.json.gz);
[replayable source patch](cp42-ipo-shared-vm-2026-09-11-candidate.patch).
The complete validation file is gzip-compressed JSON; decompress with `gzip -dc`
and check the encoded and decoded SHA-256 values in the summary.
Replay the source patch on its recorded parent with `git apply --unidiff-zero`.

## Implementation

Prepared direct calls bind their immutable argument spans and callee image IDs.
Separate call/return bridges enter the same shared activation and root protocol.
Existing frame capacity avoids a reserve call; the frame owner reuses one invalid
reference value during reset. Dynamic depth, argument/return values, root lifetimes,
source positions, cold exits and allocation failures retain D105/D108/D110/D111.

The D104 interpreter region keeps its cursor locally and refreshes immutable block
metadata only on control transfers. Every exit publishes the next unexecuted
instruction. One shared region body is specialized at entry for D087 completely
unmetered execution. Any finite frame budget or non-null shared budget, including
`SIZE_MAX`, keeps exact per-instruction accounting. No source identity is inspected.

`MMAY_COMPILER_IPO=AUTO` probes C-toolchain support and enables optimized build
configurations. Unsupported toolchains keep ordinary C17 compilation; OFF disables
this policy and ON requires support. Native runtime archive profiles and strict
floating-point options remain independent. All 47 objects in each native runtime
profile match the accepted baseline. The experimental JIT remains opt-in.

## Selection and rejected variants

The call refactor without IPO missed its 15% DSU/stack target at 8.96%.
With IPO it passed early measurement, but failed the complete interpreter gate:
the median sum rose 8.46%, and all nine flagged regressions repeated. Original
source plus IPO reproduced the large heap-sort/dijkstra regressions; the call
refactor changed their IPO timings by less than 1%. Separate JIT ablation found
9.55% further DSU/stack latency reduction from the refactor beyond IPO alone.

Three full-size sampling trials per compiler/workload attributed 95–99% of
heap-sort/dijkstra exclusive samples to the ordinary hot-region owner. Local
cursor publication removed the large regressions but missed the new 10% early
target at 7.07%; block-span reuse reached 7.71% and also remained rejected.
Entry specialization then passed both early gates: 10.97% mean latency reduction
across the four interpreter cases and 22.13% across DSU/stack with JIT. The early
title-search series had substantial variability; complete measurements follow.

The original v2 deadline was missed across a recorded system sleep during tests;
no acceptance timing was active then. The final v3 deadline remains
`2026-09-11T06:52:27.583258+00:00`; evidence preparation deadline met: **True**.

## Full paired measurements

Both modes use all 32 unchanged workloads, 114 input hashes, original arguments
and stdout, 64 MiB heap, three warmups and eleven fresh-process paired samples.
No build, test or profiler from this task ran concurrently with acceptance timing.
Host isolation remains local-unverified; power, thermal and process records are kept.

JIT median sum: **8901.94 → 7462.63 ms
(-16.17%)**; geometric mean speedup
**1.1963x**. Interpreter median sum:
**27120.26 → 22595.32 ms
(-16.68%)**; geometric mean speedup
**1.1999x**. Sums describe the workload set, not one executable.

Any median regression above 2% or P90 above 5% receives exactly one confirmation.
P90 is sorted sample ten of eleven. Repeating the same flagged metric rejects the
candidate. No confirmed regression remains and neither mode's median sum rises.
The raw records preserve every flagged case and confirmation.

| Workload | JIT before, ms | JIT after, ms | Interpreter before, ms | Interpreter after, ms |
|---|---:|---:|---:|---:|
| title-search-10k | 106.06 | 92.51 | 136.17 | 121.20 |
| title-search-100k | 576.69 | 504.10 | 879.70 | 783.17 |
| normalize-title | 170.76 | 141.58 | 202.93 | 169.15 |
| word-frequency | 322.25 | 270.32 | 391.59 | 328.08 |
| report-builder | 98.55 | 83.27 | 129.96 | 110.73 |
| report-concat | 177.37 | 163.91 | 209.73 | 191.65 |
| map-int | 81.70 | 67.64 | 96.75 | 81.74 |
| map-string | 105.88 | 89.16 | 124.42 | 105.38 |
| map-string-book | 111.27 | 93.10 | 124.16 | 103.22 |
| set-string-churn | 133.26 | 113.00 | 138.17 | 118.70 |
| catalog-match | 157.56 | 124.66 | 262.57 | 208.05 |
| catalog-dotcall | 133.00 | 103.75 | 191.05 | 154.62 |
| catalog-interface | 173.75 | 127.18 | 251.64 | 184.84 |
| record-reference-copy | 108.56 | 88.36 | 170.72 | 140.93 |
| record-create | 143.72 | 117.59 | 207.46 | 171.59 |
| array-grow | 116.69 | 92.31 | 195.34 | 161.74 |
| array-reserved | 116.32 | 92.30 | 194.09 | 161.24 |
| bfs-int | 94.92 | 79.47 | 208.66 | 175.00 |
| bfs-record | 103.12 | 85.99 | 226.27 | 187.72 |
| catalog-pipeline | 265.00 | 230.44 | 532.71 | 449.18 |
| parse-result | 94.47 | 78.16 | 109.58 | 92.00 |
| insertion | 219.83 | 212.20 | 223.97 | 215.89 |
| heap-sort | 298.95 | 281.00 | 1661.81 | 1446.29 |
| stack | 428.83 | 322.85 | 803.05 | 612.71 |
| queue | 430.75 | 325.51 | 889.82 | 682.64 |
| min-heap | 718.25 | 636.84 | 5619.03 | 4575.18 |
| dsu | 1192.89 | 968.41 | 3847.71 | 3320.23 |
| bfs | 1363.50 | 1141.00 | 5588.30 | 4435.84 |
| dijkstra | 224.40 | 217.79 | 2427.22 | 2266.50 |
| frequency-map | 283.10 | 227.00 | 476.88 | 375.66 |
| unique-set | 198.14 | 167.59 | 353.79 | 269.42 |
| rune-frequency | 152.39 | 123.64 | 244.99 | 195.04 |

## Validation and fresh comparison

97/97 JIT-enabled tests and 87/87 interpreter tests passed. ASan/UBSan with IPO
and x86-64 under Rosetta each passed 12 focused suites and all32 correctness
replays. Coverage includes backward branches, mixed finite/shared limits, a shared
`SIZE_MAX` budget, arithmetic failure after loop iterations, cold exits, repeated
and alternating call layouts, distinct argument spans, retained allocation-free
calls, forced GC and every reachable deterministic allocation failure. Allocator
tests require both allocation and byte ledgers to reach zero. The original v1
new-test failure incorrectly expected zero frame fuel to mean exhaustion; the
documented invalid-argument expectation was restored and its failure log retained.

The new build-policy test checks C-only probing, fallback, explicit errors, build
profiles and target scope. All architecture, memory-policy and documentation gates
passed before measurement; publication is gated again before commit and push.

The [fresh five-participant table](cp42-ipo-shared-vm-2026-09-11-comprehensive.md)
([raw observations](cp42-ipo-shared-vm-2026-09-11-comprehensive.json)) covers all 160 outcomes with the
same 3/11 protocol. Descriptive geometric mean time ratios: Native/Go
**2.03x**, Native/Rust **2.19x**,
VM/Go **15.04x**, VM/Rust **16.18x**.
VM includes source loading/checking/lowering; Bun includes fresh-process startup,
TypeScript loading and JIT. Each Native/Go/Rust/Bun workload command resolves to
the same executable hash as in the preceding series. Cross-series timing changes
alone are therefore not attributed to changed binaries or another agent.
