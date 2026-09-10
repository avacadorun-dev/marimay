# CP42: frame-cursor candidate held; remaining JIT costs profiled

Date: 2026-09-10. Status: **local correctness passed; implementation held for
performance stability**. This checkpoint does not merge the candidate into the
production compiler or enable JIT by default. Go/Rust parity and full Tier 1
acceptance remain open.

The candidate extends the [previous native-call experiment](cp42-sljit-calls-2026-09-10.md)
with a local current-frame pointer in the VM coordinator. Cold operations cannot
relocate that invocation's frame vector. Successful ordinary calls, returns and
native transitions refresh the pointer; failure cleanup always reads the
actual stack. This avoids repeated frame-container reads while retaining the
shared activation owner and all logical call/root/fuel semantics.

Source parent: `deacce5c2e8a3a607042e8f61cb481ce21c09d12`. The
[candidate patch](cp42-sljit-cursor-candidate-2026-09-10.patch) was replayed in a
separate Git index and reproduced all 17 indexed implementation, contract and
test inputs. The [validation record](cp42-sljit-cursor-validation-2026-09-10.json)
contains their hashes, exact build/test commands, compiler hashes and logs.

## Correctness and measurement

| Configuration | Tests | Frozen workload correctness |
|---|---:|---:|
| macOS ARM64 Release, JIT ON | 92/92 full CTest | 32/32 paired matrix |
| macOS ARM64 Release, JIT OFF | 86/86 full CTest | 32/32 paired matrix |
| macOS ARM64 ASan/UBSan, JIT ON | 8/8 focused | 32/32 |
| macOS x86-64 Release under Rosetta, JIT ON | 7/7 focused | 32/32 |

The call fixtures cover native entry across nested calls and 10,000 logical
frames, exact overflow/depth exits, root preservation across frame relocation
and forced GC, mutation performed exactly once, and exact three-frame bounds
panic reporting. Allocation/publication failure sweeps retain zero allocation,
byte and code-mapping ledgers. The focused counts and full output are preserved
in the validation record. Architecture, memory policy, roadmap, documentation,
specification-term and generated-artifact gates passed.

macOS ASan uses detect_leaks=0. Linux LeakSanitizer and complete Tier 1 execution
remain open. Rosetta checks x86 functionality, not physical x86 performance.

Both matrices retain all 32 unchanged workloads, 114 source/harness hashes,
source-run preparation and startup, expected outputs, and the 67,108,864-byte
managed-heap limit. Each has 896 fresh-process observations: two pinned
compilers, three warmups and eleven samples, with alternating participants and
rotating workloads. Builds, tests and profiling do not overlap timing.

The JIT control is published integration `8c14357`, compiler SHA-256
83564606c695677e4c1ac88528bf7a80ce291a65d84d9b3303e76b33e57ea07d.
The interpreter control is the byte-identical f43647 artifact used by the
integration's OFF check, SHA-256
0f1f65ee914aa680ed5fd7e7b4b69cec49a3b20aa916a503f6a3f9a441d4fce9.
Go, Rust, Bun and native Marimay are not remeasured here.

## Result and disposition

The [JIT matrix](cp42-sljit-cursor-jit-paired-2026-09-10.json) reduces the median
sum **14602.15 -> 14020.05 ms (-4.0%)**. The
[interpreter matrix](cp42-sljit-cursor-interpreter-paired-2026-09-10.json)
is **26932.56 -> 26902.57 ms (-0.1%)**, with no median regression above 2%.
The original speedup targets pass:

| VM workload | Previous JIT, ms | Candidate, ms | Change |
|---|---:|---:|---:|
| stack | 746.72 | 576.06 | -22.9% |
| queue | 857.16 | 628.90 | -26.6% |
| dsu | 2704.03 | 2417.09 | -10.6% |
| frequency-map | 348.52 | 352.11 | +1.0% |
| bfs | 3075.67 | 3365.48 | +9.4% |

BFS is the only JIT median above 2%. Its
[single predeclared confirmation](cp42-sljit-cursor-jit-bfs-confirmation-2026-09-10.json)
is **3058.09 -> 3066.92 ms (+0.3%)**. Thus the exact predeclared median rule does
**not** confirm a regression. No additional timing rerun was made to obtain a
passing result.

However, the candidate has a repeatable slow mode that a median alone hides.
The following 3.3-second cut is a descriptive observation, not a newly claimed
predeclared statistical threshold:

| Series | Candidate samples >3300 ms | Control samples >3300 ms |
|---|---:|---:|
| Preliminary two-workload admission | 3/11 | 0/11 |
| Full matrix | 6/11 | 0/11 |
| Single confirmation | 3/11 | 0/11 |

All three series use the same candidate compiler SHA-256
34c934157b397fe8c609c7dab5ff0da3e3f956178c5207b4c7e91e77e744ee31.
The validation record preserves the raw BFS values for every series. The cause
is not established; these observations do not prove an ASLR, cache or scheduler
explanation. The implementation is held because this unresolved distribution
makes a stable performance claim premature, even though the nominal median
acceptance is met. The current-frame change is not accepted on that basis.

## Profile: next systemic target

After all timing completed, three fresh, unchanged runs each of BFS, DSU and
min-heap were sampled using macOS `sample` at 1 ms intervals for up to two
seconds per process. Every checksum and source hash passed. The
[profile artifact](cp42-sljit-cursor-profile-2026-09-10.json) contains the script,
commands, all nine raw profiles, parsed counts and compiler/source hashes.
Its elapsed observations include profiling overhead and are not benchmarks.
Sampling begins after process launch and may miss early startup and the tail.

| Workload | Samples | Under JIT object binding | Under native call transition |
|---|---:|---:|---:|
| BFS | 5072 | 49.6% | 0.0% |
| DSU | 5075 | 58.2% | 26.8% |
| min-heap | 2758 | 25.5% | 22.5% |

These are inclusive sampled-stack fractions, not instruction counts or a speedup
forecast. Descriptor lookup alone appears in 25.5% of BFS samples and 24.8% of
DSU samples (array plus record lookup); those fractions are already included in
object binding and must not be added to it. The current lookup repeats
structural type comparisons, while the managed owner repeats reference/view
validation after each native-entry boundary.

The next bounded experiment should retain a checked array/record view only
after cheap revalidation of full reference identity, function/layout, heap
provenance and a runtime-owned GC invalidation epoch. Any collection, including
an aborted one, must invalidate the retained result before touching object
lifetimes; epoch saturation must disable reuse. Array growth must be observed
through the live object's storage/length fields. Failed revalidation must use
the existing checked owner. This is an unimplemented proposal, with its own
alias/GC/growth/failure proof obligations, not permission to omit safety checks.
Register allocation, bounded inlining and source-preparation costs remain
separate subsequent targets in the [SMART plan](../vm-performance-plan.md).
