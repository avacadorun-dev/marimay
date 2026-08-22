# Algorithm benchmark findings for Marimay 0.1-dev

Status: non-normative evidence snapshot; not a language decision and not a
checkpoint plan.
Snapshot date: 2026-08-18.

This note records what the sibling `mmay-algotithms` experiment demonstrated,
what remains only a hypothesis, and which implementation areas are plausible
optimization targets. A separate agent can turn accepted work into atomic
checkpoints; this document intentionally does not assign checkpoint numbers or
claim that an optimization is complete.

## Reproduction and scope

The evidence repository is expected next to this repository:

```text
ai-projects/
  Ma/
  mmay-algotithms/
```

Run the current comparison with:

```sh
cd ../mmay-algotithms
make benchmark
```

The harness builds ahead of measurement and then executes the resulting native
programs directly:

- Marimay: `build/release/mmay build --release`;
- Go: `go build`;
- Rust: `cargo build --release`.

Compilation is excluded. `mmay run` and the VM are not mixed into this table.
Each workload uses the same deterministic LCG and expected checksum in all
three languages. The measured interval includes process startup, input
construction, the algorithm, checksum calculation, and one output line.

The full current artifacts are in the sibling paths
`../mmay-algotithms/benchmarks/results/latest.md` and
`../mmay-algotithms/benchmarks/results/latest.json` when addressed from the
repository root.

## Current native baseline

Host: Apple M2, macOS arm64. Each value is the median of three runs after one
warm-up run.

| Workload | Marimay | Go | Rust | Marimay / Go | Marimay / Rust |
|---|---:|---:|---:|---:|---:|
| insertion | 3279.58 ms | 29.75 ms | 28.80 ms | 110.2× | 113.9× |
| heap sort | 1060.12 ms | 39.75 ms | 22.27 ms | 26.7× | 47.6× |
| stack | 99.43 ms | 15.04 ms | 9.24 ms | 6.6× | 10.8× |
| queue | 107.29 ms | 15.25 ms | 9.54 ms | 7.0× | 11.2× |
| min-heap | 1308.07 ms | 55.38 ms | 31.18 ms | 23.6× | 41.9× |
| DSU | 1053.08 ms | 24.97 ms | 21.92 ms | 42.2× | 48.0× |
| BFS | 2595.97 ms | 63.67 ms | 24.47 ms | 40.8× | 106.1× |
| Dijkstra `O(V²)` | 885.19 ms | 41.91 ms | 39.38 ms | 21.1× | 22.5× |

These values are local evidence about the current Stage 0 implementation. They
are not stable performance claims about the language.

## Release-build trap discovered by the experiment

The first measurement invoked `build/debug/mmay build --release`. Generated C
was compiled with `-O2`, but the compiler binary embeds the path to the native
runtime archive produced by its own CMake configuration. The resulting program
therefore linked `build/debug/libmmay_native_runtime.a`, whose objects were not
optimized.

That run reported, for example, 17.9 s for insertion, 4.48 s for heap sort,
9.98 s for BFS, and 3.95 s for Dijkstra. Switching only to
`build/release/mmay` reduced those medians to 3.28 s, 1.06 s, 2.60 s, and
0.89 s respectively. The old numbers are retained as configuration evidence in
the sibling path
`../mmay-algotithms/benchmarks/results/debug-runtime-baseline.md`, not as a
language comparison.

Confirmed consequence: today, `mmay build --release` does not by itself ensure
that every linked first-party runtime object is a release object. Output
performance depends on how the `mmay` compiler executable was built. A future
toolchain should make the requested output profile authoritative or reject a
mismatched runtime explicitly.

## Comparability limits

Matching checksums prove matching selected results, not identical generated
instruction streams or perfectly equivalent abstraction boundaries.

- Go and Rust benchmark binaries call the reusable packages. Current Marimay
  cross-package lowering gaps require preview-safe inline implementations in
  the benchmark package.
- The first Marimay DSU benchmark omitted path compression while Go and Rust used
  it. The current source now performs path compression before the baseline
  above was recorded.
- Marimay stack, queue, and min-heap workloads operate on their inline array state;
  Go and Rust go through library types. Optimizing Go/Rust compilers may inline
  those calls, but this has not been proven from emitted machine code.
- Marimay Dijkstra currently uses parallel target/weight arrays while Go and Rust
  use edge records. Both implement the same `O(V²)` search and consume the same
  graph, but their memory layouts differ.
- Some reusable Marimay graph paths perform invalid-input checks that the
  preview-safe benchmark can omit for generated valid inputs. That difference
  favors Marimay slightly rather than explaining its slowdown.
- Results cover one machine, one operating system, one toolchain snapshot, and
  short local samples. They are suitable for finding large regressions, not for
  distinguishing small percentage changes.

Before turning a workload ratio into a performance gate, the Marimay benchmark
should call the same reusable package surface and all three implementations
should be inspected for equivalent algorithmic work.

## Confirmed implementation observations

### AB-PERF-01 — Scalar array access has a large generic runtime path

In generated release C, one `values[index]` read currently expands to work that
includes:

1. resolving and describing the managed object;
2. checking the exact array descriptor;
3. reading its length;
4. checking the signed index;
5. resolving the object again in `mmay_managed_array_get` or `_set`;
6. mapping managed status to the public runtime status;
7. converting between `MarimayManagedCell` payload bits and the exact integer.

This sequence occurs for individual hot-loop accesses in sorting, heaps, DSU,
and graph traversal. The safe bounds and type contract must remain observable,
but repeated descriptor, resolution, and length work is not necessarily
observable.

Evidence locations:

- [`bootstrap/src/native/c_emitter_emit.c`](../../bootstrap/src/native/c_emitter_emit.c);
- [`runtime/common/managed.c`](../../runtime/common/managed.c);
- generated `program.c` under the benchmark's ignored `.mmay/` build state.

### AB-PERF-02 — Scalar cells use reference-sized union storage

`MarimayManagedCell` is a union of a 64-bit raw payload and
`MarimayManagedReference`. On the measured 64-bit target the reference is 16 bytes,
so every `Array<int>` element occupies a 16-byte cell even though `int` needs 8
bytes. Go `int` and the benchmark's Rust `i64` use 8-byte elements on this
target.

The collector already distinguishes raw and reference cells through
`MarimayManagedArrayDescriptor.cell_kind`. That makes a denser raw-scalar storage
path plausible without changing tracing semantics, but its implementation and
VM/native equivalence still require proof.

Evidence: [`runtime/include/mmay/runtime/core.h`](../../runtime/include/mmay/runtime/core.h).

### AB-PERF-03 — The five-million-element failure is a heap-budget boundary

The managed heap default is 64 MiB of live data. Array capacity grows by
doubling. With 16-byte cells, growth toward five million elements requests a
capacity of 8,388,608 cells, already 128 MiB before other managed objects. The
native runtime therefore returns `I0002: native runtime resource limit reached`.

One million elements succeeds because the next power-of-two capacity is
1,048,576 cells, roughly 16 MiB of cell storage.

This is not the language-level `int::MAX` array-length limit. It is the current
runtime resource policy and representation interacting with geometric growth.

Evidence: [`runtime/common/managed.c`](../../runtime/common/managed.c).

### AB-PERF-04 — Host `-O2` cannot remove all runtime abstraction cost

Release generated C is compiled with `-O2`, but the managed runtime is linked as
a separate static archive. Without an interprocedural optimization path, the C
compiler cannot generally inline and simplify array resolution through that
archive. The remaining 6.6×–113.9× ratios after using the release runtime show
that release flags alone are insufficient.

Evidence: [`bootstrap/src/native/build.c`](../../bootstrap/src/native/build.c).

## Optimization hypotheses and validation ideas

The following are candidate directions, not confirmed causes in isolation.

### 1. Make output profile independent of compiler build profile

`mmay build --release` should select an optimized runtime artifact regardless
of whether the invoking compiler was built for debugging. Options include
shipping explicit runtime artifacts per output profile, rebuilding the runtime
as part of the toolchain build contract, or rejecting a profile mismatch. A
test should prove which archive and flags reached the final executable.

### 2. Add MIR range analysis and bounds-check elimination

Loops such as `for let i = 0; i < length(values); i += 1` provide enough facts
to eliminate or hoist many repeated length and bounds checks. The optimizer must
preserve left-to-right evaluation, mutation invalidation, exact panic identity,
and the source span of any check that remains.

Insertion sort is the sharpest signal: its `O(n²)` inner loop multiplies the
cost of every array access and currently shows the largest slowdown.

### 3. Specialize native storage for raw scalar arrays

An `Array<int>` fast path could use contiguous `int64_t` storage rather than a
16-byte `MarimayManagedCell` per element. Reference arrays would retain traced
reference storage. This could improve cache density and permit larger scalar
arrays under the same heap budget.

The source-level `Array<T>` identity, non-moving guarantee, safe indexing,
failure atomicity, and VM/native observable behavior must remain unchanged.

### 4. Avoid repeated handle and descriptor resolution

For an exact statically typed array, native lowering can potentially resolve
the managed handle and descriptor once for a proven-safe region. Arrays are
non-moving, but growth, collection, aliasing, and root lifetime constrain how
long a raw internal pointer may remain valid. This should be an internal
backend optimization, not an unsafe source feature.

### 5. Enable cross-translation-unit optimization deliberately

Measure link-time optimization or a narrow internal inline fast-path API before
adopting it. Whole-runtime header exposure would violate the repository's owner
boundaries; any fast path needs a small reviewed interface. Generated C size,
build latency, binary size, and strict floating-point flags remain gates.

### 6. Add non-observable capacity planning

Known-size construction should allocate exact initial storage where possible.
A standard `reserve(array, count)` operation could reduce reallocations without
making capacity observable; it should return a typed growth error consistently
with `push`. Benchmark inputs then need both reserved and ordinary-growth
variants so the compiler is not judged only on allocator behavior.

### 7. Separate heap policy from representation limits

The native executable needs a documented, testable way to receive an isolate or
heap budget suitable for its deployment. Increasing a default alone would hide
the 16-byte-cell and growth issues. Report representation density, requested
capacity, live bytes, and configured budget separately when producing `I0002`.

### 8. Optimize nested-array traversal after scalar access

BFS and Dijkstra amplify handle resolution because `adjacency[node][edge]`
touches an outer and inner managed array. First optimize and measure the scalar
array primitive; then consider caching inner arrays, typed record arrays, or a
separate CSR benchmark. Replacing the benchmark graph with CSR would be an
algorithm/data-layout comparison, not proof that ordinary nested arrays are
fast.

### 9. Prove away checked integer work where ranges are known

Marimay must trap on overflow in release builds. MIR range facts can still remove
checks whose safety is proven, such as bounded loop increments or index
arithmetic under a dominating length condition. This should be measured after
array access costs are isolated.

### 10. Establish focused performance probes before setting budgets

Add independent native probes for scalar loops, function calls, `Array<int>`
construction, `push`, `length`, get, set, nested get, record field access, and
checked arithmetic. Record emitted-C size, allocation count, peak live bytes,
and runtime call counts alongside wall time. Repeat on every Tier 1 platform
with enough samples for a stable median and dispersion.

## Suggested order of investigation

1. Make `--release` select a coherent release runtime and add evidence of the
   selected artifact.
2. Add microbenchmarks and profiling counters so runtime calls and allocations
   are attributable.
3. Hoist/eliminate redundant array descriptor, length, and bounds work.
4. Prototype dense raw-scalar array storage and compare memory plus time.
5. Evaluate a narrow inlining/LTO strategy across the runtime archive boundary.
6. Add capacity planning and configurable documented heap budgets.
7. Repair cross-package execution so the benchmark invokes the reusable Marimay
   packages, then define regression budgets from equivalent implementations.

This ordering is a research dependency graph, not an atomic checkpoint plan.
