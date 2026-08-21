# Post-preview checkpoint 21: array performance baseline — 2026-08-22

Gate: pre-optimization measurement and regression policy. Checkpoint status:
incomplete. Measurement and evidence are complete on two reviewed Tier 1
runners, macOS arm64 and Linux x86_64; the remaining runner baselines are not
claimed by this record.
I2, I3, I4, and I5 remain `in-progress`; I6 and P0 remain `not-started`. This
checkpoint changes no Ruda source semantics, MIR, VM, native runtime, or
managed-array representation and performs no optimization.

The macOS baseline uses Ruda commit
`d6afbe792685ef12fe1ce9e6e053ad9758ddce8d`; the Linux baseline uses Ruda
commit `ae039188fa22b05a119a4dc5464f2a7f91d5deed`. The latter changes only this
checkpoint's documentation, not the measured compiler/runtime implementation.
Sibling protocol/probe commit: `57ad442f62ee644c82bd48243913c55cf0d27dac`.
The macOS raw-evidence commit is `b326784`; the Actions-produced Linux raw
evidence is `88d03f4df3b21d3f34256e76253e650488daaae9`, reviewed and registered by
`ed4a220aa1b88c2eb30b494bf33470a73762a12c`. These commits are published on
`origin/main` at
`https://code.example/benchmark-corpus.git`.

Authority: accepted D056 plus the implementation test-strategy performance and
Tier 1 platform contracts. D052 supplies release-runtime artifact identity;
D055 supplies the fixed managed-heap byte budget. The post-preview checkpoint
plan supplies sequencing and scope but does not override those authorities.

## Accepted measurement contract

- `ruda.performance-protocol/1` fixes a monotonic `perf_counter_ns` clock,
  three warmups, eleven odd-count measured rounds, a 300-second per-process
  timeout, no valid-sample selection, and deterministic balanced rotation of
  workload and language order. Compilation occurs before timing.
- A timed process must produce the exact `<workload> <checksum>\n` stdout,
  empty stderr, exit zero, and no timeout. Every raw sample and execution-order
  event is retained.
- Metadata includes platform/CPU/load/power, full command, fixed 67,108,864-byte
  managed-heap budget, corpus/manifest/protocol revisions and hashes, Ruda
  compiler path/hash/version, release runtime path/hash/manifest/profile, and
  generated-C compiler/linker paths, versions, and complete flags.
- Reports publish median, MAD, Tukey-hinge Q1/Q3/IQR, min, max, and noise
  fraction. Noise is `max(2%, 3*MAD/median, IQR/median)`; a baseline above 30%
  noise is rejected.
- Reviewed Ruda budgets are platform/workload specific and use
  `median * (1 + max(10%, noise))`. Go/Rust values remain comparative evidence,
  never Ruda release thresholds. One complete cool-down retry adjudicates a
  candidate regression; both original and retry medians must exceed the same
  threshold, and neither raw distribution is discarded.

Accepted D056 records these rules before checkpoints 22–27 change measured
implementation paths. The protocol SHA-256 is
`0b43886b4e02ee72e8ea4be8eca6c3fb228336bcf525522723a4b40c3b862b19`.

## Focused probes and schema coverage

The sibling `ruda.array-probes-manifest/1` owns nine independent native
entrypoints and has SHA-256
`25bcb922a294a316784e8cf382c239fc62e9b7cbe7d88618c3f61a326c4bd6ab`.
The exact probe source-tree hash is
`571d2aa80904e20b90c7b893c32c7e9e63a8b0c04484d72fa5bfe6c5e3a5d650`.

| Probe | Isolated cost |
|---|---|
| `scalar-get` | cache-hot scalar reads through runtime-selected indices |
| `scalar-set` | cache-hot indexed mutation |
| `proven-bounds` | sequential length-guarded access intended for future MIR range facts |
| `unproven-bounds` | dynamic modulo indices that retain runtime bounds work |
| `growth` | failure-atomic repeated growth from empty storage |
| `reserved-growth` | equivalent pushes after non-observable `reserve` |
| `density` | repeated traversal of one million scalar cells |
| `nested-traversal` | row-major managed nested-array access |
| `checked-loop` | checked integer recurrence without array access |

`ruda.array-probes/1`, `ruda.algorithm-benchmark/2`, and
`ruda.performance-budgets/1` are strict custom schemas. Validation rejects
wrong runtime/output profiles, checksum/stdout/stderr drift, timeout,
insufficient/even samples, incomplete metadata/commands/order, non-finite or
duplicate JSON values, summary drift, current-source/manifest drift,
incompatible compiler/runtime/resource/platform/protocol baselines,
unreviewed budgets, workload-set mismatch, and over-budget medians.

Twenty-eight Python unit/negative tests cover the checkpoint-21 owners; the
sibling total is 37 when the checkpoint-20 equivalence tests are included.
They include exact Tier 1 system/architecture normalization, unsupported and
inconsistent platform metadata, MSVC discovery fallback, Windows `.lib`
runtime discovery, unavailable tool metadata, logical and file checksum drift,
and missing/duplicate/escaped/mismatched Tier 1 registry entries. Ruda formatter
and checker validation covers all nine entrypoints plus their shared core.
No allocation-capable C API or production C path changed, so existing aggregate
fail-every-allocation and zero-counter tests remain the applicable memory
coverage and run in every main-repository preset.

## Reviewed macOS arm64 observations

Environment: macOS Darwin 25.3.0 arm64, Apple M2, Apple Clang 17.0.0, Python
3.9.6, Go 1.26.4, Rust 1.96.0. Low-power mode was disabled; the recorded
measurement interval used battery power at high charge. Load averages and the
power source at both suite boundaries remain in raw JSON; no sample was
removed. The largest retained noise fraction was 7.56% (`heap-sort`), below
the reviewed 10% budget floor.

Ruda compiler SHA-256:
`a604fed65dce8e863241baa9b0ab278902b50598c90cef69c9b2c49bc597a3e3`.
Release runtime SHA-256:
`10863a9b7e36fdd51f0483ddd2d82640ff5a2e79474e0e746f8bdbbe34dfb5b4`;
manifest SHA-256:
`326b650acb58b1d66f98291cf9177ef825ae461a45fc84b9be5ddc6016e661f4`.
The runtime manifest says `profile=release`, `host=Darwin`, `processor=arm64`,
and `compiler=AppleClang-17.0.0.17000604`. Generated C used `/usr/bin/cc`,
`-std=c17 -O2`, the complete warning-as-error set, strict floating-point
flags, and `-lm`; the exact vector is in both raw reports.

| Ruda workload | Median ms | Noise | Reviewed max median ms |
|---|---:|---:|---:|
| scalar-get | 96.957 | 2.91% | 106.652 |
| scalar-set | 127.138 | 2.28% | 139.852 |
| proven-bounds | 264.163 | 2.00% | 290.579 |
| unproven-bounds | 258.445 | 2.83% | 284.290 |
| growth | 88.833 | 4.18% | 97.716 |
| reserved-growth | 87.667 | 3.96% | 96.434 |
| density | 428.378 | 2.00% | 471.215 |
| nested-traversal | 211.872 | 4.91% | 233.059 |
| checked-loop | 310.763 | 2.00% | 341.839 |
| insertion | 4003.605 | 2.00% | 4403.965 |
| heap-sort | 1007.910 | 7.56% | 1108.701 |
| stack | 395.265 | 2.62% | 434.791 |
| queue | 429.975 | 2.00% | 472.972 |
| min-heap | 1896.281 | 6.13% | 2085.909 |
| DSU | 1948.329 | 2.00% | 2143.162 |
| BFS | 2488.138 | 2.00% | 2736.952 |
| Dijkstra O(V²) | 885.366 | 2.00% | 973.903 |

The reviewed budget document SHA-256 is
`f3ff7fb01d42b7cbac7d0aa04711c6f8096fae6d1d98a03bb99774b4e8879566`.
The final raw probe JSON SHA-256 is
`0ab81bdc0ac202d6076e81a38f3708675c343eac97ef874ef0d199b03f25442c`;
the algorithm JSON SHA-256 is
`36d15c44686162ba1e35a91bcda7720ac317ec944d0d3c13c1fe9c6ae4eccde1`.
The sibling evidence commit preserves both reports, the Markdown summary, and
the reviewed budget.

## Reviewed Linux x86_64 observations

The dedicated Gitea Actions runner completed run
[`#81`](http://ci.example:3000/OS/ruda-algotithms/actions/runs/81) from
commit `22be069a00c563f3261d0dd79759b6a7eb48dd2d`. Environment: Linux
6.12.57+deb13-amd64 x86_64, four logical CPUs reported as QEMU Virtual CPU
2.5+, Clang 18.1.3, Python 3.12.3, Go 1.26.6, and Rust 1.96.0. The runner
recorded load at both suite boundaries and retained every sample. The largest
Ruda noise fraction was 3.77% (`checked-loop`); the largest fraction anywhere
in the complete Ruda/Go/Rust matrix was 12.82%, both below the 30% rejection
limit. All 17 Ruda workloads therefore use the reviewed 10% budget floor.

Ruda compiler SHA-256:
`da467ede4b7e1520ea1977d44ef504458e8ef1ce999ce4dc075bdd5209ff08c1`.
Release runtime SHA-256:
`4ffaec1d42470bbaf946ca90d83f8f35cba836448417d4428119b32a82b034b1`;
manifest SHA-256:
`22303dcb35ea4b0dc5094b06a1f01ca167b3fd0a11aec746a10fc8d419692e90`.
The runtime manifest says `profile=release`, `host=Linux`,
`processor=x86_64`, and `compiler=Clang-18.1.3`. Generated C used
`/usr/bin/clang`, `-std=c17 -O2`, the complete warning-as-error set, strict
floating-point flags, and `-lm`; both raw reports retain the exact vector.

| Ruda workload | Median ms | Noise | Reviewed max median ms |
|---|---:|---:|---:|
| scalar-get | 167.897 | 3.76% | 184.687 |
| scalar-set | 229.951 | 2.00% | 252.946 |
| proven-bounds | 424.440 | 2.00% | 466.884 |
| unproven-bounds | 392.307 | 2.00% | 431.538 |
| growth | 140.729 | 2.43% | 154.802 |
| reserved-growth | 140.747 | 3.54% | 154.821 |
| density | 694.539 | 2.00% | 763.993 |
| nested-traversal | 383.643 | 2.95% | 422.008 |
| checked-loop | 475.486 | 3.77% | 523.034 |
| insertion | 6999.182 | 2.00% | 7699.100 |
| heap-sort | 1790.068 | 2.28% | 1969.074 |
| stack | 522.080 | 2.00% | 574.288 |
| queue | 588.694 | 2.15% | 647.564 |
| min-heap | 3760.088 | 2.00% | 4136.097 |
| DSU | 3188.845 | 2.00% | 3507.730 |
| BFS | 4541.519 | 2.00% | 4995.671 |
| Dijkstra O(V²) | 1727.676 | 2.04% | 1900.443 |

The reviewed budget document SHA-256 is
`59b79ac0ae40c4beda7de4c271110235c49f9856f9e9611bb8afcdc2324ea521`.
The raw probe JSON SHA-256 is
`af228120917b32f81a7e9dbdcfb435ae3d780269406d278180c9c56fce4716d4`;
the algorithm JSON SHA-256 is
`b1ad60853ea0c8ee1accdd34cf00b960fb2f8a4f409964a7c03314bf0d2f7e4b`;
the generated Markdown SHA-256 is
`abcd5675b2153078af7f9e98d30723619f0035b0163c7212e6c7cc6caddccf79`.
The registry validator binds the reviewed budget to those exact result paths,
file hashes, Ruda and corpus revisions, protocol, and both manifests.

## Exact local validation

| Command | Exit | Result |
|---|---:|---|
| `RUDA=../Ma/build/release/ruda ./verify.sh` in sibling | 0 | 37 Python tests; exact Ruda fmt/check/VM/test-both/Debug+Release native, Go vet/tests, Rust fmt/Clippy/tests |
| `RUDA=../Ma/build/release/ruda ./benchmarks/probes.py --samples 11 --warmups 3 --output benchmarks/results/cp21-probes-macos-arm64-apple-m2.json` | 0 | 9 probes × 3 warmups + 11 retained samples; exact checksums |
| `RUDA=../Ma/build/release/ruda ./benchmarks/run.py --languages ruda,go,rust --samples 11 --warmups 3 --output benchmarks/results/cp21-algorithms-macos-arm64-apple-m2.md` | 0 | 8 workloads × 3 languages × 3 warmups + 11 retained samples; exact checksums |
| Gitea Actions sibling run [`#81`](http://ci.example:3000/OS/ruda-algotithms/actions/runs/81) | 0 | checkout, pinned toolchains, Release Ruda build, full corpus verify, 9 probes, 8 × 3 algorithms, schema/budget candidate, raw artifact, and evidence branch all succeeded |
| `./benchmarks/validate_results.py ...cp21-probes...json ...cp21-algorithms...json` on each recorded platform | 0 | compatible release artifacts; macOS arm64 and Linux x86_64 each have 9 probes and 8 algorithms within reviewed budgets |
| partial `validate_tier1_registry(..., require_complete=False)` | 0 | exact reviewed registry entries are `linux-x86_64` and `macos-arm64` |
| `make benchmark-validate` | 2 (expected) | aggregate release gate rejects the incomplete registry and names `linux-arm64`, `macos-x86_64`, and `windows-x86_64` |
| `python3 -m unittest discover -s benchmarks -p 'test_*.py'` | 0 | 37/37 tests passed |
| `git diff --check` in sibling | 0 | clean evidence commits |
| `git push origin main` in sibling | 0 | raw Linux evidence and its reviewed registry advanced `origin/main` to `ed4a220` |
| `./tools/check-architecture` | 0 | focused C ownership, dependencies, registration, and line budgets clean |
| `./tools/check-c-memory-policy` | 0 | central production allocator boundary clean |
| `./tools/check-generated` | 0 | bytecode, diagnostic, Unicode, and syntax artifacts current |
| `./tools/check-doc-links` | 0 | evidence, roadmap, decision, and sibling links valid |
| `./tools/check-spec-terms` | 0 | D056 decision index and normative terminology clean |
| `./tools/check-syntax-coverage` | 0 | 75 syntax kinds and 116 grammar productions covered |
| `./tools/check-example-syntax` | 0 | public Ruda examples accepted |
| `cmake --build --preset debug --parallel && ctest --preset debug --output-on-failure` | 0 | 2/2 passed in 270.15 seconds; C-unit 267.94 seconds |
| `cmake --build --preset release --parallel && ctest --preset release --output-on-failure` | 0 | 2/2 passed in 263.96 seconds; C-unit 260.81 seconds |
| `cmake --build --preset asan-ubsan --parallel && ctest --preset asan-ubsan --output-on-failure` | 0 | 2/2 passed in 359.71 seconds; C-unit 356.72 seconds; no sanitizer findings |
| `git diff --check` in main repository | 0 | checkpoint files have no whitespace errors; unrelated user work preserved |

This checkpoint establishes measurement infrastructure and two reviewed Tier 1
baselines before optimization. It does not yet prove complete cross-platform
performance, rank Ruda against another language, widen a semantic contract, or
start checkpoint 22. Linux arm64, macOS x86_64, and Windows x86_64 raw
baselines and engineering review remain required before the Tier 1 performance
gate is complete.
