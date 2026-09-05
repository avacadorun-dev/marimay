# Map/Set result-bearing operations — local integration

Scope: D079 implementation optimization on top of the full CP39/CP41 merge
`4132f7c`. This report does not close CP41, CP39, CP40, or a public-alpha gate.
The source-level API, insertion order, key equality and mutation rules do not
change.

## Implementation

- Map get/remove directly produce Option; Map put and Set add directly produce
  Result. The checker no longer emits a membership query and a multi-block
  wrapper for each call.
- Runtime insertion reuses the computed hash and absent bucket. Growth or
  compaction re-probes after rebuilding storage. Primitive keys have a direct
  comparison/dispatch path; keyed SipHash is retained for every key family.
- The MIR oracle, hostile bytecode verifier, VM and generated C cover all four
  result-bearing operations. Bytecode opcode 153 remains TEST_ASSERT; the new
  operations occupy 154–157 and do not include that intervening instruction.
- Native liveness includes key/value operands, mutation invalidation, and
  collection safepoints. VM temporary root locations are owned by their caller;
  removed managed values stay rooted while Option is allocated.
- Configured collection resource limits remain execution failures, distinct
  from the source language's int::MAX growth result under D055/D079.

## Focused verification

The CP36 owner tests cover all four operations, update/duplicate booleans,
absence, insertion order, iterator invalidation, array-valued Map results,
string keys, forged result identities/layouts, invalid key/value registers,
bytecode round trips and MIR/VM/native output parity.

Allocation tests sweep checker, oracle, VM and emitter failures with both
zero-live-allocation and zero-live-byte assertions. The generated-C test
compiles at O0 and O2 with collection before allocation, obtains the successful
allocation count, then replays every failure index in a fresh execution and
checks both counters after full teardown. Local ASan/UBSan CP36 validation
passed after this generated-code sweep was added (66.82 seconds).

## Final integration replay (macOS arm64)

The final Release build passed checker, bytecode, native, CP36 unit, JSON notes
process scenarios, notes model, frequency counting and configuration migration.
The nine-group selection took 516.23 seconds. Its only failure was the CP36
process harness still expecting three tests after the reference-value test
added a fourth; the corrected harness passed its complete independent replay
in 47.63 seconds. All four language tests passed in both engines.

The final ASan/UBSan CP36 run, including forced VM collections and the
generated-C allocation sweep, passed in 78.20 seconds. ASan/UBSan bytecode and
runtime groups passed (78.40 seconds total). Final Debug checker/bytecode
replay passed (17.96 seconds). Release packaging passed after all new owner
files were staged (16.87 seconds).

```sh
cmake --build --preset release --parallel 8
ctest --test-dir build/release -C Release -R '^(mmay_c_unit_(checker|bytecode|native)|mmay_cp36_(unit|map_set)|mmay_cli_(notes|notes_model|frequency|config_migrate))$' --output-on-failure --parallel 4
ctest --test-dir build/release -C Release -R '^mmay_cp36_map_set$' --output-on-failure
ctest --test-dir build/release -C Release -R '^mmay_cp39_packaging$' --output-on-failure
ctest --test-dir build/asan-ubsan -R '^mmay_cp36_unit$' --output-on-failure
ctest --test-dir build/asan-ubsan -R '^mmay_c_unit_(bytecode|runtime)$' --output-on-failure --parallel 2
ctest --test-dir build/debug -R '^mmay_c_unit_(checker|bytecode)$' --output-on-failure --parallel 2
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-roadmap
./tools/check-generated
```

The earlier Debug application selection was superseded by this Release replay
and stopped before its notes process test completed; it is not claimed as a
fully passing selection.

No wall-clock speedup or D056 performance-baseline claim is made from these
correctness tests. Linux LeakSanitizer and other Tier 1 release evidence remain
CI obligations.
