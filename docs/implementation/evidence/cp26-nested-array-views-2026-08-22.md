# Checkpoint 26: nested managed-array traversal

Date: 2026-08-22

Scope: reusable nested array/record traversal in generated native C. No graph
opcode, source flattening, slice semantics, moving storage, or VM wire change.

## Measured cause and retained path

The faithful BFS/Dijkstra and nested probes showed that dense scalar storage
alone did not remove repeated handle resolution, outer record projection,
descriptor checks, and iterator-generation calls. Accepted D063 therefore
binds exact non-escaping array and record views for one generated activation.
An array view keeps addresses of the non-moving object's length, generation,
and storage-pointer fields, never a copied storage address. Growth remains
visible through the field. Record array fields are reused only while the
current field reference still equals the cached view reference; a replacement
through an aliasing call falls back to a fresh exact bind.

Every array register has a companion view. Exact copies preserve it and all
other reference definitions clear it. Parameter record fields may bind eagerly
after exact descriptor validation. Checked accesses retain the original G4002
span; D058 proven accesses still require verifier authority. Iterator
generation remains non-wrapping and preserves G4005.

## Coverage

Runtime tests cover 2D/3D raw and reference arrays, empty/ragged values,
aliases, rooted collection, growth, outer replacement, wrong descriptors,
stale handles, exact generation, failure atomicity, and zero allocator
counters. Generated-C tests compile and execute nested record-to-array access
at O0 and O2, including `read -> mutating call -> replacement read`, and
inspect companion resets/direct RAW64 access. The checkpoint-20 corpus retains
exact BFS and Dijkstra checksums through VM/native/both paths.

## Performance

The final checkpoint-28 11/3 report is the aggregate performance authority for
this path. The original user-observed release report measured BFS at
2218.480 ms and Dijkstra at 687.358 ms. The pre-final fast-path artifact
measured 995.676 ms and 187.891 ms respectively, before a broader regression
test exposed and forced correction of an unrelated numeric-Option safepoint
classification. The final corrected artifact measured BFS at 985.326 ms and
Dijkstra at 186.845 ms. Its focused nested-traversal probe measured 62.733 ms
median with 0.422 ms MAD and 0.886 ms IQR; checkpoint 28 records the exact
artifact and report hashes.

This local macOS arm64 evidence does not claim the unrun five-platform gate.
