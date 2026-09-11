# JIT execution costs after canonical preparation deduplication

Status: **diagnostic-complete**. No additional optimization is accepted by this
record. The accepted compiler remains `cce195cc0b15a247c9db70424527f07169e8359c`.
Overall Go/Rust parity, default JIT enablement, Linux LSan and full Tier 1 remain open.

[Raw profiles, counters, input identities and reproduction scripts](cp42-jit-execution-profile-2026-09-11.json).
The unchanged Release compiler SHA-256 is
`517a8ea363b93e4ea36e2a6a48fa93867658c9acd26dffead64980e52cf2714b`.
The 20-minute diagnostic checkpoint started at 02:54:05 UTC on September 11;
its observations and selection were recorded before the 03:14:05 UTC deadline.

## Method

Three fresh, full-size runs each of DSU, BFS, stack and min-heap were sampled by
macOS `sample` at 1 ms for up to two seconds. All 12 outputs match the immutable
32-workload corpus; its 114 source/harness hashes and 64 MiB heap cap are retained.
Sampling attaches after process launch and can miss startup and tail work.
Inclusive counts overlap; exclusive counts partition sampled stacks. Anonymous
frames are left unattributed, including when nested under the JIT entry point.
These are diagnostic observations, not a 3-warmup/11-sample acceptance benchmark.

A separate counter-only compiler links instrumented copies of two focused JIT
owners before the exact baseline archives. Four further runs keep the same
arguments and expected stdout. Counters observe direct-call attempts from the
compiled transition owner; they do not count every cold interpreter call.
They record whether stack capacity exists, whether the inactive slot already
has the exact function layout and prepared roots, and whether verified metadata
permits borrowing caller roots. Production sources and baseline binaries are
unchanged. The counter compiler's elapsed time is not a performance result.

## Findings

| Workload | Samples | Transition inclusive | Anonymous exclusive | Compiled call attempts | Capacity available | Same layout prepared |
|---|---:|---:|---:|---:|---:|---:|
| dsu | 2891 | 50.64% | 48.70% | 15002064 | 15002063 | 14999998 |
| bfs | 3633 | 0.03% | 39.50% | 900 | 899 | 300 |
| stack | 969 | 46.44% | 8.46% | 4000002 | 4000001 | 999999 |
| min-heap | 1873 | 16.66% | 64.50% | 2500001 | 2500000 | 999997 |

DSU spends about half of sampled stacks inside the native call/return transition
and makes 15 million compiled call attempts. All but one have existing frame-vector
capacity; nearly all have the same prepared layout. Ten million calls go to a
30-operation leaf whose verified image permits borrowing caller roots. Therefore
additional allocation caching alone cannot explain or remove the remaining call
cost: the shared activation, initialization and transition protocol still runs.

Stack also places about 46% of samples inside transitions, but only one quarter
of its four million attempts reuse the same layout. Its next optimization must
handle alternating layouts or fall back without penalizing them. Existing D105
retention permits one inactive layout per depth, not an extra cache of activations.
BFS has only 900 observed compiled calls and negligible transition attribution;
this call candidate cannot be presented as a solution to its remaining cost.
Min-heap retains 16.66% transition attribution, with most exclusive samples still
anonymous. Mapping native code ranges and register/view traffic remains a separate
investigation; anonymous samples alone do not prove register allocation is dominant.

## Selected next candidate

Target the per-call/return protocol with a nonallocating path for already retained
activation storage, using immutable direct-call projections and the shared
`vm_activation.c` / `vm_frame.c` owners. Cover both same-layout reuse and existing
capacity across layout changes. Move invariant metadata work to preparation;
keep dynamic depth, argument/return handling, managed-reference reset, checked root
registration and source-PC publication in their authoritative owners. Any guard
miss must enter the current path before committing the call. No frame/panic policy
moves into the machine-code generator, and no extra cached activation is allowed.

Before implementation, record a fresh bounded candidate checkpoint against the
exact accepted ON/OFF binaries. The early 3-warmup/11-sample gate must reduce the
geometric mean DSU/stack source-run latency by at least 15%; a smaller result is
insufficient for this selected experiment. Admission still requires all 32 unchanged
workloads in both modes, no confirmed regression over 2% median or 5% P90, unchanged
source/output/heap constraints, and a nonincreasing sum of medians. Full tests,
precise finite/shared-fuel and cold-exit comparisons, managed roots, all reachable
allocation-failure indexes, sanitizer/Rosetta checks and repository gates remain
mandatory. This selection is not an implemented fast path or an accepted speedup.

## Reproduction

Restore each `reproduction_files` entry at its recorded repository-relative path,
checking its SHA-256. Use the exact recorded parent and compiler/build artifacts;
a rebuilt compiler with different bytes is a new diagnostic baseline. From the
worktree root run:

```sh
python3 build/jit-execution-profile/profile.py
python3 build/jit-execution-profile/call-counts.py
```

The sibling `algorithm-benchmarks` corpus must match the committed references.
The counter report preserves exact compiler/linker commands and diagnostic C
sources. Reproduction on a different OS needs its profiler and must retain that
change as new evidence. No generated-code execution time is inferred from the
anonymous frame label.
