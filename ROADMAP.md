# Roadmap

Start with the [current product dashboard](docs/implementation/roadmap.md#current)
and [Now / Next / Later](docs/implementation/roadmap.md#now-next-later).

The [product roadmap](docs/implementation/roadmap.md) is the only authority for
gate states and release sequencing. The
[checkpoint ledger](docs/implementation/checkpoints.md) is the only execution
queue, and the [evidence index](docs/implementation/evidence/README.md) keeps
direct reports out of status cells. Their IDs and states are checked against
[`roadmap.tsv`](docs/implementation/roadmap.tsv).

Current execution links: [CLI0](docs/implementation/roadmap.md#cli0-boundary),
[CP41/I6/E0 sequencing](docs/implementation/roadmap.md#cli0-cp41-boundary-and-post-cli0-i6e0), and the
active ergonomics card,
[CP42](docs/implementation/checkpoints.md#cp42-notes-driven-cli-ergonomics).

The user accepted the [five-cycle CLI0 execution route](docs/implementation/checkpoints.md#accepted-cli0-execution-route)
on 2026-09-07. It groups shared API and application work while preserving
checkpoint verification order and all release obligations.
Its required pre-alpha acceptance includes the
[ten-application external CLI portfolio](docs/implementation/cli-ergonomics-suite.md).

For continuation context, see the dated
[CLI-alpha agent handoff](docs/implementation/agent-handoff-cli-alpha-2026-09-07.md).
It is a snapshot and reading guide, not a replacement for the authorities above.

Latest local runtime evidence: [managed VM string lifetimes and the catalogue
regression matrix](docs/implementation/evidence/cp42-vm-string-lifetime-2026-09-09.md).
This fixes the report-concat defect; release-gate states remain unchanged.

The [full catalogue performance investigation](docs/implementation/evidence/cp42-catalog-performance-2026-09-09.md)
records all 32 standard workloads, the native object-queue regression and open
shared text/collection/VM execution costs. These findings remain part of the
pre-alpha performance work; no existing acceptance budget is waived.

[VM execution regions and frame reuse](docs/implementation/evidence/cp42-vm-regions-2026-09-09.md)
remove repeated view validation and activation allocation from supported paths.
All 32 full-size VM diagnostics and 86 Release tests pass locally. Instruction
counts expose the next shared code-preparation boundary; performance parity,
startup budgets and Tier 1 acceptance remain open.

[Scalar/control preparation](docs/implementation/evidence/cp42-scalar-flow-2026-09-09.md)
records D106, all 32 unchanged workloads on the system build, the native bulk-fill
composition regression and its repair, and allocation/rollback checks. Native
text/collection costs and the large VM gap remain open; parity is not achieved.

[Scalar array VM regions](docs/implementation/evidence/cp42-vm-scalar-arrays-2026-09-09.md)
cover every scalar width and reduce paired Dijkstra from 6.63 to 2.41 seconds.
All 32 VM workloads and 86 Release tests pass locally. System `mmay` uses an
independent installed copy; catalogue runs pin their tools. Record/reference
access, call preparation and native text/collection costs remain open.

[Verified VM execution images](docs/implementation/evidence/cp42-vm-execution-image-2026-09-09.md)
records the measured D107 rejection, D108 direct call/root preparation and
generic composed loops. Paired insertion improves 6.28× and DSU 1.42× with
unchanged checksums; the stable compact runner also removes measured control
regressions. The interpreter ceiling still leaves both far from Bun. Further
trace families and all parity/Tier 1 gates remain open.

[Verified VM trace batching](docs/implementation/evidence/cp42-vm-trace-batching-2026-09-09.md)
records D109 and a passing 32-workload VM replay. Complete nonallocating
array-shift iterations retain scalar/storage state and preserve exact finite
fuel and side exits. Paired insertion improves another 4.18×, from 980.44 to
234.36 ms, and is about 26.2× faster than the original 6.14-second result. It
is still 2.67× the existing Bun median. Additional call/control trace families,
source startup, the native parity wave and all parity/Tier 1 gates remain open.

[VM source preparation](docs/implementation/evidence/cp42-vm-preparation-2026-09-09.md)
indexes D062 predecessor edges while preserving every charged work boundary
and proof result. Paired 3+11 runs remove about 9 ms from short source runs;
all 32 workloads across five participants pass the full 3+11 matrix. Release
86/86 and focused ASan/UBSan 12/12 pass locally. Native and VM performance
parity, execution-loop costs and Tier 1 acceptance remain open.

The [VM execution cost map](docs/research/vm-execution-cost-map-2026-09-09.md)
records 4.442 billion original instructions across the unchanged 32 workloads.
It identifies shared execution, call and runtime costs. A handwritten JIT is
one candidate, not a conclusion established by opcode counts; its ARM64
prototype is parked outside production after portability and correctness
review. Diagnostic coverage is not a new speedup or parity acceptance.

[Portable VM frame capacity reuse](docs/implementation/evidence/cp42-vm-frame-capacity-2026-09-09.md)
extends D105 under D110 so alternating verified callees reuse inactive buffer
capacity while rebuilding active types and GC root locations. The change is
independent of CPU architecture. Exact limits, managed-heap accounting and
source semantics remain unchanged; local timing and ownership evidence does
not close the remaining execution-loop, native runtime or Tier 1 parity gates.

[SLJIT feasibility with the reused ARM64 prototype](docs/implementation/evidence/cp42-sljit-feasibility-2026-09-10.md)
records the user-selected JIT route, a common C backend, 32 unchanged checksum
passes, the bytecode suite and a paired six-workload 3+11 series. The isolated
adapter in that first snapshot improves five heavy cells but regresses insertion.
The [owned-memory and composed-loop follow-up](docs/implementation/evidence/cp42-sljit-owned-memory-2026-09-10.md)
adds central allocation, one-way W^X publication, deterministic failure teardown
and D109 composition, with 32/32 correctness on ARM64, x86-64 under Rosetta
and ARM64 ASan/UBSan. Its paired 896-observation full matrix reduces the sum
of VM medians by 46.1%, with insertion at parity and no median regression above
2%. That snapshot leaves production integration, complete JIT conformance,
Linux LSan, Tier 1 acceptance and Go/Rust parity open.

[JIT fuel selected from invocation options](docs/implementation/evidence/cp42-sljit-fuel-mode-2026-09-10.md)
retains exact metering for bounded/shared invocations and omits unused segment
accounting for D087 local runs. All 32 correctness cases pass on ARM64, Rosetta
x86-64 and ARM64 ASan/UBSan. A separate paired six-workload series improves
Dijkstra by 15.3%; it does not replace the full-matrix evidence above or close
production, platform or parity gates at that snapshot.

[Optional shared JIT integrated into CMake](docs/implementation/evidence/cp42-sljit-integration-2026-09-10.md)
adds the D111 production owners and pinned dependency preparation behind an
explicit OFF-by-default option. The final local run passes 90/90 tests with JIT
and 86/86 without it; ARM64, Rosetta x86-64 and ARM64 ASan/UBSan each replay all
32 frozen workloads. Exact-type admission fixes the prototype's narrow-integer
arithmetic defect. A fresh full paired series reduces the sum of VM medians by
46.5%, with no median regression above 2%. Linux JIT/LSan CI jobs are configured;
their results, full Tier 1 acceptance, default enablement and Go/Rust parity
remain open. The next structural JIT boundary is ordinary calls and logical
activation state; native text/runtime work remains independently necessary.

The [working SMART performance plan](docs/implementation/vm-performance-plan.md)
defines a per-workload parity proposal, the bounded native-call checkpoint,
unchanged measurement rules and the subsequent runtime/preparation stages.
It does not set an unsupported final parity date or close a product gate.

The [native-call checkpoint](docs/implementation/evidence/cp42-sljit-calls-2026-09-10.md)
passed local correctness (92/92 JIT, 86/86 interpreter, sanitizer and Rosetta
replays) but failed its performance acceptance: frequency-map regressed in both
full matrices and the single planned confirmations. The implementation remains
a reproducible candidate patch; these results do not merge it into production
or close the parity gate.

The [frame-cursor follow-up](docs/implementation/evidence/cp42-sljit-cursor-2026-09-10.md)
passes local correctness and the nominal median confirmation rule but remains
held because BFS repeatedly develops a slow execution mode. Its replayable
patch and all observations are preserved. Post-measurement profiles locate
about half of BFS and DSU samples in JIT object binding, defining the next
systemic experiment; they do not establish a new speedup or close parity.

[Checked object-view reuse across JIT entries](docs/implementation/evidence/cp42-sljit-views-2026-09-10.md)
revalidates retained array/record metadata through exact identities and a
runtime-owned GC epoch while preserving the full checked miss path. It carries
forward the shared logical call owner and cursor. Local checks pass 93/93 with
JIT, 86/86 without it, plus sanitizer and Rosetta workload replays. The paired
JIT matrix reduces the sum of medians by 23.2%; the default interpreter changes
by -0.05%. One flagged catalogue P90 increase is not reproduced in its single
predeclared confirmation; no regression is confirmed. A new full five-participant
matrix records current native, Go, Rust, VM/JIT and Bun results. The opt-in
implementation is accepted locally; default enablement, Linux LSan, full Tier 1
and native/VM parity remain open.

[Nonallocating JIT array segments with retained identity](docs/implementation/evidence/cp42-sljit-array-identity-2026-09-10.md)
add complete non-string reference reads and scalar pushes into existing spare
capacity through the runtime mutation owner. The [preceding candidate](docs/implementation/evidence/cp42-sljit-array-segments-2026-09-10.md)
was rejected after DSU exceeded the 2% median threshold in both prescribed
series. The follow-up removes redundant identity comparisons after successful
pushes and passes new 95/95 enabled tests, sanitizer/Rosetta replays and the
complete paired JIT matrix: median sum -9.9%, BFS -32.5%, catalogue-pipeline
-26.2%. Every workload stays within the 2% median / 5% P90 thresholds; DSU is
+1.77% median and -1.05% P90. The identical OFF executable's previous 86-test
result, full matrix and successful confirmations are explicitly reused.
A [fresh five-participant matrix](docs/implementation/evidence/cp42-sljit-array-identity-comprehensive-2026-09-10.md)
passes all 32 unchanged workloads with this candidate executable. Descriptive
geometric mean time ratios are native/Go 2.04x, native/Rust 2.19x, VM/Go 19.92x
and VM/Rust 21.38x; VM includes source preparation.
This is local opt-in acceptance; default enablement, full Tier 1 and parity
remain open. The earlier checkpoint overrun remains recorded.

[Prepared JIT activations with OFF isolation](docs/implementation/evidence/cp42-sljit-prepared-isolation-2026-09-11.md)
prepare immutable call/frame metadata and bounded view tables. The enabled
compiler is byte-identical to the measured candidate: the full JIT median sum
falls 8.8%, with stack/queue improvements of 20.5%/21.5%. The preceding candidate
remains rejected under its observed OFF gate; an identical-binary A/A diagnostic
then exceeded the P90 threshold, leaving code causality unresolved. The follow-up
proves canonical OFF implementation identity, explicitly accounting for build
paths and linker UUID/signature metadata. Current validation covers all 96
enabled tests (95 initial passes and one sleep-timeout recheck), 86/86 OFF,
sanitizers and x86 Rosetta, including the unchanged 32-workload replays.
The [fresh five-participant matrix](docs/implementation/evidence/cp42-sljit-prepared-isolation-comprehensive-2026-09-11.md)
passes all 160 correctness outcomes. Descriptive geometric mean ratios are
native/Go 2.12x, native/Rust 2.22x, VM/Go 19.98x and VM/Rust 20.99x.
These local-unverified timings do not establish a cross-series code regression
or overall parity. The missed deadline is recorded; JIT remains OFF by default,
and Linux LSan, full Tier 1 and overall Go/Rust parity remain open.

[Source-run phase attribution](docs/implementation/evidence/cp42-source-phase-profile-2026-09-11.md) records
two 32-workload diagnostic series with unchanged inputs and exact compiler
identity. Repeated MIR and bytecode-function verification dominates preparation
on short cases; it selects a bounded verification-deduplication candidate.
These are diagnostic measurements, not a new accepted optimization or parity.

[Canonical program verification](docs/implementation/evidence/cp42-program-verification-2026-09-11.md)
removes three redundant function-verifier passes from source preparation while
retaining D049, standalone API checks, complete program verification and VM
admission. All 32 encoded programs match the control byte-for-byte. Full paired
JIT/interpreter median sums fall 3.26%/0.77%, with geometric mean speedups of
1.055x/1.032x; no workload exceeds the predeclared 2% median / 5% P90 thresholds.
Current validation passes 96/96 JIT tests, 86/86 interpreter tests, and 12 focused
suites plus all32 workloads under both ASan/UBSan and x86 Rosetta. This is local
implementation acceptance; default JIT enablement, Linux LSan, full Tier 1 and
overall Go/Rust parity remain open.

[JIT execution diagnosis](docs/implementation/evidence/cp42-jit-execution-profile-2026-09-11.md)
attributes about 51% of DSU and 46% of stack sampled stacks to compiled call/return
transitions. Separate counters show 15 million DSU calls with almost complete
prepared-layout reuse, while stack alternates layouts. This selects a guarded
nonallocating activation-path experiment in the shared owners, with a predeclared
15% early DSU/stack latency target. These are diagnostic samples and counts, not
an additional accepted speedup; all32, safety and overall parity gates remain.

[Compiler IPO, shared calls and VM dispatch](docs/implementation/evidence/cp42-ipo-shared-vm-2026-09-11.md)
passes the complete unchanged all32 paired gate in both modes, with geometric
mean speedups of 1.196x for JIT and 1.200x for the default interpreter.
Rejected no-IPO, combined IPO/call and partial cursor variants remain recorded.
Exact finite/shared fuel, roots, depth and allocation-failure behavior are retained;
97/97 ON and 87/87 OFF tests plus focused sanitizer/Rosetta and all32 replays pass.
Native runtime objects and Native/Go/Rust/Bun executable hashes match prior evidence.
JIT remains opt-in; Linux LSan, full Tier1 and overall Go/Rust parity remain open.

[Address-level JIT attribution](docs/implementation/evidence/cp42-jit-address-attribution-2026-09-11.md) maps current generated-code samples back to
VM operations on DSU, BFS, stack and min-heap. Both diagnostic variants preserve
all inputs and all48 expected outcomes. Repeated checked-binding work across
managed-reference copies selects a segment-local alias analysis experiment;
C call/runtime costs and source preparation remain independent gaps. This is
diagnostic evidence, not an additional speedup or a parity claim.

Do not replace evidence-based states with unchecked task-list boxes or treat a
locally implemented slice as publicly shippable.

[JIT reference aliases and record-field reuse](docs/implementation/evidence/cp42-jit-reference-values-2026-09-11.md) passes the local
unchanged all32 gate: median sum -5.49%,
geometric speedup 1.022x, with no confirmed regression.
The alias-only variant remains rejected. 99/99 ON tests, 14 focused tests in
sanitizer/Rosetta builds and all32 correctness pass. Exact OFF binary/dependency
provenance reuses previous interpreter evidence; Native/comparator binaries are
unchanged. JIT remains opt-in; full Tier1, Linux LSan and parity remain open.

[VM reachable preparation: rejected candidate](docs/implementation/evidence/cp42-vm-reachable-preparation-rejected-2026-09-11.md) records completed
source-phase diagnosis and the unchanged-corpus experiment. A fixed recheck
with sampled foreign-compute rejection gives JIT median sum -3.41% and OFF -1.45%,
but Dijkstra OFF P90 repeats above the 5% gate (+18.22%, +15.64%). Production
remains at the f4c9961 implementation. The rejected patch, correctness evidence,
contaminated first series and all recheck observations are preserved. Full
Tier 1, Linux LSan and overall Go/Rust parity remain open.

[Text runtime diagnosis and current accepted-version snapshot](docs/implementation/evidence/cp42-text-runtime-diagnosis-2026-09-11.md) records
reproducible string counters and a fresh five-participant all32 comparison of
the unchanged f4c9961 implementation. Both engines duplicate 1.356 GB of
concatenation copying in report-concat; direct shared-runtime string construction
is selected for the next experiment, not implemented or accepted. The record
also specifies focused diagnostic additions while preserving the fixed corpus.
Full Tier 1, Linux LSan and overall parity remain open.

[Shared string storage and direct concatenation](docs/implementation/evidence/cp42-shared-string-storage-2026-09-11-rejected.md) is **rejected**.
It removes temporary concatenation copies and a separate payload allocation;
Unchanged-corpus measurements, allocation/GC/root tests and the exact candidate patch are
preserved. The record also exposes the existing D055 string-allocation precedence
gap. Overall parity, full Tier 1 and Linux LSan remain open.

[Immediate enum construction inside JIT segments](docs/implementation/evidence/cp42-jit-immediate-enum-2026-09-11-rejected.md) is **rejected**.
Paired unchanged-corpus timing and exact before/after counters connect a shared
VM boundary removal to several workloads. All 32 Native executables remain
byte-identical. Full ON/OFF, focused sanitizer/Rosetta and failure/fuel checks
are recorded. Overall parity, complete Tier 1 and Linux LSan remain open.

[Immediate enums and one checked-index guard](docs/implementation/evidence/cp42-jit-enum-bounds-2026-09-11.md) is **accepted locally**.
It combines a shared VM boundary removal with the existing array-length invariant.
The unchanged 32-workload comparison, revised user-directed tradeoff policy,
101/87 full tests, sanitizer/Rosetta checks and frame-work counters are retained.
Native executable hashes remain identical. Overall parity and full Tier 1 remain open.

[Parity architecture review and ordered workload work](docs/implementation/evidence/cp42-parity-architecture-review-2026-09-11.md) records the user-directed text-first queue against accepted `51e0be3`. It distinguishes measured implementation costs from unproven language limits, preserves Native/VM boundaries and all32 completion requirements, and makes no new speedup claim.

[Retained string bytes in Native, interpreter and JIT](docs/implementation/evidence/cp42-string-views-2026-09-11.md) is **accepted locally**.
It removes redundant byte validation while preserving original32 inputs, logical
comparison/allocation counts and exact fuel. Full103/88 tests,20/20 sanitizer and
Rosetta checks, all32 Native/ON/OFF paired timing and a fresh five-way comparison
are recorded. Three rejected candidates remain archived. Title-search stays
active; all-row parity, known baseline defects and full Tier1 remain open.

[Title costs after retained string bytes](docs/implementation/evidence/cp42-title-after-string-views-2026-09-11.md) records completed local
phase diagnosis on `78d3070`: shared string creation/storage is the next common
mechanism, subject to D055 preflight/lifetime obligations and a fresh protocol.
About 40 ms of VM source preparation remains a separate dependency. No new
optimization, row closure or parity result is claimed.

[Shared string allocation and budget preflight: rejected candidate](docs/implementation/evidence/cp42-string-publication-2026-09-11-rejected.md) is **rejected**.
Title-search improves, but the unchanged all32 gate contains a confirmed Native
record-create regression. Full104/89 tests,21/21 sanitizer/Rosetta checks, exact
allocation counters and every Native/ON/OFF timing remain archived. Production
is restored; D055, title-search parity and complete Tier1 remain open.

[String-publication regression diagnosis](docs/implementation/evidence/cp42-string-publication-regressions-2026-09-12.md) records two preserved local
diagnostics. Record registration volume and a controlled wrapper replacement
support revising the publication path; OFF Dijkstra's tail consumes extra CPU
inside otherwise unchanged hot-array owners. The rejected candidate stays
rejected. Cache/path counters are the next bounded investigation; production,
title-family closure and full Tier1 status are unchanged.

[Shared strings, publication and recent array views](docs/implementation/evidence/cp42-string-publication-v2-2026-09-12.md) is **accepted locally**.
It records coallocated string storage with D055 preflight, shared publication
internals and D104 previous-entry-first lookup, full 104/89 tests, 21/21 sanitizer
and Rosetta checks, exact allocation/cache counters and all 32 paired timings.
The original title family, all-32-workload parity and full Tier1 remain open.

[Title costs after string publication](docs/implementation/evidence/cp42-title-after-publication-2026-09-12.md) records local repeated-entry
profiles with per-return zero allocator ledgers and a refuted Native alias-cache
hypothesis. String-array projection and checked JIT publication remain measured
costs to investigate. Production and all32/Tier1 parity status are unchanged.

[Native title string-projection screening](docs/implementation/evidence/cp42-title-projection-probe-2026-09-12.md) passes its predeclared
diagnostic thresholds with unchanged counts and zero post-return allocator
ledgers. It motivates a general descriptor-selected implementation with full
GC and checked identity support; no production optimization or parity status
is accepted by this two-row experiment.

[String-array projection: rejected early](docs/implementation/evidence/cp42-string-array-projection-2026-09-12-rejected.md) preserves the general
same-stride implementation and its fixed title-pair results. Native improves
15.28% geometrically, but VM+JIT improves only 0.33%, missing its 3% minimum.
Focused 7/6 tests and all32 Native/ON/OFF correctness pass; full validation and
full32 timing were not run. Production is restored. Checked JIT publication
is the next bounded investigation; accepted all32/Tier1 status is unchanged.

[String-array projection V2: rejected early](docs/implementation/evidence/cp42-string-array-projection-v2-2026-09-12-rejected.md) records a separate checked
machine-code projection owner and actual native/C oracle coverage. Focused 8/6
checks and all32 correctness pass; Native improves 14.47% geometrically, while
VM+JIT shows no useful gain and misses its 3% minimum. Production is restored.
Repeated invariant binding stores are the next hypothesis; no full32/Tier1
acceptance or parity claim follows this rejected fixed series.

[String-array projection V3: rejected early](docs/implementation/evidence/cp42-string-array-projection-v3-2026-09-12-rejected.md) removes over 98% of
invariant binding-header publications with unchanged compiled string-read counts,
but VM+JIT elapsed time does not improve. Native also misses its 10k row minimum.
Focused 8/6 and all32 correctness pass; the exact candidate, failed setup logs and
fixed series remain archived. Production is restored. Map anonymous JIT frames
before another instruction change; original all32/Tier1 parity remains open.

[Mapped accepted title JIT instructions](docs/implementation/evidence/cp42-title-jit-map-2026-09-12.md) completes the bounded diagnosis.
All 434 anonymous leaf observations map across captured code lifetimes; 512 original
returns and 256 zero-ledger returns pass. The next candidate targets repeated view
checks within a segment and immediate boolean reloads, preserving all VM stores
and exact exits. Production, the accepted all32 comparison and parity/Tier1 status
remain unchanged.

[Local JIT view/boolean candidate](docs/implementation/evidence/cp42-jit-local-views-2026-09-12-rejected.md) is **rejected early**: the original
VM title pair improves 2.10%, below its fixed 3% minimum. Focused 19/5 checks and
all32 correctness pass; Native executables are byte-identical to the accepted
control. The exact candidate and fixed observations remain archived; production
is restored. The next hypothesis concerns invalidation of a copied alias's own
view. Original all32 parity and complete Tier1 remain open.

[Alias-cache JIT V2](docs/implementation/evidence/cp42-jit-local-views-v2-2026-09-12-rejected.md) is **rejected early**: the unchanged VM title
pair improves 2.995623%, below its fixed 3% minimum (exact ratio 0.970043769715).
Focused 20/5 checks and all32 correctness pass; Native executables are identical
to accepted controls. The candidate and failed fixed series remain archived;
production is restored. Next investigate full checked original-Cell string
projection in the JIT. Original all32 parity and complete Tier1 remain open.

[Inline original-Cell string projection](docs/implementation/evidence/cp42-jit-cell-projection-2026-09-12-rejected.md) is **rejected early**:
the unchanged VM title pair improves 2.353783%, below its fixed 3% minimum.
Focused 21/5 tests, 108 native/C-oracle cases and all32 stdout checks pass;
Native executables are identical to accepted controls. The exact candidate and
series are archived and production restored. Next measure stable array identity,
generation and heap-epoch reuse before considering speculative whole-array
child validation. Original all32 parity and complete Tier1 remain open.

[Stable string-array reuse diagnosis](docs/implementation/evidence/cp42-string-array-reuse-2026-09-12.md) records 128 unchanged original
results, four-way counter agreement per workload and 64 zero-ledger returns.
An independent source oracle confirms repeated reads of the same title array.
Waiting for twice its length in prior reads avoids one-pass text arrays in the
work-count model. No speedup or new child certificate is accepted; production,
the accepted all32 comparison and complete Tier1/parity status remain unchanged.

[Adaptive string-array proof: rejected early](docs/implementation/evidence/cp42-jit-string-array-proof-2026-09-12-rejected.md) preserves the full checked
certificate candidate and fixed 3.55% title-pair slowdown. Focused 18/5 tests,
all 32 outputs and Native byte identity pass. Post-rejection 128-run counters
confirm predicted reuse and 64 zero-ledger returns; the C helper still performs
almost as many hot-path instructions. Production is restored. Next investigate
certified projection inside the JIT segment under fresh fixed gates; accepted
all 32/Tier1 and original parity status remain unchanged.

[Inline certified string-array projection: rejected early](docs/implementation/evidence/cp42-jit-string-array-proof-v2-2026-09-12-rejected.md) archives the
fixed 1.45% title-pair slowdown and restores production. Focused 18/5 tests,
all 32 outputs and Native byte identity pass. Direct 128-run counters and 64
zero-ledger returns prove 7,894,270 title100k reads bypass C; emitted disassembly
shows complete destination publication remains. Next inspect repeated invariant
metadata writes under the existing entry-epoch proof from accepted production.
The accepted full comparison, original parity and Tier1 status remain unchanged.

[Checked string metadata reuse: rejected early](docs/implementation/evidence/cp42-jit-string-metadata-2026-09-12-rejected.md) records the fixed 0.62%
title-pair reduction, below the 3% minimum, and restores production. Focused 18/5,
all 32 outputs and Native byte identity pass after a recorded C17 fixture repair.
128 diagnostic runs and 64 zero ledgers prove metadata reuse on 98.76% of title100k
reads. Next test proof-based emitted projection and entry metadata reuse together
under fresh gates, against accepted production. Full parity/Tier1 remain open.

[Combined certified reads and metadata reuse: rejected early](docs/implementation/evidence/cp42-jit-string-array-proof-v3-2026-09-12-rejected.md) archives the
fixed 0.65% title-pair slowdown and restores production. First-attempt focused
19/5 tests, all 32 outputs and Native byte identity pass. Direct 128-run counters,
64 zero-ledger returns and exact emitted capture prove both mechanisms execute
without the required elapsed-time improvement. Stop isolated bridge variants;
attribute the whole original search loop and source/JIT startup before another
candidate. Generic checked loop composition is a hypothesis, not acceptance.
The accepted full comparison, original parity and Tier1 status remain unchanged.
