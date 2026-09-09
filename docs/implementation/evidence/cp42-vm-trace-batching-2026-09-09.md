# CP42 verified VM trace batching — local evidence

Local Apple M2 / macOS arm64 evidence, 2026-09-09. This intermediate D109
slice does not close VM/Bun parity, native parity, CP42, CP41, startup, Tier 1
or public-alpha acceptance.

The unchanged inputs are in
`/workspace/Projects/Marimay/algorithm-benchmarks`. The D109 causal control
is an immutable detached build of commit `9fb7523`, which includes the
independently committed MIR-preparation index but not D109. The complete D108
VM journal is `benchmarks/.build/catalog/journals/run-d0hfcbz9.jsonl`; the final
integrated D109 journal is
`benchmarks/.build/catalog/journals/run-fw34x6rd.jsonl`. Their compiler binary
SHA-256 values are `7c00ebbe6fc61ac6a30dee701d71aad3c84762e3a7f5be48da906eb64ed7a51d`
and `09f44f4c2e54fd7bfdfabedfc54761ab4190f6c46331027ed730dfa2cd02195d`.

## Implemented boundary

- The array-shift execution-image owner recognizes the complete generic
  opcode, operand, definition/use and CFG relation. It receives no source,
  package, symbol or workload identity.
- A batch starts only with enough frame/shared fuel for all 17 original
  instructions in one iteration. It validates reads, comparison, destination,
  source and mutation-generation capacity before the iteration's first write.
- Successful iterations write the array in original order and retain the loop
  values and generation in C locals. Portable `memcpy` element access makes no
  extra alignment assumption.
- Before fuel, condition, comparison, bounds or generation side exit, the
  runner materializes exactly the last complete iteration. The ordinary D108
  runner executes the first uncommitted instruction and retains panic/span
  ownership.
- Eligibility rejects an invariant or written register that aliases the
  induction register, every invariant/definition overlap, and duplicate
  retained definitions. The stable storage pointer is retained only because
  the matched trace cannot
  allocate, grow, collect, call or invoke host code.

## Measurements

Eleven full-size observations alternated the detached `9fb7523` control and
D109 candidate after two warmups per executable. Every stdout value was
`insertion 999956059`.

| Build | Raw elapsed times, ms | Median |
|---|---|---:|
| `9fb7523` without D109 | 980.006, 977.586, 1009.388, 976.698, 978.865, 984.368, 984.218, 982.895, 981.081, 975.900, 980.439 | 980.439 |
| `9fb7523` with D109 | 234.219, 234.166, 251.533, 234.417, 234.421, 232.926, 234.362, 235.253, 233.491, 233.011, 234.677 | 234.362 |

D109 is **4.18× faster than its same-base control** in this paired window.
Against the user's original 6 142.94 ms VM result it is about **26.2× faster**.
Against the existing Bun/TypeScript median of 87.63 ms it remains **2.67×
slower**, with
the VM number still including source loading, checking and lowering.

The final full VM-only replay used 3 warmups and 11 measured samples for all 32
unchanged workloads and passed every checksum. It began with load averages
3.65/3.58/4.17; the D108 replay began at 1.28/1.83/2.55. Relative to D108,
`insertion` changed 932.23 → 230.19 ms while every other cell stayed between
-5.9% and +3.5%. Summed medians changed 28 305.37 → 27 977.31 ms, a 1.2%
reduction despite the higher starting load; the geometric ratio across all 32
cells is 0.952. The alternating same-base series above isolates D109 from the
concurrently committed MIR-preparation change.

## Correctness and resource evidence

`test_bytecode_vm_image_shift.c` builds a general five-block bytecode program
and compares cold verified dispatch with image execution. For both per-frame
and shared fuel it checks every budget from 0 through 62, spanning three full
iterations and every internal instruction boundary. It compares status,
result, remaining fuel, all array elements, mutation generation, trap span and
panic code/span. Separate comparisons cover a false loop condition, out-of-
bounds iteration, saturated mutation generation and a verifier-valid trace
whose write aliases a retained loop invariant and therefore must use D108.
Teardown requires both allocator counters to be zero.

Recorded commands:

```text
cmake --build build/release -j6
ctest --test-dir build/release -R '^mmay_c_unit_bytecode$' --output-on-failure
MMAY=build/release/mmay ./benchmarks/catalog_bench.py --engine vm --anchors --console
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

Final verification:

- Release CTest: **86/86 passed**, 714.81 seconds.
- Focused Apple ASan/UBSan CTest: **11/11 passed**, 129.11 seconds,
  including bytecode/runtime, allocation indices 0–7 and managed string
  lifetime. Apple leak detection was disabled; Linux LeakSanitizer remains a
  CI/Tier 1 obligation.
- Architecture, roadmap, C-memory, documentation, generated-file, specification
  terminology, syntax, example and release-contract gates passed.

## Open performance work

This batching tier covers one general trace family. It does not yet cover the
function/call/control traces dominating DSU, min-heap and BFS, and it does not
reduce source-pipeline startup for short workloads. Native text, Map/Set,
temporary result transport and record-allocation costs also remain open. No
parity gate is claimed by this result.

## Book use

No new idea from Vladimirov was used in D109. The design follows the measured
D108 CPU profile, exact finite-fuel differential results and the previously
selected execution-image architecture. The earlier D106 use remains recorded
with printed pages 71–74 / PDF pages 72–75.
