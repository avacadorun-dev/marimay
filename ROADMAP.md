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

Do not replace evidence-based states with unchecked task-list boxes or treat a
locally implemented slice as publicly shippable.
