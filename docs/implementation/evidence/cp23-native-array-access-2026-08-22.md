# Checkpoint 23: measured native array-access boundary

Date: 2026-08-22
Scope: internal generated-C/runtime ABI and measured native optimization; no
Ruda source, MIR, bytecode-wire, array-representation, capacity, or observable
semantic change.

## Decision and delivered boundary

Accepted D059 selects one versioned portable-C17 exact-access runtime ABI and
rejects LTO as the permanent boundary. Generated C now calls
`ruda_managed_array_access_{length,require,get,set}` with the exact immutable
array descriptor. Each access resolves the handle once, checks that descriptor,
observes the current length, performs either the signed checked comparison or
the D058 verified in-bounds operation, and returns without retaining a slot,
array, cell pointer, length, or capacity.

An immediately adjacent matching `ARRAY_REQUIRE_INDEX` and in-bounds consumer
are emitted as one checked access call carrying the original G4002 span. A
separated barrier remains an explicit call at its original evaluation point.
An `ARRAY_ASSUME_INDEX` and later verified consumers use the in-bounds mode.
The array reference stays in the function's precise native shadow-root frame;
calls, allocation, collection, growth, and initialized-sequence mutation keep
the D058 invalidation rules.

The previous generated `ruda_g_array_check`,
`ruda_managed_array_{get,set}_in_bounds`, and their production/test source owner
are removed. `array_access.h` is a 53-line public owner API and
`managed_array_access.c` is a 207-line independently compiled runtime owner.

## Surviving work

Generated-C inspection plus focused owner tests record the following boundary
operations. “Resolve” means a managed slot/generation/provenance lookup, not a
source-level operation.

| Operation | CP22 generated/runtime chain | D059 retained chain |
|---|---:|---:|
| exact length | describe resolve + length resolve | one resolve + descriptor + length |
| checked get after barrier | three resolves, repeated descriptor/length/bounds/status chain | one checked exact-access call and one resolve |
| checked set after barrier | at least three resolves, including a second mutable resolution on the legacy set path | one checked exact-access call and one mutable resolve |
| proven get/set | one in-bounds resolve per access | one exact-descriptor in-bounds resolve per access |

`test_native_c_emitter_array_access.c` asserts one get and one set call in each
direct/checked-proof/static-proof fixture. In the fused checked-proof fixture it
also asserts zero `access_require` calls, exactly one checked mode (get), one
in-bounds mode (the later set), and absence of every former helper spelling.
Generated programs execute with strict C17 at both `-O0` and `-O2`.

## Runtime and differential coverage

- raw and reference cells, exact descriptors, wrong descriptors, negative and
  upper-bound indices, `INT64_MAX`, aliases, initialized-sequence mutation, and
  observed lengths;
- stale collected handles, precise roots across collection and growth, wrong
  reference element type, and alias-visible writes;
- an armed fail allocator observes no access allocation attempt, followed by
  heap teardown with `live_allocations == 0` and `live_bytes == 0`;
- the existing D058 hostile-bytecode, call/mutation invalidation, G4002 span,
  checked/optimized MIR, encoded VM, and native O0/O2 suites now execute through
  the exact API; VM range dispatch also removes its separate pre-access
  descriptor resolve;
- debug, release, and ASan/UBSan C-unit suites compile and run the same raw,
  reference, alias, mutation, safepoint, panic, and output owners.

## Prototype comparison

The retained exact ABI was measured with the D056 11-sample/3-warmup protocol
on macOS arm64 Apple M2 using AppleClang 17 and the release runtime archive.
The raw schema-valid report is
`../ruda-algotithms/benchmarks/results/cp23-probes-macos-arm64-apple-m2.json`.
It is preserved in the benchmark repository by commit `e74bd93`.
The CP22 report is the comparison point because it already includes the D058
range-proof pass.

| Probe | CP22 median ms | D059 exact ABI ms | Change | CP21 reviewed ceiling headroom |
|---|---:|---:|---:|---:|
| scalar-get | 96.810 | 79.210 | -18.18% | 25.73% |
| scalar-set | 89.483 | 84.423 | -5.65% | 39.63% |
| proven-bounds | 207.477 | 195.138 | -5.95% | 32.85% |
| unproven-bounds | 255.857 | 219.362 | -14.26% | 22.84% |
| growth | 86.737 | 87.774 | +1.20% | 10.17% |
| reserved-growth | 85.358 | 85.272 | -0.10% | 11.57% |
| density | 347.864 | 331.230 | -4.78% | 29.71% |
| nested-traversal | 169.628 | 168.893 | -0.43% | 27.53% |
| checked-loop | 306.639 | 309.733 | +1.01% | 9.39% |

The report passes its schema/protocol/source/checksum validation, and no probe
exceeds its reviewed CP21 workload ceiling. The checkpoint-21 paired validator
intentionally pins one compiler revision and therefore cannot pair this new
compiler artifact with the old algorithm report; no false paired-baseline pass
is claimed.

For the competing prototype, the same emitted scalar-get/set C was compiled
with all runtime translation units under AppleClang `-O2 -flto`. A balanced
11-sample order produced:

| Prototype | scalar-get median ms | scalar-set median ms | get/set binary bytes | link wall time |
|---|---:|---:|---:|---:|
| selected exact archive ABI | 80.037 | 84.091 | 96,144 / 96,304 | ordinary `ruda build` path |
| whole-program LTO | 22.299 | 23.577 | 70,088 / 70,520 | 0.89 s each locally |

The 11 raw LTO samples were get
`21.678, 22.029, 22.127, 22.079, 22.299, 23.038, 22.425, 22.514, 22.166, 22.673, 22.558`
ms and set
`23.862, 24.045, 23.727, 23.577, 23.535, 23.417, 23.845, 23.491, 23.833, 23.476, 23.560`
ms. The speed and size benefit is real on this compiler, but it is not a stable
runtime ABI: archive LTO objects and linker plugins are tied to compiler family,
version, target, and distribution. Clang, GCC, and MSVC do not share one
relocatable LTO archive contract, while D050/D052 require one deterministic
distribution-selected C17 compiler path and a version/profile-matched runtime
artifact. Selecting LTO would therefore leave host/profile-dependent permanent
paths or require shipping compiler-specific runtime IR. D059 keeps the measured
portable exact ABI, records LTO as a later whole-program build-mode opportunity,
and removes the prototype rather than retaining two access implementations.

## Validation record

The final local tail passed against the complete checkpoint tree on macOS
arm64 with AppleClang 17:

```text
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-generated
./tools/check-doc-links
./tools/check-spec-terms
cmake --build --preset debug --parallel 4
ctest --preset debug --output-on-failure
cmake --build --preset release --parallel 4
ctest --preset release --output-on-failure
cmake --build --preset asan-ubsan --parallel 4
ctest --preset asan-ubsan --output-on-failure
```

| Validation | Result |
|---|---|
| architecture, C-memory policy, generated artifacts, doc links, spec terms | all passed |
| example syntax and syntax coverage | passed; 75/75 syntax kinds and 116/116 grammar productions |
| debug C unit | passed, 217.64 s |
| debug install relocation | passed, 2.44 s |
| release C unit + install relocation | 2/2 passed, 231.82 s |
| ASan+UBSan C unit + install relocation | 2/2 passed, 345.71 s |

The focused D056 report command also completed with 11 samples and 3 warmups,
and its schema plus every reviewed local workload ceiling was validated. The
separate D057 public-alpha registry still requires post-optimization reports on
all five Tier 1 targets; this local checkpoint evidence does not widen that
claim.
