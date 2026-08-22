# Checkpoint 28: post-integration release hardening

Date: 2026-08-23

Scope: internal native-C/runtime hardening after the accepted checkpoint-28
integration. This change keeps the D060-D063 language and VM/native contracts;
it adds no source construct, benchmark-only opcode, or unchecked observable
behavior.

## Correctness fixes found by the release regression

The complete native differential suite exposed three defects while the final
algorithm fast paths were being integrated:

- array mutation type validation wrote a mutation generation into the MIR
  instruction target even when that target was the managed array reference;
- a proven index incorrectly implied that a local generated array view was
  already bound, allowing a null object dereference;
- managed parameter initialization was nested under root-frame creation, so a
  rootless leaf taking a record could dereference an uninitialized record view.

The emitter now treats a proven bound and a proven bound view as distinct
facts, emits managed parameter initialization independently from root-frame
ownership, and elides mutation descriptor lookup only when the exact array
view or unchanged parameter proves the type. Existing nominal O0/O2
differential tests exercise the rootless record case, and the checkpoint-17
and checkpoint-20 corpora cover array mutation plus all eight reusable
algorithms under VM/native execution.

## Managed child-cache ownership

Nested generated traversal uses an array-owned child-object cache keyed by
the array mutation generation. Cache resize is failure-atomic, participates in
the managed heap byte budget, is invalidated by mutation, and is released with
the owning array. Focused allocator injection covers the reachable cache
allocation failure and the successful refresh path. Full teardown asserts
both `live_allocations == 0` and `live_bytes == 0`.

## Final comparative algorithm run

The final arm64 release compiler has SHA-256
`76db64a271c03b1afccc059bbdb83067030aea7bffa88493d825e6bd6093c0f9`.
Its release runtime archive has SHA-256
`5cc8e4146dc060b95c623c5dc2c3ed1c903e4e74fbe2bf3b08eb50e3db2b65fb`.
The compiler/linker is AppleClang 17 on Apple M2. The temporary cross-language
report used 11 measured samples after 3 warmups, preserved the exact checksum
contract for every run, and has SHA-256
`c65279c30b92dee60159fe9d63c85d29f1170abdd23b0c48c4683c7bc419637a`.
The benchmark repository itself was not modified.

| Workload | Ruda ms | Go ms | Rust ms | Ruda / Go | Ruda / Rust |
|---|---:|---:|---:|---:|---:|
| insertion | 50.25 | 29.01 | 28.43 | 1.7x | 1.8x |
| heap-sort | 32.07 | 38.69 | 21.83 | 0.8x | 1.5x |
| stack | 12.16 | 14.48 | 8.75 | 0.8x | 1.4x |
| queue | 12.54 | 15.03 | 8.80 | 0.8x | 1.4x |
| min-heap | 52.77 | 54.57 | 30.41 | 1.0x | 1.7x |
| dsu | 31.29 | 25.04 | 21.91 | 1.2x | 1.4x |
| bfs | 52.95 | 62.25 | 24.07 | 0.9x | 2.2x |
| dijkstra | 41.06 | 41.12 | 39.39 | 1.0x | 1.0x |

These are local comparative results, not a replacement for the Tier-1 runner
registry. They show Go parity or better on five workloads, a 1.2x gap on DSU,
a 1.7x gap on insertion, and a larger remaining Rust gap on BFS.

## Release validation

| Validation | Result |
|---|---|
| architecture and C-memory policy | passed |
| generated artifacts, doc links, spec terms, examples, syntax coverage | passed; 75 syntax kinds and 116 grammar productions covered |
| `git diff --check` | passed |
| debug C unit + install relocation | 2/2 passed, 252.61 s |
| release C unit + install relocation | 2/2 passed, 253.90 s |
| ASan+UBSan C unit + install relocation | 2/2 passed, 452.29 s |
| Ruda/Go/Rust algorithm checksums | all eight matched in every measured run |

Apple's local ASan run used `detect_leaks=0` because LeakSanitizer is not
available on this macOS configuration. This does not weaken the explicit
allocator counters and failure-index coverage above. Linux LeakSanitizer and
TSan remain CI contracts; this local evidence does not claim to replace their
runner results.
