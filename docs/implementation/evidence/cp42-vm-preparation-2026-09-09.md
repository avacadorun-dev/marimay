# CP42 VM source preparation: indexed integer-range predecessors

Local Apple M2 / macOS arm64 evidence, 2026-09-09. Parent revision `880d9bd`.
This is an implementation of accepted [D062](../../design/decisions.md).
It does not close performance parity, CP42, CP41 or Tier 1 acceptance.

## Cause and change

The shared integer interval analysis used a complete scan of every block to
find each block's predecessors on every widening/narrowing pass. VM source
preparation repeats this analysis in MIR verification, bytecode lowering,
encoding, decoding and final verification. A short `map-int` CPU sample found
the interval analyzer in those preparation stages as well as ordinary runtime
execution; the uncharged predecessor search was quadratic in CFG blocks.

`base/integer_range_edges.c` builds one temporary, checked allocator arena with
at most two edges per block. Incoming lists preserve ascending predecessor
order; equal branch targets contribute once. Only real predecessors are
visited during lattice iteration. The interval rules, traversal of blocks,
widening/narrowing schedule, charged work, proof acceptance and wire/VM
verification chain are unchanged. No source syntax, arithmetic semantics,
execution fuel, heap budget, benchmark input or public API changes.

The CFG owner releases its arena after one analysis, including work exhaustion
and allocation failure. It cannot preserve proof state between invocations.
The persistent analysis result has the same representation as before.

## Paired 3+11 measurements

The reference corpus is the user's complete `comprehensive-local.json`, SHA-256
`00c1af8e3ce02237bfb5377e161b108db9d56b93eb7b1ebbb23f91f0a9464f49`.
All **114** recorded source/harness hashes match before and after the paired
run. The compiler, runtime and public headers are copied into independent
immutable layouts; the two compiler hashes are:

- Control (`880d9bd`): `4597174a95b92e264353b7a85cbf7fc964b676200fd329f094dde8d9899898e0`.
- Candidate: `aa3951f96f56c830bb2386261ad105cba08784605fe67bfd1a2ac8cf9c21f213`.

Every observation is a fresh `mmay run --engine vm` process, including source
loading, checking and lowering. Arguments, full-size workloads and the
**67,108,864-byte managed-heap limit** are copied from the reference. Each
version receives three warmups and eleven samples. Before/after pairs and
workload order rotate; compiles, tests and CPU sampling finish before timing.
The host is `local-unverified`, rather than a dedicated Tier 1 benchmark host.

| Workload | Control median, ms | Candidate median, ms | Candidate/control |
|---|---:|---:|---:|
| map-int | 112.70 | 103.87 | 0.922 |
| parse-result | 133.44 | 124.37 | 0.932 |
| record-reference-copy | 198.67 | 189.57 | 0.954 |
| catalog-pipeline | 560.73 | 552.14 | 0.985 |
| title-search-100k | 897.36 | 898.59 | 1.001 |
| heap-sort | 1,639.88 | 1,644.65 | 1.003 |
| bfs | 5,465.43 | 5,483.87 | 1.003 |

All 196 paired observations pass exact stdout/stderr/exit checks. The short
cells remove about 8.6–9.1 ms of preparation. The three long controls do not
demonstrate a speedup; their 0.1–0.34% differences are below the protocol's 2%
floor for reported measurement noise. These results do not establish VM execution-loop
parity. [Raw paired observations and toolchain hashes](cp42-vm-preparation-paired-2026-09-09.json)
retain the complete sample distributions.

## Complete unchanged matrix

The [full five-participant table](cp42-vm-preparation-comprehensive-2026-09-09.md)
and [raw matrix](cp42-vm-preparation-comprehensive-2026-09-09.json) contain all
**32 workloads × 5 participants × (3 warmups + 11 samples) = 2,240** passing
observations. All 114 source/harness hashes still match the reference, and the
candidate compiler hash is the same as in the paired series. The matrix's
`passed` status records matching observations, not performance acceptance.

| Participant | Sum of workload medians, ms |
|---|---:|
| Marimay native | 1,083.43 |
| Go native | 552.32 |
| Rust native | 467.56 |
| Marimay VM source run | 29,312.31 |
| Bun / TypeScript | 989.44 |

Native's geometric ratios are 2.07× Go and 2.19× Rust. Only three native
workloads meet the research plan's 1.25× limit against the faster of Go/Rust.
VM's geometric ratio is 17.59× Bun; `min-heap` remains 5,773.97 ms and BFS
5,721.49 ms. Per-workload gaps remain authoritative; no aggregate closes parity.

The matrix also captures D108, which was already present in this change's
control compiler: insertion is 980.82 ms and DSU 3,907.95 ms, compared with
6,131.72 and 5,540.29 ms in the user's older compiler report. Those larger loop
improvements belong to D108. The paired series above isolates this change's
preparation effect. Native execution timings in separate matrix runs are not
evidence that this analysis-only change improves native runtime performance.

## Correctness and resources

`test_integer_range_edges.c` records the pre-index analyzer's facts and work
from `880d9bd`. Five CFG shapes cover a sparse chain, a backward-numbered loop,
an unreachable block, duplicate branch targets, `<`/`<=` refinement and a
redefined condition. The transcript includes all incoming intervals and all
proof decisions at **2,141 finite work limits**, plus unmetered execution.
Every transcript is identical to the old implementation.

A 64-block sparse chain retains exactly 1,140 work units but performs only
253 terminator reads, including index construction. Separate cases cover
self/entry edges, failed adapter reads, invalid successors, checked size
overflow, repeated disposal, and every reachable deterministic allocation
failure through successful analysis. Full teardown asserts both
`live_allocations == 0` and `live_bytes == 0`.

- Full Release CTest: **86/86 passed**, 358.11 seconds with `-j4`.
- ASan/UBSan frontend, bytecode, runtime, native and allocation indices 0–7:
  **12/12 passed**, 444.01 seconds with `-j2`.
- Native coverage includes O0/O2, forced GC and VM/native comparisons. The
  existing source conformance, negative diagnostics, formatter, CLI resource,
  stream and installation checks pass in the full suite.
- Architecture, roadmap, C memory policy and whitespace checks pass.

Apple sanitizer runs use `detect_leaks=0`; they do not replace the required
Linux LeakSanitizer CI contract or prove all Tier 1 platforms.

## Reproduction

Build the unchanged `880d9bd` source in `build/control` before editing; retain
that executable and its runtime metadata. Build the candidate in `build/release`.
From the compiler checkout, run:

```sh
cmake --build build/release -j6
ctest --test-dir build/release -j4 --output-on-failure
cmake --build build/asan-ubsan -j6
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/asan-ubsan -j2 \
  -R '^(mmay_c_unit_(frontend|bytecode|runtime|native)|mmay_cp31_allocation_[0-7])$' \
  --output-on-failure
python3 tools/performance/compare-vm-preparation.py \
  --benchmarks ../algorithm-benchmarks \
  --reference ../algorithm-benchmarks/benchmarks/results/comprehensive-local.json \
  --before build/control/mmay --after build/release/mmay \
  --output build/vm-preparation-paired.json \
  --workload map-int --workload parse-result \
  --workload record-reference-copy --workload catalog-pipeline \
  --workload title-search-100k --workload heap-sort --workload bfs
MMAY="$PWD/build/release/mmay" \
  python3 ../algorithm-benchmarks/benchmarks/catalog_bench.py \
  --anchors --samples 11 --warmups 3 \
  --output "$PWD/build/vm-preparation-comprehensive.md"
./tools/check-architecture
./tools/check-roadmap
./tools/check-c-memory-policy
git diff --check
```

The paired runner uses the existing catalogue protocol and snapshot owners;
it rejects changed reference inputs and writes each observation to a durable
journal before proceeding. No new idea or page attribution to Vladimirov's
book is claimed for this change.
