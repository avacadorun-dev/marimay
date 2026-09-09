# VM execution regions and inactive frame reuse

Local Apple M2 / macOS arm64 investigation, 2026-09-09; parent `8d659cc`.
[Raw matrices, instruction counts, probes and validation logs](cp42-vm-regions-2026-09-09.json).
This is D104/D105 implementation evidence under execution spec §9.2. The
performance-parity objective, CP42, CLI0 and other Tier 1 evidence remain open.

## Shared runtime changes

The program VM now executes supported scalar/control/raw64-array operations
inside one nonallocating region. A bounded local cache retains validated array
views until the next cold instruction. Descriptor creation, calls, allocation,
GC, I/O and failures stay in the ordinary dispatcher. No source rewrite, new
wire opcode, benchmark-specific kernel or native compiler substitution is used.
Every successful instruction consumes exactly its original applicable fuel;
failed speculation leaves the instruction, registers, array and fuel unchanged
for the ordinary error path. Unsupported operations and layouts retain that
path. The single-function entry remains an ordinary-dispatch test reference.

After this change, a DSU CPU sample attributed about 40% of sampled stacks to
frame initialization, root preparation and disposal. The separate activation
owner now keeps one inactive function layout per call depth for that invocation.
Returning unlinks its roots after publishing the result; reactivation resets
all old managed references before checked root registration. Inactive storage
does not keep objects alive. Different callees replace the storage, and final
teardown includes inactive and partially initialized slots. Linked root frames
have stable separately allocated addresses across stack-vector relocation.
This changes host allocation frequency, not managed-heap accounting or source
values, defer order, panic identity, recursion limits or collection safety.

## Measurement

All 32 unchanged standard-size workloads pass in a three-sample diagnostic
matrix, without warmups; every checksum and input/artifact integrity check
passes. An initial candidate matrix and the final system-release matrix are
both retained. The final run measures `/opt/homebrew/bin/mmay` after rebuilding
its `build/release/mmay` target. The prior full-size VM baseline is from the
catalogue investigation; `8d659cc` changes native emission only, so its VM
execution code is the same baseline. This is local diagnostic evidence, not an
11-sample performance acceptance or a claim of Go/Rust parity.

| Workload | Before, ms | System release, ms | Before/after |
|---|---:|---:|---:|
| insertion | 21,202.58 | 7,765.96 | 2.73× |
| heap-sort | 5,989.24 | 1,909.57 | 3.14× |
| dsu | 12,278.35 | 5,780.01 | 2.12× |
| bfs | 12,940.75 | 6,256.77 | 2.07× |
| dijkstra | 10,480.94 | 9,409.11 | 1.11× |
| array-grow | 359.02 | 241.33 | 1.49× |
| bfs-record | 531.18 | 278.02 | 1.91× |
| catalog-pipeline | 760.54 | 611.02 | 1.24× |

Summed median VM round: 84.04 → 47.21 seconds. Source and harness hashes match the baseline exactly.

System release SHA-256:
`79f3702a41d8ccd83ad91e5b30bac978cb299dcc6c437047412ef6e7a45b0866`.

VM process times include source loading, checking and lowering. Native numbers
in the previous investigation exclude compilation; this report does not turn
those unequal boundaries into VM/Go/Rust acceptance ratios. Host isolation is
local-unverified, and before/after matrices are separate runs. Small differences
are not evidence of significant improvements or regressions. Summed medians
describe the approximate cost of one VM round, not a language-wide score.

## Instruction demand and remaining route

A separate instrumented executable counts original opcodes and uses D087 shared
fuel as an independent total. Counts agree exactly and all original checksums
pass. Its sources, commands and complete disassemblies are retained in the raw
evidence. These runs can overlap validation and are **excluded from timings**;
the production executable has no counters or diagnostic output.

| Fixed workload | Executed instructions | Constants/copies | Jumps/branches | Direct calls |
|---|---:|---:|---:|---:|
| insertion | 1,800,629,785 | 34.78% | 17.41% | 251 |
| DSU | 570,544,256 | 19.49% | 17.72% | 15,002,065 |
| Dijkstra | 989,567,569 | 22.31% | 44.63% | 1,833 |

These are instruction frequencies, **not CPU percentages**. The first hot-loop
insertion sample spends 2,531/2,534 samples in the region itself: repeated
descriptor work has been removed, but dispatch and the generated instruction
stream remain expensive. Dijkstra's disassembly materializes short-circuit
conditions through temporary booleans, copies and chains of jumps. Its bool
array also uses the ordinary typed-storage path. DSU additionally requires
prepared direct-call and nominal-layout access, beyond recycling allocations.

The next shared boundary is executable-code preparation: simplify typed MIR
control flow and scalar temporaries, prepare call/layout metadata, and retain
the exact checked runtime path for unsupported cases. Measure instruction
reduction separately from dispatch throughput and source preparation. Neither
opcode frequencies nor this speedup predicts native throughput. Any later
instruction fusion or compilation tier needs its own semantics, fuel,
safepoint, failure and platform evidence; it is not silently authorized as an
unchecked shortcut by this implementation. Native text/Option/Result and
Map/Set costs from the previous report remain part of the parity objective.

## Validation

All **86 Release CTest cases pass** in one complete run. The focused bytecode,
CP31 allocation shards 0–7 and string-lifetime suite pass under ASan/UBSan
(10/10); after adding the frame-owner regression, bytecode passes again under
ASan/UBSan. Generated C in the shared engine fixture runs at O0 and O2 and
inherits the sanitizer flags. Linux LeakSanitizer remains a CI obligation.

The hot-region fixture compares ordinary and optimized execution at exact
finite/shared fuel boundaries, arithmetic extrema, out-of-bounds accesses and
mutation-generation saturation. It compares result publication, array content,
generation, status, trap and panic spans. Source fixtures cover call/GC
boundaries, aliases, growth, rebinding, closure allocation and unsupported
layouts through MIR, encoded/decoded VM and native execution with forced GC.

The frame-owner fixture proves that relocation preserves roots, inactive values
are collected, every stale managed slot is cleared, repeated same-layout setup
performs no allocation attempts, and changed-layout/partial/inactive storage is
released. All applicable deterministic failure sweeps require zero live
allocations **and** zero live bytes after full teardown. These local checks do
not imply an unqualified absence of all memory defects.

Architecture, central C memory policy, generated artifacts and roadmap gates
are required and their output is retained with the evidence. No new book idea
was used for D104/D105; the implementation follows measured execution and the
existing D055/D063/D087 contracts. Earlier Vladimirov RPO attribution remains
printed pages 145–147 / PDF pages 146–148 in the prior research journal.
