# CP42 external-config finding: accidental local VM fuel

Scope: local D087 implementation for the commit carrying this report, based
on `f53b2896582e60806f4bb1295c7227054fbb528b`, macOS arm64 / AppleClang 21.
CP42 and the ten-utility portfolio remain incomplete. This is not Tier 1
completion or public-release evidence.

## Reproducer and diagnosis

The external `Marimay-Examples/marimay-config` application validates a strict
versioned project-scanner configuration. Its deterministic `tools/measure.py`
dataset contains 43,680 distinct required paths in a 1,048,459-byte JSON
document. This is below the input, string and container-item limits. Placing
`version` first or last succeeds natively but exited 70 with generic I0002
in the unmodified `f53b289` VM.

Input SHA-256 values:

```text
8e2cdd14ff180c15662d3b9162c442fc2ae1dc72f2d99914da7984bc9fba4d38  version-first
e8b36fe61735866040d39a4ab5492ba6ab390879c843534fc2672411df64ceb7  version-last
```

LLDB breakpoints at `mmay_cli_run_runtime_error` and the failed step check in
`bytecode/vm.c` established `MMAY_BYTECODE_RUN_STEP_LIMIT`, frame steps and
maximum both exactly 10,000,000, and frame count 4. No source instrumentation
or verifier bypass was used. The initial failed VM timings and RSS are not
successful workload measurements.

## Repair and boundaries

D087 records the conflict with VM/native observable equivalence and makes
ordinary local run/test policy explicit. The CLI selects unmetered per-frame
instructions; the embedding default remains finite. `SIZE_MAX` is an explicit
unmetered sentinel, not a larger magic limit that fails on the next workload.
Unmetered frames do not increment a counter. A supplied shared budget still
consumes exactly one unit per instruction and stops before executing with no
fuel. Finite per-frame boundaries, invalid zero maxima and call-depth checks
are unchanged. So are the MIR oracle, fuzz fuel, memory/capability checks,
encoded-bytecode decoding/verification and allocation ownership.

The remaining VM/native call-stack resource-policy mismatch is explicitly
retained in CP42's resource-boundary removal gate. This repair is not a claim
that all resource limits or infinite programs behave identically.

## Focused verification

```sh
cmake --build build/release -j 4
cmake --build build/debug -j 4
cmake --build build/asan-ubsan -j 4
ctest --test-dir build/release --output-on-failure \
  -R 'mmay_cli_local_execution|mmay_c_unit_bytecode$'
ctest --test-dir build/debug --output-on-failure -j 3 \
  -R 'mmay_cli_local_execution|mmay_c_unit_bytecode$|mmay_c_unit_cli-(run|test|heap)$'
ctest --test-dir build/asan-ubsan --output-on-failure -j 2 \
  -R 'mmay_cli_local_execution|mmay_c_unit_bytecode$|mmay_c_unit_cli-(run|test|heap)$'
```

Final focused Release pair passed; Debug and ASan/UBSan each passed all five
selected CTests in 54.78 and 105.18 seconds respectively. These overlapping
correctness runs are not performance baselines or a Linux LeakSanitizer claim.
The bytecode tests retain finite limits and add unmetered/shared exact
boundaries, bounded termination of an infinite loop, preserved call-depth
failure, and every reachable allocation-failure index for the unmetered
standalone path. Complete teardown asserts both live counters are zero.

The new public `local-execution` corpus runs a finite loop with 10,000,001
iterations through `run` and `test --engine both`. The process owner compares
VM, debug native and release native status/stdout/stderr for JSON exactly at
1 MiB, invalid syntax at its end, stdin one byte over its limit, and JSON one
byte over its own limit. The latter byte is appended after a legal stdin read:
requesting a 1,048,577-byte stdin limit would itself be an invalid API request.
Initial fixture assumptions about that limit and zero per-frame fuel were
corrected to the existing contracts, without changing those contracts.
Formatting and checking are included; there is no new source syntax.

All generated/spec/document/example/syntax/architecture/C-memory/roadmap/
release-contract gates passed during integration. The new process CTest raises
the registered count from 67 to 68. No tests, failure indices or sanitizer
checks were removed.

The final full Release command `ctest --test-dir build/release
--output-on-failure -j 4` passed all 68 CTests in 241.68 seconds. The installed
Release binary passed `mmay doctor`; its SHA-256 at measurement time is
`7609f2b1694dcc08af772f5c46c0a01420cac0e6f9de3f02739acaa6355564ee`.
After staging the new regression files so the tracked-source archive includes
them, `mmay_cp39_distribution` was rerun and passed in 16.28 seconds.

Expanded external `python3 tools/acceptance.py` passed 32 exact process cases
across VM/debug-native/release-native, plus atomic replacement, unchanged
destination on failure, capability denial and broken-pipe checks, in 88.70
seconds under parallel compiler-test load. The new cases retain the original
near-limit bytes, both version orders, exact 1 MiB, one byte over, and an
invalid final token. Application `.my` source needed no workaround or change.

## Sequential application measurements

After local build/test processes completed, `python3 tools/measure.py --output
build/measurements-d087-20260906.json` ran the original datasets sequentially.
Normal desktop applications remained running; no OS cache eviction or truly
disk-cold/idle-host claim is made. The report records compiler/source hashes,
raw results and timings; measured successful validation now requires the exact
expected JSON report, not merely exit zero. The language working tree was the
tested D087 change, not a clean `f53b289` binary.

The external raw report SHA-256 is
`569221e9cfef92c00f0779e40c95fcf2b89f1d57d36a4a154cffac15aceb4b85`;
the preserved initial report is
`7f7a04069cd74a9d83684d9e03f5964da52f6931fde90cfca07e666d001c914f`.

The first small VM run took 0.2887 s; repeated runs were 0.2897–0.3008 s.
The large original input succeeded in 6.7441 s with version first and 11.2990 s
with version last. The latter **misses the predeclared 10 s VM budget**.
Native counterparts took 0.4147 and 0.5653 s. Peak RSS on the successful
version-first input was 152,813,568 bytes for VM and 91,717,632 for native,
both below the predeclared 256 MiB process budget. RSS is not the same as the
managed-heap byte counter, and is not evidence for all memory boundaries.

The first invocation of the newly copied native artifact still took 0.5935 s,
above the 0.1 s small-native budget; subsequent invocations took 0.0035–0.0060 s.
The cause of this first-invocation overhead has not been established. Repeated
seven-test both-engine loops took 5.5543–5.6468 s with an already exercised
native cache; they do not establish cold test-build latency. The original
baseline's first 14.5976 s test loop remains an open observation.

Thus this change fixes correctness, not the large-input performance or all
startup ergonomics. Next investigations include byte copying and the
version-last prefix scan/second decode; they must retain the exact inputs and
all error, resource and verification contracts.

## Outstanding acceptance

The config application now accepts the original near-limit document in the VM
without a source workaround. Measured budget misses remain open above.
OS permission/symlink cases, other resource boundaries, all remaining utility
repositories, the CP42 ergonomic findings and missing Tier 1 evidence remain
open. Previous CI results apply only to their own revisions.
