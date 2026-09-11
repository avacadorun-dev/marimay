# Source-run phase attribution after prepared JIT activations

Status: **diagnostic-complete**. This is a cost attribution result, not acceptance
of a new speed optimization. Overall Go/Rust parity remains open.

[Raw observations, checksums and replay scripts](cp42-source-phase-profile-2026-09-11.json).
The parent is `aa0c92b3a77e256df19d16316e094fcb9b25d77c`; the uninstrumented
Release compiler SHA-256 is
`5a1ed60d7f05f3ca38cf6bb6003925d8b1285196131100b7fbd38c93eb6f4db7`.

## Method and integrity

Two separately preserved diagnostic series each execute all 32 original
workloads in three alternating fresh-process control/instrumented rounds:
384 observations total. All stdout, exit codes, 114 source/harness hashes and
the 64 MiB heap cap match the immutable reference. This is not a 3-warmup,
11-sample benchmark. Production sources, baseline compiler and its archives
are unchanged. Clocks are added only to copies of focused owners linked before
the exact Release archives. The first series partitions source loading, frontend,
MIR optimization, lowering, codec, verification, preparation and execution.
The second attributes nested verifier calls, with exclusive phase intervals.
All per-process CPU/wall intervals reconcile with their totals.

Instrumentation can change layout and elapsed time. Instrumented/control
median ratios have geometric means of 1.0007 and 1.0086 for the two series;
three samples do not establish a precise instrumentation overhead. Host isolation
is unverified. These data choose a hypothesis; they cannot accept it.

## Finding and next candidate

For `map-int`, the refined diagnostic records about 39 ms CPU in MIR and
bytecode-function verification within an approximately 90 ms source run.
The catalogue program has 67 bytecode functions; each is verified eight times
(536 function-verifier calls). Whole-program verification occurs five times;
function lowering, encoding and decoding add three further function checks.
The actual codec work is much smaller than its inclusive phase originally
suggested. MIR verification is also repeated between bounded optimizations.

The next candidate will remove redundant function verification inside the
already checked program encoder and the program lowerer, and remove the CLI
repeat after successful whole-program decoding. Public standalone APIs keep
their checks; the decoded program and VM admission remain fully verified.
D049's encode/decode roundtrip, all checked functions, limits, failure outcomes
and VM execution semantics must remain intact. This is selected work, not an
implemented or accepted shortcut. Long DSU/BFS workloads still spend most of
their time executing and require separate runtime/JIT work.

## Refined diagnostic medians

Control is external wall time; the remaining columns are exclusive process
CPU phase medians. Do not sum independently computed medians as an exact
per-process total. These are the unchanged accepted implementation.

| Workload | Control wall ms | MIR verify CPU ms | Bytecode function verify CPU ms | VM execution CPU ms |
|---|---:|---:|---:|---:|
| title-search-10k | 114.04 | 14.59 | 24.71 | 50.54 |
| title-search-100k | 578.73 | 14.61 | 24.54 | 513.09 |
| normalize-title | 178.00 | 14.60 | 24.36 | 107.01 |
| word-frequency | 328.53 | 14.47 | 24.67 | 250.57 |
| report-builder | 105.49 | 14.51 | 24.36 | 41.59 |
| report-concat | 181.04 | 14.47 | 24.30 | 116.41 |
| map-int | 89.41 | 14.58 | 24.46 | 27.53 |
| map-string | 114.10 | 14.54 | 24.42 | 50.75 |
| map-string-book | 120.15 | 14.63 | 24.45 | 50.48 |
| set-string-churn | 140.01 | 14.63 | 24.38 | 67.03 |
| catalog-match | 163.91 | 14.58 | 24.43 | 101.77 |
| catalog-dotcall | 139.90 | 14.60 | 24.41 | 77.69 |
| catalog-interface | 179.94 | 14.58 | 24.47 | 117.75 |
| record-reference-copy | 116.03 | 14.57 | 24.38 | 54.21 |
| record-create | 150.32 | 14.54 | 24.40 | 89.66 |
| array-grow | 124.47 | 14.50 | 24.34 | 62.01 |
| array-reserved | 123.01 | 14.56 | 24.34 | 61.78 |
| bfs-int | 102.66 | 14.55 | 24.44 | 40.75 |
| bfs-record | 110.68 | 14.93 | 24.52 | 48.79 |
| catalog-pipeline | 271.55 | 14.59 | 24.56 | 210.48 |
| parse-result | 101.31 | 14.51 | 24.45 | 35.23 |
| insertion | 222.42 | 6.88 | 12.45 | 181.79 |
| heap-sort | 300.26 | 6.94 | 12.44 | 260.46 |
| stack | 432.76 | 6.91 | 12.46 | 392.71 |
| queue | 440.47 | 6.95 | 12.41 | 395.80 |
| min-heap | 731.73 | 6.91 | 12.44 | 678.85 |
| dsu | 1184.11 | 6.95 | 12.46 | 1162.35 |
| bfs | 1407.66 | 6.99 | 12.54 | 1349.58 |
| dijkstra | 228.87 | 6.93 | 12.44 | 187.00 |
| frequency-map | 285.34 | 6.91 | 12.38 | 270.08 |
| unique-set | 199.42 | 6.96 | 12.48 | 166.45 |
| rune-frequency | 156.14 | 6.91 | 12.41 | 117.53 |

## Reproduction

Extract `reproduction_files[*].text` from the JSON into
`build/source-phase-profile/`, checking each stored SHA-256. At the exact parent
revision, retain/build the recorded Release compiler with its pinned optional
SLJIT configuration. The build record contains the compiler, archive and source
hashes and every compiler/linker command. From the worktree root run:

```sh
python3 build/source-phase-profile/build-profile.py
python3 build/source-phase-profile/run-profile.py
python3 build/source-phase-profile/verification/build-profile.py
python3 build/source-phase-profile/verification/run-profile.py
```

The runners discover the sibling `algorithm-benchmarks` corpus and verify it
against the committed September 9 reference. A different checkout path changes
build-location metadata; it is a new diagnostic artifact and must be recorded.
The original scripts require their exact baseline hash rather than quietly
accepting a different compiler. No target-specific instruction is added to the
production implementation by this diagnostic.
