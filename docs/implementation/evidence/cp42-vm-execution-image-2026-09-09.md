# CP42 verified VM execution image — local evidence

Local Apple M2 / macOS arm64 investigation, 2026-09-09. This is an
intermediate D108 performance slice and records why D107 was rejected. It does
not close CP42, CP41, startup, Tier 1 or public-alpha acceptance.

The unchanged benchmark inputs come from
`/workspace/Projects/Marimay/algorithm-benchmarks`. The control executable
is the immutable installed toolchain
`~/.local/share/marimay/toolchains/15e6a5077e6b18bf/bin/mmay`, SHA-256
`15e6a5077e6b18bfc56908563f19cb08461b12244bf8ebd69561a197e8dd45b4`.
The full 32-workload control report is
`benchmarks/results/comprehensive-local.json`, SHA-256
`732e069f9cd51e6df67230d3547ebe3fd53a26a11ba5ecb387380a0a96f9c3ee`.

## Implemented boundary

- A verified program is flattened into one central-allocator arena of 32-byte
  operations with direct block targets, prepared direct callees and exact
  original block/instruction coordinates.
- Generic CFG relations mark array-shift and record-backed array-follow/path-
  compression loops. Matchers cannot inspect source paths, package symbols or
  workload names.
- Dedicated compact runners retain full managed identity checks and publish
  intermediate register/heap effects in original bytecode order. Finite frame
  and shared fuel retain each original instruction boundary.
- Call-free nested functions whose complete opcode set cannot allocate or
  collect borrow the caller's linked roots. Entry frames and any function with
  calls, growth, allocation, host effects or an unknown opcode remain rooted in
  the ordinary way.
- Functions without a proved specialized loop continue through the compact
  D104 runner. This selection follows the measured 5–15% regression from a
  universal image dispatcher.
- Predecoded execution has its own `vm_image_run.c` translation unit. The
  established `vm_hot.c` runner remains 2 404 bytes on this build, equal to the
  immutable control, after a combined owner changed inlining and regressed
  unrelated algorithms.

## Paired diagnostic measurements

The following five-sample sequence alternated the immutable control and final
candidate after one warmup of each. Each command used `--engine vm
--max-heap-bytes 268435456`; stdout matched on every observation.

| Workload | Control median | Candidate median | Change | Checksum |
|---|---:|---:|---:|---|
| insertion | 6 062.67 ms | 965.82 ms | 6.28× faster | `insertion 999956059` |
| heap-sort | 1 701.39 ms | 1 664.35 ms | 1.02× faster | `heap-sort 999995320` |
| DSU | 5 478.70 ms | 3 846.05 ms | 1.42× faster | `dsu 21343152` |
| BFS | 5 573.46 ms | 5 585.41 ms | 1.002× candidate/control | `bfs 186275928` |
| Dijkstra | 2 416.21 ms | 2 418.03 ms | 1.001× candidate/control | `dijkstra 38672` |

The control/candidate medians for BFS and Dijkstra differ by 0.21% and 0.08%,
respectively. Their alternating samples cross in both directions and do not
show the earlier systematic regression. This remains a directional local
series, not the required final 3-warmup/11-sample matrix.

D107's general record region was also measured directly with seven alternating
samples. Enabling it changed `record-reference-copy` 204.11 → 211.10 ms,
`record-create` 241.75 → 246.65 ms, `catalog-interface` 352.81 → 358.07 ms and
Dijkstra 2 410.01 → 2 436.93 ms. The region and its focused test owner were
removed. Only the fully matched D108 record-backed array loop remains.

The post-change DSU CPU sample still distributes time across the scalar/array
runner, program/frame boundaries and execution-image runner. Against the
existing Bun medians, both remain far from parity. This measures the interpreter
ceiling and requires the next bounded
hot-loop compilation tier; it is not presented as parity.

## Correctness and resource evidence

`test_bytecode_vm_image.c` checks fixed operation layout, direct callee binding,
root-borrow eligibility, both generic follow-loop CFG forms, every finite frame
and shared-fuel value around a real record/array traversal, and the image arena's
allocation-failure/success cleanup with both allocator counters zero.

Recorded commands for this slice:

```text
cmake --build build/release -j6
ctest --test-dir build/release --output-on-failure
cmake --build build/asan-ubsan -j6
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/asan-ubsan \
  -R '^(mmay_c_unit_(bytecode|runtime)|mmay_cp31_allocation_[0-7]|mmay_vm_string_lifetime)$' \
  --output-on-failure
./tools/check-architecture
./tools/check-roadmap
./tools/check-c-memory-policy
git diff --check
```

Final verification after the structural split:

- Release CTest: **86/86 passed**, 580.67 seconds.
- Focused Apple ASan/UBSan CTest: **11/11 passed**, 125.41 seconds,
  including bytecode/runtime, allocation indices 0–7 and managed string
  lifetime. Apple leak detection was disabled; Linux LeakSanitizer remains a
  CI/Tier 1 obligation.
- `check-architecture`, `check-roadmap`, `check-c-memory-policy` and
  `git diff --check`: passed.

## Book use

No new idea from Vladimirov was used in D107/D108. The execution image,
superinstruction shapes and decision to move to a compilation tier come from
the unchanged benchmark matrix, dynamic opcode counts and CPU samples. The
earlier D106 book use remains recorded separately with printed pages 71–74 /
PDF pages 72–75.
