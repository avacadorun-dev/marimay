# VM and native performance: working SMART plan

Date: 2026-09-10. This plan scopes performance work under D111; it does not
change language semantics, release gates or the current opt-in JIT status.

## Current work order — 2026-09-11

The user-directed [architecture review and ordered work protocol](evidence/cp42-parity-architecture-review-2026-09-11.md)
supersedes earlier next-intervention suggestions in this historical log.
Start with `title-search-10k` and `title-search-100k` together, then
`normalize-title`, then `word-frequency`, and continue through the original
32-row manifest order. Keep one active workload family and report Native and
VM separately. Necessary shared-runtime/compiler changes are documented
dependencies of that family. Existing call-root analysis remains follow-up work.
The [string-view step](evidence/cp42-string-views-2026-09-11.md) records the accepted title-pair
implementation, exact operation counts and latest comparison. The family remains
active: title creation, residual search and source preparation still require
measurement against the current control. No per-row parity or language-level
impossibility has been established. Each candidate requires
its own targets before timing and the unchanged corpus, safety, limit, validation
and main/push evidence contracts.

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

## Rejected stage: JIT array reads and reserved scalar pushes

Starts **13:58 UTC on 2026-09-10**, due **15:58 UTC**.
The accepted `bbb9b2b` profile shows 40.8% of BFS samples under ordinary
value execution. Non-string managed-reference array reads currently exit JIT;
this stage compiles those reads using the existing checked parent view and
full reference representation. String reads and reference stores retain their
existing checked owners. No workload or source-language semantics change.

The first read-only candidate measured BFS 2135.28 to 1915.02 ms (10.3%),
BFS-record 133.00 to 120.34 ms (9.5%), and catalogue-pipeline 386.29 to
289.29 ms (25.1%). It misses the BFS target and is not accepted. Its preserved
profile still has 43.7% inclusive cold value dispatch. Before the second edit,
the candidate scope was extended to scalar pushes with proven spare capacity:
the existing runtime mutation owner executes inside the nonallocating segment;
growth and failed guards return to the original instruction. The deadline,
baseline, workload hashes and acceptance thresholds remain unchanged.

Early 3+11 measurement against `bbb9b2b` must improve BFS by at least 25%;
BFS-record and catalogue-pipeline are additional diagnostic workloads. Full
acceptance also requires at least 5% improvement in the JIT median sum and
retains the 2% median / 5% P90 limits on all 32 workloads, independently for
JIT and OFF. Exactly one confirmation per flagged workload applies the same
rule as the preceding stage. All safety, failure, sanitizer, architecture and
unchanged-input gates precede acceptance and main/push publication. A failed
target remains a recorded rejected experiment.

The [complete local result](evidence/cp42-sljit-array-segments-2026-09-10.md)
passed correctness and improved the JIT median sum by 10.3%, but DSU regressed
2.14% in the full matrix and 2.05% in its single confirmation. The candidate is
rejected. The 15:58 UTC checkpoint was also missed; final validation was recorded
at 16:06 UTC. Neither the speedup nor the later checks erase these failures.

## Locally accepted stage: preserve unchanged array identity after push

The next one-hour stage starts at the timestamp recorded in
`build/jit-array-identity/SMART.json`. It removes redundant native view-identity
checks after a successful scalar push, which cannot change the array reference.
The accepted `bbb9b2b` controls and all numerical full-matrix thresholds remain
unchanged. Early BFS must improve at least 25%; DSU must stay within 2% median
and 5% P90 before full acceptance work proceeds. No confirmation reruns the
rejected candidate. The exact unchanged OFF executable and its already completed
tests/full matrix/confirmations may be reused with explicit provenance.

The [follow-up evidence](evidence/cp42-sljit-array-identity-2026-09-10.md)
records local acceptance at 16:28 UTC: 95/95 enabled tests, sanitizer/Rosetta
replays and a full paired matrix with median sum -9.9%, BFS -32.5% and
catalogue-pipeline -26.2%. No workload exceeds either full-series threshold;
DSU is +1.77% median and -1.05% P90. No new JIT confirmation is required.
The previous OFF result is reused with exact executable and input provenance.
The earlier candidate's rejection and missed checkpoint are not rewritten.
A [new five-participant snapshot](evidence/cp42-sljit-array-identity-comprehensive-2026-09-10.md)
passes all 32 workloads with this same candidate. Equal-workload geometric mean
time ratios are native/Go 2.04x, native/Rust 2.19x, VM/Go 19.92x and VM/Rust
21.38x. These descriptive ratios include VM source preparation and are separate
from the paired acceptance gate. This stage changes JIT, not native generation.
Default JIT enablement, full Tier 1 validation and overall parity remain open.

## Active stage: prepare immutable call and frame metadata

Starts **16:51 UTC on 2026-09-10**, due **18:51 UTC**, with exact timestamps and
the accepted `719aa54` JIT/OFF executable hashes in
`build/jit-prepared-activations/SMART.json`. A fresh profile of that binary puts
call transitions at 44.9% of DSU, 49.7% of stack and 52.1% of queue sampled
stacks. Binding contributes separately; sampling is diagnostic, not timing.

Prepare direct-call metadata, initial register templates and managed-root
indexes once per selected verified function. Keep the shared activation/frame
owners authoritative for dynamic depth, storage growth, argument/return checks,
invalid-reference initialization, checked roots, source PCs and cleanup.
No workload changes or additional cached live activations are allowed.

Early 3+11 paired acceptance requires at least 20% improvement in both stack
and queue, and 15% in DSU. Full acceptance requires at least 8% improvement in
the JIT sum of medians, while retaining the independent 2% median / 5% P90
regression limits for every workload in JIT and OFF. Exactly one confirmation
per flagged workload applies the existing rule; all observations are retained.
Ordinary/prepared frame tests, actual native calls, depth/fuel and precise exits,
GC/root/layout changes, deterministic failures, full ON/OFF, sanitizer/Rosetta
and all32 correctness precede the complete timing matrix and main/push outcome.
A missed gate or deadline is documented explicitly.

The first candidate passed focused correctness and all32 replay but missed all
early targets: stack 566.72 to 482.87 ms (-14.8%), queue 592.10 to 481.48 ms
(-18.7%), DSU 1439.32 to 1358.95 ms (-5.6%). Its binary, replayable patch and
all observations are preserved. Before the second edit, `SMART-v2.json` extends
preparation with bounded function-specific view tables and inlined retained-view
revalidation before the same checked miss owner. The baseline, numeric gates
and original deadline remain unchanged; the first candidate is not accepted.

The second candidate passes early 3+11 targets: stack 583.96 to 461.14 ms
(-21.0%), queue 595.84 to 461.86 ms (-22.5%), DSU 1439.46 to 1206.49 ms
(-16.2%). The [full result](evidence/cp42-sljit-prepared-activations-2026-09-10.md)
passes correctness and reduces the JIT median sum by 8.8%, but is rejected by
the original OFF performance gate. Five OFF workloads repeat a threshold
exceedance. This is an observed-performance decision, not proof of code causality.

The user subsequently reported concurrent agent work. A single predeclared
five-workload A/A diagnostic used the exact same OFF compiler on both sides,
AC power and a background-process/thermal monitor. Median differences stay
within 1.2%, but queue P90 differs by 14.7%; the negative control is inconclusive
and its planned A/B follow-up is not run. All previous observations and the
second candidate's rejection remain preserved. Repeating until passing is not
an acceptance strategy.

Before the third edit, `SMART-v3.json` records compile-time isolation of the
prepared-JIT additions in the common frame and activation owners. The ON
compiler is byte-identical to measured candidate two. OFF retains the accepted
407 identical objects; two build-location objects become identical when compiled
with the control's generated paths. Relinking then matches the accepted binary
apart from linker UUID/signature metadata. This is explicit implementation
identity evidence, not a replacement OFF timing result. The
[follow-up evidence](evidence/cp42-sljit-prepared-isolation-2026-09-11.md)
accepts the opt-in implementation locally: current ON tests cover 95 initial
passes and one successful recheck after a recorded 978-second system sleep;
OFF passes 86/86, with new sanitizer/Rosetta tests and all32 replays. The original
deadline was missed and remains recorded. A fresh five-participant series
passes all 160 correctness outcomes; identical Native/Go/Rust/Bun binaries
run slower across this series, so cross-series absolute times are not a code
regression test. The overall Go/Rust parity goal remains unchanged.

## Measured source preparation follow-up

The [two source-phase diagnostic series](evidence/cp42-source-phase-profile-2026-09-11.md)
complete the 20-minute attribution checkpoint started at 01:38:04 UTC on
September 11. All 384 runs preserve the 32 workloads and exact outputs. They
show eight bytecode-function verifications per function and substantial repeated
MIR verification. The next bounded candidate targets redundant checks inside
program lowering/encoding and the CLI repeat after verified decoding, retaining
D049 and every public/untrusted admission check. The
[implemented candidate](evidence/cp42-program-verification-2026-09-11.md) passes
its predeclared early 7% target (8.7% observed geometric mean time reduction)
and both complete paired matrices. The JIT median sum falls 3.26%, the interpreter
sum 0.77%, with no 2% median / 5% P90 exceedance on any workload in either mode.
All 32 complete bytecode encodings remain identical to the baseline while each
function is checked five times instead of eight. Current full ON/OFF tests and
focused sanitizer/Rosetta suites plus all32 replays pass. This is local acceptance
of reduced preparation cost, not closure of the overall parity objective.

[JIT execution diagnosis](evidence/cp42-jit-execution-profile-2026-09-11.md)
attributes about 51% of DSU and 46% of stack sampled stacks to compiled call/return
transitions. Separate counters show 15 million DSU calls with almost complete
prepared-layout reuse, while stack alternates layouts. This selects a guarded
nonallocating activation-path experiment in the shared owners, with a predeclared
15% early DSU/stack latency target. These are diagnostic samples and counts, not
an additional accepted speedup; all32, safety and overall parity gates remain.

[Compiler IPO, shared calls and VM dispatch](evidence/cp42-ipo-shared-vm-2026-09-11.md)
passes the complete unchanged all32 paired gate in both modes, with geometric
mean speedups of 1.196x for JIT and 1.200x for the default interpreter.
Rejected no-IPO, combined IPO/call and partial cursor variants remain recorded.
Exact finite/shared fuel, roots, depth and allocation-failure behavior are retained;
97/97 ON and 87/87 OFF tests plus focused sanitizer/Rosetta and all32 replays pass.
Native runtime objects and Native/Go/Rust/Bun executable hashes match prior evidence.
JIT remains opt-in; Linux LSan, full Tier1 and overall Go/Rust parity remain open.

[Address-level JIT attribution](evidence/cp42-jit-address-attribution-2026-09-11.md) maps current generated-code samples back to
VM operations on DSU, BFS, stack and min-heap. Both diagnostic variants preserve
all inputs and all48 expected outcomes. Repeated checked-binding work across
managed-reference copies selects a segment-local alias analysis experiment;
C call/runtime costs and source preparation remain independent gaps. This is
diagnostic evidence, not an additional speedup or a parity claim.

## Execution tiers and allocation of effort

Keep the interpreter as the portable checked execution tier, the fallback for
operations outside JIT coverage, and an independent differential reference.
Its correctness, bounded execution and supported-platform coverage remain
required. Subsequent performance stages prioritize JIT execution, shared VM
mechanisms, source preparation and the native backend. Interpreter optimization
is justified by a measured shared cost or a regression that blocks an otherwise
useful change; it is not assigned a separate Go/Rust parity program.

The extra 10% interpreter early target in the cursor experiments was a local
experiment choice. It remains part of those recorded outcomes but is not a
standing prerequisite for future JIT work. Future stages declare their own
measurable improvement target and retain the all32 correctness and regression
checks. JIT remains opt-in under the current platform and safety gates.

## Sequence after this stage

1. Follow the current execution diagnosis: shorten the shared call/return protocol
   for retained activation storage and measure both same-layout and alternating-layout
   cases. Separately map anonymous native frames before choosing register allocation,
   view-traffic changes or bounded leaf inlining. Inlining requires exact logical-frame
   materialization on exits; retained views require explicit GC/provenance invalidation.
   These proof obligations remain in addition to the unchanged all32 acceptance gate.
2. Address remaining source preparation as its own measured stage. Retain all source-run
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

[JIT reference aliases and record-field reuse](evidence/cp42-jit-reference-values-2026-09-11.md) passes the local
unchanged all32 gate: median sum -5.49%,
geometric speedup 1.022x, with no confirmed regression.
The alias-only variant remains rejected. 99/99 ON tests, 14 focused tests in
sanitizer/Rosetta builds and all32 correctness pass. Exact OFF binary/dependency
provenance reuses previous interpreter evidence; Native/comparator binaries are
unchanged. JIT remains opt-in; full Tier1, Linux LSan and parity remain open.

[VM reachable preparation: rejected candidate](evidence/cp42-vm-reachable-preparation-rejected-2026-09-11.md) records completed
source-phase diagnosis and the unchanged-corpus experiment. A fixed recheck
with sampled foreign-compute rejection gives JIT median sum -3.41% and OFF -1.45%,
but Dijkstra OFF P90 repeats above the 5% gate (+18.22%, +15.64%). Production
remains at the f4c9961 implementation. The rejected patch, correctness evidence,
contaminated first series and all recheck observations are preserved. Full
Tier 1, Linux LSan and overall Go/Rust parity remain open.

[Text runtime diagnosis and current accepted-version snapshot](evidence/cp42-text-runtime-diagnosis-2026-09-11.md) records
reproducible string counters and a fresh five-participant all32 comparison of
the unchanged f4c9961 implementation. Both engines duplicate 1.356 GB of
concatenation copying in report-concat; direct shared-runtime string construction
is selected for the next experiment, not implemented or accepted. The record
also specifies focused diagnostic additions while preserving the fixed corpus.
Full Tier 1, Linux LSan and overall parity remain open.

[Shared string storage and direct concatenation](evidence/cp42-shared-string-storage-2026-09-11-rejected.md) is **rejected**.
It removes temporary concatenation copies and a separate payload allocation;
Unchanged-corpus measurements, allocation/GC/root tests and the exact candidate patch are
preserved. The record also exposes the existing D055 string-allocation precedence
gap. Overall parity, full Tier 1 and Linux LSan remain open.

[Immediate enum construction inside JIT segments](evidence/cp42-jit-immediate-enum-2026-09-11-rejected.md) is **rejected**.
Paired unchanged-corpus timing and exact before/after counters connect a shared
VM boundary removal to several workloads. All 32 Native executables remain
byte-identical. Full ON/OFF, focused sanitizer/Rosetta and failure/fuel checks
are recorded. Overall parity, complete Tier 1 and Linux LSan remain open.

[Immediate enums and one checked-index guard](evidence/cp42-jit-enum-bounds-2026-09-11.md) is **accepted locally**.
It combines a shared VM boundary removal with the existing array-length invariant.
The unchanged 32-workload comparison, revised user-directed tradeoff policy,
101/87 full tests, sanitizer/Rosetta checks and frame-work counters are retained.
Native executable hashes remain identical. Overall parity and full Tier 1 remain open.

[Retained string bytes in Native, interpreter and JIT](evidence/cp42-string-views-2026-09-11.md) is **accepted locally**.
It removes redundant byte validation while preserving original32 inputs, logical
comparison/allocation counts and exact fuel. Full103/88 tests,20/20 sanitizer and
Rosetta checks, all32 Native/ON/OFF paired timing and a fresh five-way comparison
are recorded. Three rejected candidates remain archived. Title-search stays
active; all-row parity, known baseline defects and full Tier1 remain open.

[Title costs after retained string bytes](evidence/cp42-title-after-string-views-2026-09-11.md) records completed local
phase diagnosis on `78d3070`: shared string creation/storage is the next common
mechanism, subject to D055 preflight/lifetime obligations and a fresh protocol.
About 40 ms of VM source preparation remains a separate dependency. No new
optimization, row closure or parity result is claimed.

[Shared string allocation and budget preflight: rejected candidate](evidence/cp42-string-publication-2026-09-11-rejected.md) is **rejected**.
Title-search improves, but the unchanged all32 gate contains a confirmed Native
record-create regression. Full104/89 tests,21/21 sanitizer/Rosetta checks, exact
allocation counters and every Native/ON/OFF timing remain archived. Production
is restored; D055, title-search parity and complete Tier1 remain open.
