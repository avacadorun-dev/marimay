# VM and native performance: working SMART plan

Date: 2026-09-10. This plan scopes performance work under D111; it does not
change language semantics, release gates or the current opt-in JIT status.

## Objective and measurement

The user's objective is Go/Rust parity on the unchanged 32-workload corpus,
first removing systemic VM costs, with semantics, safety and limits preserved.
The proposed numerical definition of parity is a median no greater than
**1.10 times the faster of Go and Rust on each workload**, evaluated separately
for native Marimay and VM source execution. This tolerance is a working
proposal, not a user-ratified requirement or an achieved result. A good aggregate
score cannot hide a failed workload.

Every accepted comparison pins compiler/runtime inputs, the 114 frozen
source/harness hashes, workload arguments and expected results. It uses three
warmups and eleven fresh-process samples, alternating participants and rotating
workloads. VM timing includes source loading, checking and lowering; the managed
heap remains limited to 67,108,864 bytes. Builds, tests and profiling finish
before timing. A cache experiment must report cold and warm costs explicitly.
Contemporaneous Go/Rust measurements are required for a parity claim; an earlier
table or a two-Marimay comparison cannot establish it.

## First bounded stage: rejected

At 09:12 UTC on 2026-09-10 the current stage was timeboxed to **11:12 UTC
(18:12 Asia/Krasnoyarsk)**. The deliverable is either a verified, measured change
committed to main and pushed, or a documented failed acceptance gate. This is a
checkpoint deadline, not a promise of final parity within two hours.

- Specific: support direct native call/return transfers through the existing
  logical frame protocol, using the shared SLJIT adapter for ARM64 and x86-64.
- Measurable: relative to published integration `8c14357`, improve VM `stack`
  and `queue` by at least 15% and DSU by at least 5%, with no confirmed regression
  above 2% across all 32 workloads. Check the default interpreter separately.
- Achievable scope: the cost map identifies millions of ordinary calls, and
  the six-workload experiment already supports those three improvement targets.
  The full matrix and final validation still decide acceptance.
- Relevant: reduce VM dispatch/activation overhead shared by ordinary programs;
  do not recognize benchmark names or rewrite their algorithms.
- Time-bound: publish the checkpoint outcome by the time above. A failed
  experiment remains rejected even when the timebox expires.

Native calls retain logical depth, exact bounded/shared fuel, managed roots,
allocation failure behavior, trap/panic frames and the actual callee exit PC.
Required checks include conformance and differential suites, deterministic
allocation/publication failures, sanitizers, architecture and documentation
gates. Rosetta is functional x86 evidence; physical x86 performance and complete
Tier 1 results remain separate open gates.

At 10:55 UTC the [call-stage checkpoint](evidence/cp42-sljit-calls-2026-09-10.md)
was rejected: stack/queue improved by 22.9%/27.8% and DSU by 7.7%, but the
single planned frequency-map confirmation showed 2.5% regression with JIT
and 3.3% without it. All correctness checks passed. The candidate is preserved
as an indexed source patch and measured evidence; its production code is not
accepted by this checkpoint. The next candidate must remove that regression
while retaining the original speedup targets and full-matrix gate.

## Second bounded stage: frame cursor held

The second stage started at **11:12 UTC on 2026-09-10** and is due by
**13:12 UTC (20:12 Asia/Krasnoyarsk)**. It retains all acceptance criteria above
and the same published JIT and interpreter controls. The specific change is a
local current-frame pointer in the VM coordinator, refreshed after activation
transfers. Cold operations cannot relocate this invocation's frame vector.
This removes repeated frame-container reads without changing call semantics,
compiler-wide optimization flags or safety checks.

Preliminary paired measurements put frequency-map at -0.3% without JIT and
+1.1% with JIT; BFS is -0.4% with JIT. These are admission experiments, not the
full acceptance matrix. The deadline deliverable remains either accepted code,
tests and measurements committed to main and pushed, or a recorded failed gate.

The [full checkpoint](evidence/cp42-sljit-cursor-2026-09-10.md) passes local
correctness and the nominal median criteria: stack/queue improve 22.9%/26.6%,
DSU improves 10.6%, and the only flagged JIT median (BFS +9.4%) has a +0.3%
single confirmation. The interpreter has no median regression above 2%.
Nevertheless, the implementation is held: a slow BFS mode recurs in all three
series, while the control stays stable. The recorded outcome distinguishes this
unresolved stability concern from the predeclared median rule, which passed.
The candidate remains a replayable patch; its production code is not merged.

Post-measurement sampling locates 49.6% of BFS and 58.2% of DSU samples under
JIT object binding. The next bounded experiment should test cheap revalidation
and reuse of already checked object views, with explicit GC/provenance
invalidation and the same unchanged-corpus acceptance. Its implementation and
deadline must be recorded before that experiment starts.

## Checked object-view stage: accepted locally

This stage starts at **12:18 UTC on 2026-09-10** and is due by
**14:18 UTC (21:18 Asia/Krasnoyarsk)**. It tests reuse of the
existing JIT array/record view after exact function/layout, execution, full
reference and runtime GC-epoch revalidation. Collection must invalidate the
proof before tracing/sweeping; saturation disables persistent reuse. Every
miss retains the existing checked owner and all heap, fuel and depth limits.

Early acceptance against published `8c14357` requires BFS improvement of at
least 25%, DSU 20% and min-heap 10%. Full acceptance uses all 32 unchanged
workloads with JIT and the default interpreter independently. It retains the
2% median regression rule and adds a **5% P90 regression rule** before this
experiment starts, addressing the previous candidate's repeated slow mode.
P90 linearly interpolates sorted samples at `0.9 * (n - 1)`. Exactly one further
3+11 series confirms each flagged workload; a threshold exceeded in both series
rejects the candidate. All observations and both statistics remain visible.
Required correctness, failure, sanitizer and architecture checks precede any
acceptance, main commit and push. A missed target records a rejected experiment.

Outcome: **accepted locally after the predeclared correctness and performance
checks**. The [complete evidence](evidence/cp42-sljit-views-2026-09-10.md)
records 93/93 enabled tests, 86/86 disabled tests, sanitizer/Rosetta checks and
all unchanged workload replays. The early BFS/DSU/min-heap gains are 31.3%, 47.4%
and 17.8%. The full JIT median sum falls 23.2%; OFF changes -0.05%, with no
threshold exceeded. The one JIT P90 flag (`catalog-interface`) is not reproduced
in its single confirmation, so neither median nor P90 has a confirmed regression.
A fresh five-participant full matrix is also recorded. This accepts the opt-in
implementation, not default enablement, Tier 1 validation or overall parity.

## Sequence after this stage

1. Follow the measured object-binding profile. Reduce repeated managed-object validation,
   view binding and register/frame traffic where the profile shows the largest
   remaining cost. The optimizing tier should retain hot scalar values in CPU
   registers and consider bounded leaf inlining with exact logical-frame
   materialization on exits. Any retained view needs explicit GC/provenance
   invalidation; these are separate proof obligations, not unconditional skips.
2. Address source preparation as its own measured stage. Retain all source-run
   costs and validate any prepared artifact against source/dependency changes,
   corruption and execution options.
3. Address native text and collection/runtime costs independently; faster VM
   dispatch alone does not close the native Go/Rust gap.
4. Rerun all five participants on the unchanged corpus, then the supported
   platform matrix. Claim parity only when the per-workload acceptance holds.

Each subsequent implementation stage needs an exact baseline, measurable
acceptance and a bounded checkpoint before work starts. A substantiated final
parity date is not yet available: source preparation, runtime costs and platform
coverage are still unresolved. The current evidence starts with the
[integrated JIT comparison](evidence/cp42-sljit-integration-2026-09-10.md) and the
[VM cost map](../research/vm-execution-cost-map-2026-09-09.md).

Reference designs: LLVM describes [mapping virtual values to physical registers
and spill slots](https://llvm.org/docs/CodeGenerator.html#register-allocation);
LuaJIT's [snapshot restoration](https://github.com/LuaJIT/LuaJIT/blob/v2.1/src/lj_snap.c)
reconstructs interpreter state on exits. These inform the proposed next tier;
they are not imported implementations or evidence that Marimay already has it.
