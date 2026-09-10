# CP42: native-call checkpoint rejected by the performance gate

Date: 2026-09-10. Status: **correctness passed; performance acceptance failed**.
The candidate is published as a reproducible patch, not merged into the
production compiler. The existing integrated JIT remains opt-in. Complete
Tier 1 acceptance and Go/Rust parity remain open.

The [SMART plan](../vm-performance-plan.md) required stack/queue improvements
of at least 15%, DSU of at least 5%, and no confirmed regression above 2% across
all 32 workloads, including a separate default-interpreter check. The speedup
targets passed; frequency-map failed the regression gate in both configurations.
This decision was recorded at 10:55 UTC, before the 11:12 UTC checkpoint deadline.
Source parent: `8c14357c3dfb4f6cb116cd1145327c5f8c3c8d8b` plus indexed inputs
in the [validation record](cp42-sljit-calls-validation-2026-09-10.json). The compiler embeds that parent identity; the source
and compiler SHA-256 values identify the measured candidate.

## Rejected candidate

The candidate D111 tier compiles bounded direct callees reachable from selected
backedge functions in the unlimited-frame/no-shared-counter mode. Calls and
returns transfer between compiled labels within a single native trampoline.
The same independently compiled activation owner serves ordinary VM dispatch
and the native tier, preserving logical depth, arguments, return checks and root
lifetimes. The frame owner retains all buffer allocation and cleanup.

An invocation context is prepared once. A transfer refreshes registers, views
and the actual callee PC after any frame-vector relocation. Cold exit publishes
the next uncommitted instruction in that callee; an already executed prefix,
including mutations, is never replayed. Finite/shared calls retain ordinary
execution and exact original-instruction accounting. Dependency, code budget,
W^X and unsupported-host/publication fallback rules are unchanged.

The shared activation owner borrows stable frame slots only after the reserve
that may relocate them and publishes the count only after roots are ready.
The allocator retains its original value-passing boundary. This reduces repeated
stack-container reads without changing allocation order or failure paths.
Arguments use a once-checked immutable contiguous span instead of repeating
generic vector lookup for each argument.

## Validation

The [validation record](cp42-sljit-calls-validation-2026-09-10.json) contains the complete commands, compiler/input hashes,
configuration flags, logs and correctness replays:

| Configuration | Tests | Frozen workload correctness |
|---|---:|---:|
| macOS ARM64 Release, JIT ON | 92/92 full CTest | 32/32 paired matrix |
| macOS ARM64 Release, JIT OFF | 86/86 full CTest | 32/32 paired matrix |
| macOS ARM64 ASan/UBSan, JIT ON | 8/8 focused | 32/32 |
| macOS x86-64 Release under Rosetta, JIT ON | 7/7 focused | 32/32 |

Focused fixtures assert that nested calls and 10,000 recursive logical frames
execute in one native entry. They check argument transport, call-depth traps,
exact overflow exits and the current leaf PC after frame growth. Another fixture
writes an array exactly once, exits inside the leaf before allocation, forces
collection and resumes through the return chain. The bounds-panic variant
checks G4002 and all three logical frames with their exact source spans.

Call initialization and overflow paths exhaust 27 ordinary allocation failure
indices each. Managed calls exhaust 34/24 indices for forced collection/write
once and 35/25 for panic-frame construction in unmetered/metered mode. Existing
code ownership tests exhaust 13 ordinary and four platform indices for each
fuel mode; the central code-page tests retain their separate five allocator/two
platform cases. Final teardown asserts zero live allocation/byte and mapping/byte
ledgers where those owners are directly controlled by the fixture.

Architecture, memory policy, roadmap, documentation, specification terms and
generated-artifact gates pass. macOS ASan uses detect_leaks=0; Linux LeakSanitizer
and complete Tier 1 execution remain open. Rosetta verifies x86 functionality,
not physical x86 performance. These results do not enable JIT by default.

## Paired performance

Each matrix contains 896 fresh processes: all 32 unchanged workloads, two pinned
compilers, three warmups and eleven samples. Participant order alternates and
workload order rotates. All 114 source/harness digests and expected outputs pass;
source preparation, startup and the 67,108,864-byte heap limit stay measured.
Builds, tests and profiling do not overlap either timing series.

The JIT control is the published 8c14357 integration candidate with SHA-256
83564606c695677e4c1ac88528bf7a80ce291a65d84d9b3303e76b33e57ea07d.
The interpreter control is the byte-identical f43647 production artifact used
by the 8c integration OFF check, with SHA-256
0f1f65ee914aa680ed5fd7e7b4b69cec49a3b20aa916a503f6a3f9a441d4fce9.
Go, Rust, Bun and native Marimay are not remeasured in these two comparisons.

For JIT, the median sum is **14317.26 → 13538.86 ms (-5.4%)**. The [full raw matrix](cp42-sljit-calls-jit-paired-2026-09-10.json) retains every observation.

For default interpreter, the median sum is **26720.94 → 26602.41 ms (-0.4%)**. The [full raw matrix](cp42-sljit-calls-interpreter-paired-2026-09-10.json) retains every observation.

The full matrices identified frequency-map as the only workload above 2% in
each configuration. Before rerunning it, the confirmation policy was recorded:
exactly one additional 3+11 paired series per flagged case, with both series
reported. A second median above 2% confirms rejection; there is no retry-until-
pass loop. Both confirmations failed:

| Configuration | Series | Control, ms | Candidate, ms | Change |
|---|---|---:|---:|---:|
| JIT | [paired](cp42-sljit-calls-jit-paired-2026-09-10.json) | 344.61 | 351.92 | +2.12% |
| JIT | [confirmation](cp42-sljit-calls-jit-frequency-map-confirmation-2026-09-10.json) | 342.95 | 351.44 | +2.47% |
| Interpreter | [paired](cp42-sljit-calls-interpreter-paired-2026-09-10.json) | 470.56 | 486.69 | +3.43% |
| Interpreter | [confirmation](cp42-sljit-calls-interpreter-frequency-map-confirmation-2026-09-10.json) | 473.89 | 489.73 | +3.34% |

The complete JIT comparison is shown below. Small movements within noise are not individual speedup claims.

| Workload | Previous JIT, ms | Candidate, ms | Change |
|---|---:|---:|---:|
| `title-search-10k` | 119.45 | 120.20 | +0.6% |
| `title-search-100k` | 648.72 | 651.09 | +0.4% |
| `normalize-title` | 201.34 | 184.13 | -8.6% |
| `word-frequency` | 339.22 | 333.21 | -1.8% |
| `report-builder` | 131.02 | 120.95 | -7.7% |
| `report-concat` | 205.39 | 197.40 | -3.9% |
| `map-int` | 87.91 | 88.34 | +0.5% |
| `map-string` | 113.97 | 113.96 | -0.0% |
| `map-string-book` | 127.80 | 124.97 | -2.2% |
| `set-string-churn` | 144.51 | 142.20 | -1.6% |
| `catalog-match` | 229.86 | 218.07 | -5.1% |
| `catalog-dotcall` | 176.31 | 168.25 | -4.6% |
| `catalog-interface` | 234.49 | 222.44 | -5.1% |
| `record-reference-copy` | 160.39 | 156.45 | -2.5% |
| `record-create` | 195.12 | 190.98 | -2.1% |
| `array-grow` | 166.71 | 154.72 | -7.2% |
| `array-reserved` | 166.49 | 154.41 | -7.3% |
| `bfs-int` | 136.45 | 133.52 | -2.1% |
| `bfs-record` | 150.62 | 146.93 | -2.5% |
| `catalog-pipeline` | 434.39 | 439.48 | +1.2% |
| `parse-result` | 111.86 | 106.76 | -4.6% |
| `insertion` | 228.01 | 228.78 | +0.3% |
| `heap-sort` | 426.31 | 417.68 | -2.0% |
| `stack` | 732.33 | 564.90 | -22.9% |
| `queue` | 847.19 | 611.47 | -27.8% |
| `min-heap` | 1088.36 | 1036.76 | -4.7% |
| `dsu` | 2620.29 | 2418.81 | -7.7% |
| `bfs` | 3036.20 | 3024.19 | -0.4% |
| `dijkstra` | 261.64 | 261.25 | -0.1% |
| `frequency-map` | 344.61 | 351.92 | +2.1% |
| `unique-set` | 260.42 | 263.18 | +1.1% |
| `rune-frequency` | 189.90 | 191.48 | +0.8% |

## Reproduction and next boundary

The [candidate patch](cp42-sljit-calls-candidate-2026-09-10.patch) applies to
`8c14357c3dfb4f6cb116cd1145327c5f8c3c8d8b`. Applying it to a separate temporary
Git index reproduced all 17 indexed implementation, contract and test inputs
byte for byte. Use a clean checkout/worktree of that parent, apply the patch,
then follow the patched `docs/implementation/vm-jit.md` CMake instructions with
the pinned local SLJIT input. The validation JSON records the exact flags,
compiler hashes, source hashes and comparison commands. Preserve the frozen
114-file corpus and compare the indicated baseline/candidate snapshots.

Two [rejected precursor experiments](cp42-sljit-calls-experiments-2026-09-10.json)
retain raw samples, compiler metadata and replay patches: global ThinLTO improved
catalog-interface but regressed BFS by 16.5%; passing the allocator by address
regressed BFS by 10.6% and missed the DSU target. Neither change is retained in
the final candidate. Restoring value passing removed that BFS regression.

The next call-stage hypothesis is to keep the ordinary VM frame cursor in local
C variables and synchronize it only across actual call/return/native transfers.
The shared logical transition protocol can remain centralized while reducing
C aliasing and repeated container reads. This is a new candidate to measure,
not an accepted optimization or a changed acceptance threshold.

A separate preparation candidate is the source encode/decode round trip and
repeated verification in `cli_run_program.c`. D049 and the toolchain spec
currently require that boundary. Any replacement needs an accepted decision,
normative text, hostile-bytecode coverage, immutable verified ownership, the
existing encoded-size/heap/capability limits and B0001/I0002 diagnostics. No
speedup from that proposed change is claimed here.
