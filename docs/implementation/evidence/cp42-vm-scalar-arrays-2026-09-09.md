# VM regions support all scalar array storage widths

Local Apple M2 / macOS arm64 evidence, 2026-09-09; parent `57f6ec0`.
[Raw matrices, paired runs, checks and installed toolchain](cp42-vm-scalar-arrays-2026-09-09.json).
This implements the existing D060/D063/D104 contracts. Parity and all product
and platform gates remain open.

## Cause and change

A nonallocating VM region previously handled only RAW64 array get/set.
A bool or other narrow array access therefore returned to ordinary dispatch
and discarded all locally validated array views. Dijkstra repeatedly reads
its bool visitation array between integer array operations. The cost included
revalidation of the surrounding arrays, not just transport of one bool.

`array_scalar_view.h` now owns RAW8/RAW16/RAW32/RAW64 typed host-order copies,
complete-bit validation before narrowing stores, bounds/capability consumption
and generation commit order. `vm_hot.c` translates scalar VM values, including
canonical bool values. Floating-point bit patterns are preserved. The existing
RAW64 path remains available; reference layouts still use ordinary dispatch.
Nothing allocates or reaches a safepoint inside this region. Failure returns
at the original instruction without value/store/fuel publication, leaving the
ordinary dispatcher to construct the same observable error.

## Measurement

The paired Dijkstra experiment alternates old/new order, uses one warmup per
version and retains all five subsequent measurements. Both versions use the
same unchanged full-size source, heap limit, expected stdout and VM source-run
boundary. The old binary is a pinned copy of the preceding system version;
the candidate and its runtime/headers are independently copied too.

| Dijkstra VM | Before | After | Before/after |
|---|---:|---:|---:|
| Paired median | 6,633.84 ms | 2,405.49 ms | 2.76× |
| Separate full-matrix diagnostic median | 6,755.03 ms | 2,382.35 ms | 2.84× |

The complete unchanged 32-workload VM matrix passes at standard sizes and a
64 MiB managed heap, with three diagnostic samples and no warmups. Its summed
medians change from 40.95 to 35.58 seconds. The paired experiment supports the
Dijkstra gain; other small inter-run differences are not attributed to this
change. Insertion remains 5.90 s, DSU 5.37 s and BFS 5.48 s: a large gap remains.
There is no native parity claim or new native timing comparison in this slice.

Workload sources, parameters, expected outputs, manifests and measurement
protocol match the prior matrix. The harness changed to isolate its tools;
that change is explicit in the raw evidence rather than described as identical
harness bytes. Compiles, tests and sampling were excluded from timed runs.
VM timing still includes source loading/checking/lowering. Host isolation is
local-unverified.

## Correctness and installation

- Full Release CTest: **86/86 passed**, 264.80 seconds.
- Focused runtime-view and VM scalar-array owners pass Release and ASan/UBSan.
  They compare ordinary/program dispatch for all scalar types, checked/proven
  access, every instruction/shared fuel boundary, bounds errors, generation
  saturation, signed extrema, bool directions, negative zero and NaN payloads.
  Direct region tests assert actual fast-path coverage and zero allocations.
- Runtime view checks include rejected truncation and unchanged generation,
  malformed widths, null/reference views, rooted growth and collection.
- A source fixture checks narrow/bool mutation, aliases, growth, rebinding,
  calls and closure-triggered GC through prepared MIR, wire/VM, native O0/O2,
  forced GC and deterministic allocation-failure sweeps. Both allocator live
  counters are zero after teardown. No allocation-capable production API is
  added by the scalar view helpers.
- Architecture, C memory policy, generated-artifact and roadmap checks pass.
  Local sanitizer success is not Linux LeakSanitizer or all Tier 1 evidence.

The measured release binary SHA-256 is
`15e6a5077e6b18bfc56908563f19cb08461b12244bf8ebd69561a197e8dd45b4`.
It was installed without recompiling into
`~/.local/share/marimay/toolchains/15e6a5077e6b18bf`; system
`/opt/homebrew/bin/mmay` was atomically redirected to its `bin/mmay`.
The old `build/release/mmay` was left intact for already resolved consumers.
`mmay doctor --json` reports installed layout and matching debug/release
runtimes. A default-PATH catalogue smoke run passes 21 × 4 × 3 checks and
records exactly the measured binary hash. Rebuilds use separate candidate
build directories; neither installed prefix nor active benchmark copies are
build outputs to overwrite.

## Next boundary

A separate current DSU sample, excluded from timings, records 2351 samples.
It still shows repeated record projection returning to cold dispatch, followed
by array descriptor comparison/view rebinding, and direct-call symbol lookup.
The next shared work is record/reference access and prepared call/layout data;
all reference identity, layout, roots, failure order and fuel must remain exact.
This source/VM investigation used the existing accepted runtime contracts and
measured profiles. No additional idea or page from Vladimirov's book is claimed;
D106's actual book applications remain in the reading journal (printed 71–74,
PDF 72–75).
