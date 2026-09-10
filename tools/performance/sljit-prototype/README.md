# SLJIT VM feasibility experiment

This experiment reuses the previous ARM64 prototype's region plan, VM bindings,
source-PC mapping and fuel accounting. A single C adapter delegates machine
instruction generation to [SLJIT](https://github.com/zherczeg/sljit), pinned at
`d6ec3d07dcdbf2256ccae2b84d0d8a2d019f392e` (BSD-2-Clause). No handwritten
ARM64 or x86-64 emitter is carried forward.

These files are diagnostic tooling, excluded from production CMake and ordinary
`mmay run`. They are not a release-ready JIT. In particular, SLJIT's built-in
W^X allocator and default C heap hooks are used for this experiment; the
production memory policy requires central allocator hooks, deterministic
failure tests and owned platform executable-memory adapters before integration.
`code_memory.h` is the retained prototype interface, not an enabled allocator.

From a Marimay checkout at the recorded source base, with a matching Release
archive and a clean SLJIT checkout:

```sh
git clone https://github.com/zherczeg/sljit.git build/sljit-evaluation
git -C build/sljit-evaluation checkout d6ec3d07dcdbf2256ccae2b84d0d8a2d019f392e
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j4
python3 tools/performance/sljit-prototype/build.py --sljit build/sljit-evaluation --unit
```

The script verifies baseline coordinator/image and prototype source hashes,
applies the narrow adapter only to copies under `build/`, links against the
matching archive and records commands, inputs and compiler hash. It also copies
the native runtime and generated build configuration so the existing benchmark
snapshot tool can pin the resulting compiler without changes to the harness.
`--arch arm64` / `--arch x86_64` select a macOS target when the baseline archive
was built for that same target. Full VM execution on x86-64 is still unverified;
the small arithmetic smoke test alone has run under Rosetta.

The standalone `smoke.c` checks signed overflow, unchanged destinations on
failure, branches and memory loads/stores on each backend. The bytecode suite
is run as follows after the `--unit` build:

```sh
mkdir -p build/sljit-prototype/suite-2
cd build/sljit-prototype/suite-2
../mmay_c_unit ../mmay --suite-shard 2
```

Use `tools/performance/compare-vm-preparation.py` with the recorded immutable
32-workload reference, a separately pinned control compiler and this compiler.
That tool preserves source-run execution, all workload arguments, heap limits,
checksums, 3 warmups and 11 samples. Diagnostic single observations are not
accepted speed measurements. The September 10 evidence records all 32
correctness observations and a paired six-workload feasibility comparison.

## Lessons carried forward

- `MarimayBytecodeOperand` stores `unsigned_value` and `signed_value` in separate
  fields. Reading a record selector from the wrong member silently selected
  field zero: Dijkstra read `Edge.to` as `Edge.weight`. The original prototype's
  checksum failure was reproduced with SLJIT before the record selector fix.
- The main checkout already had an independently prepared fix and regressions
  for signed third operands. Its broad conversion also makes a 64-bit enum
  payload selector reject the entire compact image. This experiment narrows
  its copied adapter to record-field selectors; enum helpers continue to read
  the original instruction. No foreign working files are changed or staged.
  A production image fix must cover wide enum selectors and preserve image
  availability as well as record correctness.
- Resetting a bound view on every managed COPY/record read discards it even
  when the object is unchanged. The revised invalidation compares all 16 bytes
  of provenance/slot/generation and retains the view only for the identical
  reference. Epoch changes still expire views across every return to the VM.
- Eligibility is bounded by register, operation and code sizes, independent of
  names or workload identity. Calls remain ordinary VM activations. This is
  insufficient for the call-heavy BFS and DSU workloads.
- The generic JIT currently supersedes D109's image shift loop in an eligible
  function. The measured insertion regression blocks production adoption.

## Production gates

1. Own compiler allocations and executable pages through the central allocator
   and narrow W^X platform interfaces; exercise every deterministic failure and
   teardown, with both allocation counters zero.
2. Add focused JIT differential tests for every operation, signed field
   selectors, full-width enum payload selectors, reference rebinding, aliasing,
   forced GC, all finite frame/shared-fuel boundaries and original trap spans.
   The existing bytecode suite passing is necessary, not complete JIT coverage.
3. Preserve or exceed existing composed-loop performance and remove repeated
   activation/dispatch costs across ordinary calls while retaining logical call
   depth and root ownership.
4. Validate every supported Tier 1 host, sanitizers and the unchanged complete
   32-workload matrix before enabling a production tier. Rosetta smoke evidence
   is neither Linux/Windows coverage nor an x86-64 performance result.
