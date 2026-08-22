# Checkpoint 28: integrated release rebaseline

Date: 2026-08-22

Scope: checkpoints 24-27 integration, native fast-path cleanup, exact reusable
algorithm validation, and an honest release rebaseline. No Ruda source
semantics or benchmark-specific opcode is introduced.

## Integrated implementation

D060 dense storage, D061 exact/geometric capacity, D062 reconstructed integer
proofs, and D063 activation views converge in one generated-C/runtime path.
D063 additionally keeps small root-location tables in fixed activation storage
and elides root linkage only after a conservative transitive no-safepoint
proof. Deterministic allocation-injection builds retain the allocating root
path and failure order. Unknown/interface calls, allocation-capable array and
nominal construction, checked numeric Option construction, string results,
output, and suspension-capable paths require roots.

The final tests also cover two correctness defects found during integration:
an aliasing call that replaces a cached record array field now fails the
identity guard and rebinds, and checked numeric Option/conversion instructions
are classified as allocation safepoints before root elision.

## Performance-first validation

The user's original 11-sample/3-warmup task report is the direct local
comparison requested for this work. At capture its untracked sibling JSON had
SHA-256
`fbd9115a265e2781b7344110eef4507cf262f316f478a1e2b9421635e7d94979`.
That sibling path was subsequently rewritten by another local run, so the
captured task values in the tables below, rather than the current contents of
that untracked path, are the baseline record. A pre-final D063 artifact already
produced the following exact-checksum medians:

| Workload | Original Ruda ms | Pre-final Ruda ms | Reduction | Go gap | Rust gap |
|---|---:|---:|---:|---:|---:|
| insertion | 2960.677 | 301.685 | 89.81% | 10.28x | 10.60x |
| heap-sort | 734.837 | 130.133 | 82.29% | 3.29x | 5.88x |
| stack | 356.716 | 270.312 | 24.22% | 18.24x | 29.59x |
| queue | 383.140 | 267.573 | 30.16% | 17.77x | 28.79x |
| min-heap | 1518.153 | 399.281 | 73.70% | 7.37x | 13.39x |
| dsu | 1617.682 | 477.331 | 70.49% | 19.41x | 21.76x |
| bfs | 2218.480 | 995.676 | 55.12% | 15.74x | 41.29x |
| dijkstra | 687.358 | 187.891 | 72.66% | 4.54x | 4.76x |

That report is `cp28-release-local.json`, SHA-256
`4b8c4d5ff55682a3caf9836e66e347322cd55e20796fe244983b6d22494b3055`.
It is retained as an intermediate measurement, not final acceptance evidence,
because the broader C-unit run subsequently exposed the numeric-Option
safepoint defect described above. The corrected artifact is rebuilt and
remeasured below rather than silently substituting the earlier numbers.

## Remaining limitation

The performance change is material but not parity. Stack, queue, and min-heap
still construct a managed `Option` enum for hot operations; those allocations
and resulting collection dominate their profiles. BFS retains high nested-loop
and managed-iteration overhead. Closing those gaps requires a separately
reviewed value-enum/escape or managed-allocation decision, not a hidden
checkpoint-28 semantic change. This evidence therefore reports exact ratios
and does not describe Ruda as close to Go/Rust on the complete corpus.

## Final validation

The corrected, identity-guard-narrowed release artifact is:

- compiler: `/workspace/Projects/ai-projects/Ma/build/release/ruda`,
  SHA-256
  `a5441d8f1485e2e4800f45913e583afa6afede3592aa35d28fdfdd079db45c91`;
- release runtime:
  `/workspace/Projects/ai-projects/Ma/build/release/native-runtime/release/libruda_native_runtime_release.a`,
  SHA-256
  `c2e9d3cc0b6f66e67f4a80066126a493ba92751f32ed5b1a747af4acbaa9052a`;
- native compiler/linker: `/usr/bin/cc`, AppleClang 17, strict C17 O2 with
  `-fno-fast-math`, `-frounding-math`, and contraction disabled.

The final algorithm report is `cp28-release-final2-local.json`, SHA-256
`aba10364be7e7814ec6c1715e1efac8e6527310f5f1108b002b362d6b858414b`.
All eight exact checksums passed.

| Workload | Original Ruda ms | Final Ruda ms | Speedup | Reduction | Final / Go | Final / Rust |
|---|---:|---:|---:|---:|---:|---:|
| insertion | 2960.677 | 299.832 | 9.87x | 89.87% | 10.38x | 10.65x |
| heap-sort | 734.837 | 129.408 | 5.68x | 82.39% | 3.32x | 5.96x |
| stack | 356.716 | 267.911 | 1.33x | 24.90% | 18.28x | 30.04x |
| queue | 383.140 | 264.925 | 1.45x | 30.85% | 17.78x | 29.41x |
| min-heap | 1518.153 | 400.782 | 3.79x | 73.60% | 7.52x | 13.66x |
| dsu | 1617.682 | 481.448 | 3.36x | 70.24% | 19.57x | 22.07x |
| bfs | 2218.480 | 985.326 | 2.25x | 55.59% | 15.65x | 41.47x |
| dijkstra | 687.358 | 186.845 | 3.68x | 72.82% | 4.60x | 4.80x |

The all-probe report is `cp28-probes-final-local.json`, SHA-256
`7eb70baa5b72bc6afd352283567aa0a5f9bb22e1b2f3de9f337852d5497ee2ea`.

| Probe | Median ms | MAD ms | IQR ms |
|---|---:|---:|---:|
| scalar-get | 29.160 | 0.143 | 0.330 |
| scalar-set | 11.075 | 0.123 | 0.323 |
| proven-bounds | 49.844 | 0.158 | 0.495 |
| unproven-bounds | 76.255 | 0.269 | 0.620 |
| growth | 82.329 | 0.299 | 0.770 |
| reserved-growth | 78.888 | 0.372 | 0.966 |
| density | 119.476 | 0.307 | 1.825 |
| nested-traversal | 62.733 | 0.422 | 0.886 |
| checked-loop | 68.357 | 0.354 | 1.593 |

`validate_results.py` accepted all 9 probes and all 8 algorithms against the
reviewed macOS arm64 checkpoint-21 budgets. The final post-narrowing tree also
passed the complete local validation tail:

| Validation | Result |
|---|---|
| architecture, C-memory policy, and generated-artifact gates | passed |
| doc links, spec terms, example syntax, and syntax coverage | passed; 75 syntax kinds and 116 grammar productions covered |
| `git diff --check` | passed |
| debug C unit + install relocation | 2/2 passed, 244.30 s |
| release C unit + install relocation | 2/2 passed, 251.14 s |
| ASan C unit + install relocation | 2/2 passed, 440.56 s |
| ASan+UBSan C unit + install relocation | 2/2 passed, 441.79 s |
| sibling algorithm equivalence and harness | passed; 8 Ruda workloads, 45 harness tests, and Go/Rust reference suites |
| macOS x86_64 Debug C unit + install relocation under Rosetta | 2/2 passed, 1210.97 s; both compiler and C-unit are Mach-O x86_64 |

The full debug run supersedes the earlier focused 261.08-second observation;
all four final presets use the same post-narrowing production sources.

The additional x86_64 compiler and release-runtime archive have SHA-256
`5deb1252873c2b795be238211df0c267145caa29808f853158453deafb40e942`
and `a61d6ce6d359c6d8066bfc4b43ad4ab380c2b75fefd1b644912615219953b6d5`
respectively. Both runtime archives are non-fat x86_64 archives. This is
correctness/ABI evidence, not a performance result: the reviewed 11/3
rebaseline remains macOS arm64 only. Linux arm64/x86_64 and Windows x86_64,
plus a macOS x86_64 performance report, remain required by the distinct
public-alpha registry.
