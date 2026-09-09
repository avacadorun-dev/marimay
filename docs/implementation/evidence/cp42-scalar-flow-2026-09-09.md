# Scalar/control MIR preparation and native composition checks

Local Apple M2 / macOS arm64 evidence, 2026-09-09; parent `8ac216c`.
[Raw matrices, probes, source hashes and checks](cp42-scalar-flow-2026-09-09.json).
D106 is an intermediate shared compiler improvement. Parity, CP42, startup
budgets, CLI0 and other Tier 1 evidence remain open.

## Change and correctness boundary

The shared CLI preparation pipeline simplifies privately cloned typed MIR
blocks before the existing range pass. It propagates local scalar copies and
successful exact scalar results, hoists uniquely defined scalar literals,
removes dead nontrapping scalar definitions using backward liveness, threads
empty jumps and removes unreachable blocks. Mutable register definitions,
call/construction metadata reads and implicit deferred cleanup are accounted
for. No managed allocation or copy, host effect, potentially failing check,
floating-point arithmetic or memory load is speculatively removed.

The original input is published over only after successful verification. Work
or allocation exhaustion leaves it unchanged and uses the checked fallback.
Invalid transformed MIR is a compiler error. Finite fuel still counts the
verified bytecode actually supplied; source compiler revisions need not emit
identical numbers of instructions. This is neither a new wire opcode nor a
native compiler hidden behind VM execution.

## Measured result

Final system `mmay` passes all 32 unchanged standard workloads. Native uses
3 warmups and 11 samples; VM uses 3 samples without warmups for diagnosis.
Source and harness hashes match their respective prior matrices exactly.
The final binaries are measured only after validation finishes, without
concurrent builds or test runs initiated by this task. Host isolation remains
local-unverified; separate matrices and small differences are not controlled
language-wide scores. VM includes source loading/checking/lowering, while
native compilation is excluded. No cross-boundary VM/native parity ratio is
claimed.

| VM workload | Before, ms | After, ms | Before/after |
|---|---:|---:|---:|
| insertion | 7,765.96 | 5,946.71 | 1.31× |
| heap-sort | 1,909.57 | 1,684.71 | 1.13× |
| dsu | 5,780.01 | 5,658.36 | 1.02× |
| bfs | 6,256.77 | 5,565.16 | 1.12× |
| dijkstra | 9,409.11 | 6,755.03 | 1.39× |
| bfs-record | 278.02 | 250.31 | 1.11× |
| catalog-pipeline | 611.02 | 583.21 | 1.05× |

Summed VM median round: 47.21
→ 40.95 seconds.
System compiler SHA-256:
`e3f090261d119c48a74bd6623ed4129f03e5f6de51d110305908c60a53a9b32f`.

| Latest native workload | Marimay, ms | Go, ms | Rust, ms |
|---|---:|---:|---:|
| title-search-100k | 82.34 | 15.36 | 18.25 |
| normalize-title | 57.53 | 8.89 | 11.08 |
| word-frequency | 145.88 | 19.03 | 30.45 |
| map-string-book | 31.45 | 6.48 | 10.56 |
| set-string-churn | 47.43 | 10.16 | 17.21 |
| catalog-pipeline | 37.78 | 7.78 | 9.94 |
| bfs | 65.72 | 63.39 | 23.87 |

Text, collections and object construction still have substantial gaps.
These are open performance work, not accepted exceptions to the parity goal.

## Instruction demand and composition regression

A separate counting executable uses the prior probe sources, relinked against
the new compiler. Every checksum passes, and opcode sums equal independent
shared-fuel totals. Counting runs are excluded from timings.

| Workload | Before instructions | After instructions | Reduction |
|---|---:|---:|---:|
| insertion | 1,800,629,785 | 1,331,188,719 | 26.07% |
| DSU | 570,544,256 | 524,872,376 | 8.00% |
| Dijkstra | 989,567,569 | 502,462,668 | 49.22% |

The initial native matrix exposed BFS slowing from 66.81 to 109.96 ms. An
alternating three-variant release probe separated the array cleanup fix from
the scalar pass. Generated C showed that the native bulk-fill proof required
`COPY` from zero; D106 correctly folded this into a direct zero constant, so
bulk filling and dependent length proofs stopped applying. The proof now
accepts both equivalent zero forms while retaining its unique dominating
initialization, step, region, empty-array and mutation requirements. Nonzero
initialization remains rejected. A corrected alternating pair measured
64.41 ms before and 67.76 ms after, followed by the full final matrix above.
The rejected matrix and raw observations are retained instead of discarded.

## Allocation and validation

The mixed `defer`/closure/array fixture also exposed an existing native leak:
if literal-string promotion failed while building an array, a block-local
argument buffer was bypassed by function cleanup. It reproduced with and
without D106. That buffer now belongs to the activation, is cleared after
successful use, and is freed on every exit before deferred calls.

The full Release run initially passed 85/86 cases. The remaining CLI test
stopped at a successful optimizer fallback and therefore never reached runtime
allocation failures. Run/build sweeps now continue until the injected index is
past all attempted allocations; both pass. The full native suite is rerun
after the bulk-fill repair. Together the full run and focused reruns provide
passing applicable results for all 86 cases. Logs preserve the initial failure
and its resolution; this is not described as one clean 86-case run.

Focused scalar, array-cell and fill owners pass under Release and ASan/UBSan.
They cover integer extrema, traps and spans, divergence, mutable aliases,
metadata arguments, deferred cleanup, wire round trips, forced collection,
native O0/O2, all allocation indices and work-budget rollback. Native fill
planning sweeps allocation failures for copied/direct zero and nonzero input.
After teardown both live-allocation and live-byte counters are zero.
Linux LeakSanitizer remains a CI obligation; no other Tier 1 gate is closed.

## Book and remaining route

Vladimirov's backward liveness on printed **71–72 / PDF 72–73** and separation
of analysis/transformation on printed **73–74 / PDF 74–75** inform the pass.
[Application 5 in the reading journal](../../research/vladimirov-optimizing-compilers-2026-09-07.md)
distinguishes the borrowed method from Marimay-specific safety rules.

Remaining VM costs are now concentrated in the reduced instruction stream,
ordinary typed-array/record accesses and direct-call preparation. Next work
must address those shared owners with exact fuel, GC and failure guarantees.
Native text/Option/Result construction and measured Map/Set paths remain part
of the same objective. No benchmark-specific implementation, weakened input,
unchecked semantic shortcut or release-gate waiver is introduced.
