# Checkpoint 41: x86_64 code-generation trigger

Date: 2026-08-24.

Status: planning/trigger evidence only. CP41 remains `planned`; this report is
not a D056 baseline, optimization result, or completion claim.

Measured Marimay revision:
`97cbc0e152bfbdc06a9806bafbf8515dd6ee17e3`. Benchmark-corpus revision:
`4a0d56662e167a18f9b2ee8690af31fcd0ad958b`. The Linux artifacts were retained
on evidence commit `4a627875e5d5685cc1923bcd7fb5c26b8e06fec9` after Gitea runner
run 20 completed successfully. The evidence commit contains the raw 3-warmup,
11-sample algorithm, focused-probe, and CP30 dynamic-text JSON reports.

## Trigger observation

The Linux runner used Linux 6.12.57 x86_64, a four-logical-CPU QEMU Virtual
CPU 2.5+, Clang 18.1.3 for generated C, Go 1.26.6, Rust 1.96.0, the 64 MiB
managed-heap limit, and the D056 rotating-balanced protocol. The compiler and
release runtime were profile-coherent and the exact outputs, empty stderr,
exits, and workload checksums passed.

The runner was nevertheless too busy for a new reviewed baseline: load average
was 7.106/7.437/5.988 at start and 5.860/7.132/5.918 at end on four logical
CPUs. The Go queue distribution had a 69.2% noise fraction; D056 rejects a
baseline above 30%. The run is therefore a reproducible trigger and diagnostic
artifact only. Its algorithm JSON SHA-256 is
`683aa43264f7b9fe73832f400de941c8f596fc895ef7db0a67d4b7ba0fa9c3de`;
the focused-probe JSON SHA-256 is
`31c262b4d79d981bf8012eadab2232c457ee3ea311785c39ce6f89d0252aaa65`.

Linux medians were:

| Workload | Marimay ms | Go ms | Rust ms | Marimay / Go |
|---|---:|---:|---:|---:|
| insertion | 91.253 | 48.717 | 47.172 | 1.87x |
| heap-sort | 90.576 | 60.975 | 51.459 | 1.49x |
| stack | 30.476 | 19.032 | 9.636 | 1.60x |
| queue | 32.012 | 22.538 | 9.704 | 1.42x |
| min-heap | 131.542 | 86.991 | 66.358 | 1.51x |
| dsu | 64.239 | 54.081 | 44.846 | 1.19x |
| bfs | 132.152 | 151.722 | 55.184 | 0.87x |
| dijkstra | 90.270 | 79.949 | 45.893 | 1.13x |

An immediate local macOS arm64 rerun used the same CP30 compiler revision,
same corpus, and the same 3/11 protocol. Its Marimay medians were 49.30,
31.91, 12.16, 12.54, 52.19, 30.82, 51.14, and 41.10 ms in table order. This
local diagnostic is supplemental and is not committed as a CP41 baseline.
Normalizing Marimay to the same-platform Go participant changed as follows:

| Workload | macOS arm64 Marimay / Go | Linux x86_64 Marimay / Go |
|---|---:|---:|
| insertion | 1.69x | 1.87x |
| heap-sort | 0.82x | 1.49x |
| stack | 0.82x | 1.60x |
| queue | 0.87x | 1.42x |
| min-heap | 0.97x | 1.51x |
| dsu | 1.27x | 1.19x |
| bfs | 0.82x | 0.87x |
| dijkstra | 1.00x | 1.13x |

These cross-language ratios locate the issue but are explicitly exploratory
under D056. They are not release budgets and cannot be CP41 completion gates.

## Narrowing the cause

The exact CP30 compiler's nine focused array/native probes did not show the
same selective amplification. Linux-x86_64 time divided by local-macOS-arm64
time was 1.21x for scalar-get, 1.02x scalar-set, 0.99x proven-bounds, 1.27x
unproven-bounds, 1.23x growth, 1.14x reserved-growth, 1.05x density, 1.06x
nested-traversal, and 0.96x checked-loop. That makes a general Linux host or
basic array-access explanation insufficient.

The generated stack, queue, heap-sort, and min-heap C contains simultaneously
live 16-byte managed handles, record/array activation views, immediate enum
payloads, status/cleanup state, identity/descriptor checks, mutation generation,
and cold panic/error edges. Clang inlined the hot generated functions for both
targets, so the finding is not a missed-inline boundary.

To remove toolchain-version and source differences, the same emitted C was
compiled by the same Apple Clang toolchain with only the target changed between
arm64 and x86_64. Symbolized object inspection produced:

| Hot function | arm64 code bytes | x86_64 code bytes | Change | arm64 stack-addressed ops | x86_64 stack-addressed ops |
|---|---:|---:|---:|---:|---:|
| stack | 5,712 | 7,648 | +34% | 351 | 533 |
| queue | 6,000 | 7,952 | +33% | 367 | 497 |
| heap-sort | 5,436 | 7,152 | +32% | 293 | 451 |

For context, the corresponding Go hot-function sizes grew far less under the
same arm64-to-x86_64 comparison: stack 416→448 bytes, queue 432→480,
heap-sort 496→608, and min-heap 496→544. This does not prove a single compiler
defect by itself, but together with the workload selectivity and focused probes
it localizes the likely cause to register pressure and live-state shape in the
generated native path rather than to Marimay source semantics or corpus drift.

## Required next evidence

CP41 must begin with a clean dedicated Linux x86_64 D056-eligible before-run,
then retain identical-protocol after-runs, generated C, optimization remarks,
symbolized cross-target assembly, and stable Linux hardware counters. It must
show that spill/branch pressure decreases without changing algorithms, typed
MIR, checks, rooting, allocation/failure order, panic spans, or VM/native
results. The macOS arm64 and Go/Rust observations remain supplemental. The
full five-platform D057 public-alpha registry remains a separate open gate.
