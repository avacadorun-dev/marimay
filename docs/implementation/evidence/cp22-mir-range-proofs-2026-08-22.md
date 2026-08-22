# Checkpoint 22: verified MIR range-proof capabilities

Date: 2026-08-22
Scope: internal MIR/bytecode/native optimization foundation; no source syntax
change.

## Delivered contract

- `ARRAY_REQUIRE_INDEX` remains the sole observable D045/G4002 bounds barrier.
- `ARRAY_GET_IN_BOUNDS` and `ARRAY_SET_IN_BOUNDS` consume its matching
  verifier capability, so a compound indexed assignment does not re-check the
  same index after the barrier. A MIR-only assumption additionally removes the
  barrier for literal lengths and exact `index < array.length()` true edges
  whose separate `index >= 0` must-fact reaches the CFG fixed point;
  it is serialized as a checked bytecode barrier and introduces no
  source-visible rule or cached array cell.
- The MIR and bytecode verifiers independently reject a missing or forged
  carrier. Calls, interface calls, stores, and length/capacity mutations clear
  all range capabilities.
- The optimizer prepares every allocation before rewriting and rolls back on a
  post-rewrite verification failure. Resource exhaustion returns checked MIR;
  verifier exhaustion rejects the artifact.
- VM, MIR oracle, and generated C each route in-bounds operations to the
  explicit runtime trust-boundary helpers. No raw cell pointer is cached.

## Focused coverage

`test_mir_range.c` covers transactional rewrite of the get/store pair,
checked-versus-rewritten MIR and VM execution, literal and source-level nested
affine-loop assumptions, a forged negative true edge, hostile-bytecode proof
rejection, generated-C execution at both `-O0` and `-O2`, direct and interface
call invalidation, every array mutation and block-boundary invalidation, and
deterministic pass/verifier/codec allocation sweeps with zero live allocator
counters after teardown. Existing checker, imported-array, opcode-registry,
VM, native, and CLI suites exercise the canonical instructions through their
adjacent owners.

## Local validation

The following completed locally on macOS arm64:

```text
cmake --build --preset debug --parallel 4
ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
cmake --build --preset release --parallel 4
ctest --preset release -R '^ruda_c_unit$' --output-on-failure
cmake --build --preset asan-ubsan --parallel 4
ctest --preset asan-ubsan -R '^ruda_c_unit$' --output-on-failure
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-generated
./tools/check-doc-links
./tools/check-spec-terms
```

The generated native code contains one checked `ARRAY_REQUIRE_INDEX` barrier
per source indexing operation unless a literal or true-edge range fact proves
it unnecessary, and replaces the following matching get/store with the
in-bounds helper. A generated array acceptance program has seven barriers and
seven in-bounds helpers; the `proven-bounds` generated program has five dynamic
barriers and seven in-bounds loads. The 11/3 supplemental Apple M2 probe report is
`../ruda-algotithms/benchmarks/results/cp22-bounds-macos-arm64-apple-m2.json`:
`proven-bounds` median improved from 264.163 ms (CP21) to 207.477 ms (CP22),
or 21.46%. The old algorithm-repository validator still requires a paired
algorithm report with the identical compiler hash, so it correctly rejects
mixing this CP22 probe report with the CP21 algorithm report; no unsupported
budget-pass claim is made here.

D057 still requires the reviewed Linux x86_64 performance result and the
separate public-alpha Tier 1 registry. This document records local evidence
only and does not claim either external gate as verified.
