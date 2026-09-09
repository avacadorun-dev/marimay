# CP42: portable VM activation buffer capacity reuse

Local Apple M2 / macOS arm64 evidence, 2026-09-09. Parent revision `8ebf5ec`.
Implementation contract: [D110](../../design/decisions.md), extending D105.
Performance parity, CP42 and Tier 1 acceptance remain open.

## Cause and implementation

The explicit VM frame stack already reused inactive buffers when the same
function returned to the same depth. Alternating functions released and
allocated register, defer and managed-root storage repeatedly even when their
sizes were stable. The frame owner now retains each buffer's capacity and grows
it only when required. Different layouts still initialize active register types
and invalidate managed references before argument installation. Their complete
root-location map is rebuilt before checked registration.

The old root map becomes inactive before a register buffer can move. Retained
root-frame objects keep stable addresses across frame-vector growth. The active
root-map length and its allocated capacity are separate, so smaller functions
cannot root stale trailing references and teardown frees the exact owned size.
Allocation failure preserves ownership of old buffers for normal cleanup.

This is portable C17 in `bytecode/vm_frame.c`; dispatch, MIR, public values,
call-depth and per-frame/shared fuel accounting are unchanged. It applies to
any verified callees, without source names, workload patterns, machine-code
emission or new dependencies. Native runtime and emission are unchanged.

The tradeoff is retained host capacity: each depth keeps the largest register,
defer and root-location buffers it has needed during this invocation. Inactive
storage does not keep managed objects alive. The existing 67,108,864-byte managed
heap limit and its accounting are unchanged; host capacities are not relabeled
as managed allocations.

## Measurements

All **896** fresh-process observations passed. All **114** source/harness hashes
match the unchanged reference. [Raw paired results](cp42-vm-frame-capacity-paired-2026-09-09.json) include every warmup/sample, exact commands and artifact hashes.

Pinned compiler SHA-256:

- Before (`8ebf5ec`): `2dade11024ec42f6c0758bab3e0b701270421850473797cf28cd6d6000fb890c`.
- After: `0f1f65ee914aa680ed5fd7e7b4b69cec49a3b20aa916a503f6a3f9a441d4fce9`.

| Workload | Before, ms | After, ms | Time change |
|---|---:|---:|---:|
| title-search-10k | 149.11 | 145.85 | -2.19% |
| title-search-100k | 923.48 | 896.95 | -2.87% |
| normalize-title | 222.45 | 212.62 | -4.42% |
| word-frequency | 409.53 | 409.05 | -0.12% |
| report-builder | 153.87 | 139.45 | -9.37% |
| report-concat | 235.76 | 223.06 | -5.39% |
| map-int | 107.03 | 107.37 | +0.32% |
| map-string | 137.75 | 135.30 | -1.78% |
| map-string-book | 148.35 | 134.84 | -9.11% |
| set-string-churn | 154.95 | 151.01 | -2.54% |
| catalog-match | 339.67 | 274.98 | -19.05% |
| catalog-dotcall | 243.97 | 198.32 | -18.71% |
| catalog-interface | 349.98 | 259.54 | -25.84% |
| record-reference-copy | 198.00 | 181.39 | -8.39% |
| record-create | 234.85 | 219.84 | -6.39% |
| array-grow | 207.55 | 204.87 | -1.29% |
| array-reserved | 206.72 | 204.23 | -1.20% |
| bfs-int | 221.71 | 219.10 | -1.18% |
| bfs-record | 239.47 | 237.19 | -0.96% |
| catalog-pipeline | 567.84 | 547.45 | -3.59% |
| parse-result | 130.23 | 121.81 | -6.46% |
| insertion | 231.25 | 232.29 | +0.45% |
| heap-sort | 1680.51 | 1683.12 | +0.16% |
| stack | 934.98 | 800.24 | -14.41% |
| queue | 1046.30 | 898.17 | -14.16% |
| min-heap | 5624.08 | 5642.22 | +0.32% |
| dsu | 3936.75 | 3898.56 | -0.97% |
| bfs | 5620.75 | 5607.34 | -0.24% |
| dijkstra | 2426.14 | 2417.47 | -0.36% |
| frequency-map | 490.57 | 484.95 | -1.15% |
| unique-set | 367.61 | 358.82 | -2.39% |
| rune-frequency | 252.40 | 250.84 | -0.62% |

The main improvements are catalog-interface (25.8% less time), catalog-match
and catalog-dotcall (about 19%), and stack/queue (about 14%). The largest median
increase is insertion at 0.45%; the long arithmetic/array loops are essentially
unchanged. This removes allocator work in alternating calls but does not close
the execution-loop gap. No Go/Rust parity or new native timing result is claimed.

The comparison includes full source loading, checking, lowering and VM
execution in every fresh process. It uses three warmups and eleven samples per
compiler per workload, rotating workload and participant order. Both compiler
installations, runtime archives and headers are independently pinned. All
workload arguments and the managed-heap limit come from the reference matrix.
No observations are selected or discarded.

The upstream metadata collector hardcodes `dedicated-runner-contract` without
checking host activity. The local comparison tool now preserves that value as
`collector_reported_mode` and sets the actual classification to
`local-unverified`. The same metadata-only correction is applied to this run,
which started before that correction. It changes no duration, ordering, command
or result. These local measurements do not claim dedicated-runner isolation or
performance acceptance on other architectures.

## Correctness and ownership

- Full Release CTest: 86/86 passed, 319.50 seconds with `-j4`.
- ASan/UBSan bytecode, runtime, native and allocation shards 0–7: 11/11 passed,
  419.09 seconds with `-j2`.
- The focused frame test grows from two to six registers and three managed
  locations, then alternates three layouts 48 times with no further allocation
  attempts. It checks changed types/reference positions, stale trailing
  references, forced collection, borrowed-to-owning transitions, stable roots
  across stack relocation, refusal to replace a linked frame and inactive
  teardown. Every deterministic failure index is replayed through successful
  completion; both `live_allocations` and `live_bytes` are zero after teardown.
- Existing tests cover exact finite/shared fuel, recursion/call depth, return
  validation, panic/defer order and VM/native behavior including forced GC.

Architecture, C memory policy, roadmap, documentation links, Python syntax and
whitespace checks pass.

Apple sanitizer runs use `detect_leaks=0`; Linux LeakSanitizer and other Tier 1
results are not inferred from these local tests.

## Reproduction

Retain the parent compiler and runtime installation independently before
building the candidate. From the compiler checkout:

```sh
cmake --build build/release -j4
ctest --test-dir build/release -j4 --output-on-failure
cmake --build build/asan-ubsan -j4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/asan-ubsan -j2 \
  -R '^(mmay_c_unit_(bytecode|runtime|native)|mmay_cp31_allocation_[0-7])$' \
  --output-on-failure
python3 tools/performance/compare-vm-preparation.py \
  --benchmarks ../algorithm-benchmarks \
  --reference docs/implementation/evidence/cp42-vm-preparation-comprehensive-2026-09-09.json \
  --before build/jit-baseline/run-0clipjt2/toolchain/bin/mmay \
  --after build/release/mmay --output build/frame-capacity-paired.json
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-roadmap
./tools/check-doc-links
git diff --check
```
