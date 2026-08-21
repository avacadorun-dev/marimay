# Post-preview checkpoint 19: managed-heap resource policy — 2026-08-21

Gate: I3 bytecode VM, I4 C17 equivalence, and I5 managed data. Checkpoint
status: locally complete. I3, I4, and I5 remain `in-progress`; the complete
Tier 1 matrix is not claimed by this local record.

Baseline main commit: `60c6111` (`Complete checkpoint 18 array spelling`).
The behavior-only `32f4443` extraction gave CLI argument display its existing
focused owner before D055 logic was added to the bounded CLI coordinator.

Authority: accepted D045 and D053 array failure contracts plus new accepted
D055. Normative execution, toolchain, diagnostic, standard-library,
requirements, architecture, test-strategy, help/example, and roadmap text are
updated in the same semantic change.

## Delivered slice

- `ruda run`, `ruda test`, and `ruda build` accept one positive decimal
  `--max-heap-bytes N` or `--max-heap-bytes=N`. The range is
  `1..min(int::MAX, SIZE_MAX)` and the unchanged default is exactly
  `67108864`. Zero, signs, whitespace, empty/non-decimal values, overflow,
  missing values, and duplicates have stable `U0004`/`U0005` locations and
  exit `2`. No environment variable changes the selected/default value.
- VM runs receive the selected value in a fresh bytecode execution. Each test
  receives a fresh VM heap or native process with the same value; the
  `--engine both` comparison still uses byte-exact observations.
- Native build and `--emit c` publish an explicit decimal `max_live_bytes`
  assignment, including the default. The generated source participates in the
  existing content-addressed hash, so different budgets have different cache
  identities and standalone artifacts retain their selected policy.
- Managed accounting records published object payloads, reserved array cells,
  and slot-table bytes. Exact equality succeeds. A post-collection excess is
  rejected before its host allocation and records live/requested/limit bytes;
  a subsequent allocator failure records the distinct host-allocation class.
- Source-semantic negative/excessive array growth retains typed
  `ArrayGrowthError` behavior. The native managed-status mapper no longer
  coalesces semantic `LENGTH_LIMIT` with the configured resource ceiling.
- VM human/short/JSON reporting and generated native human reporting use exact
  `I0002` messages, stderr only, and exit `70`. Budget JSON carries the typed
  numeric runtime-resource context; host JSON names host memory without an
  address.

## Ownership and coverage

`cli_heap_limit.c`, `cli_resource_report.c`, and `managed_resource.c` are the
new focused owners. Existing run/test/build coordinators only propagate the
validated value; the native emitter embeds it, and the managed runtime owns
accounting classification.

Focused coverage includes:

- CLI separate/equal spellings, minimum and maximum, default, help, every
  invalid family, duplicate/missing diagnostics, JSON argument location,
  environment-independence probe, test isolation, emitted default/selected C,
  and cache separation;
- exact boundary and one-byte-under construction/growth, collection retry,
  budget preflight without a host allocation, raw and recursive-reference
  arrays, nested push, reserve, failure atomicity, and semantic-length
  precedence;
- every reachable array-construction host allocation failure, focused reserve
  resize failure, VM CLI fail-every-allocation execution, native forced host
  failure, and zero live allocator counters after teardown;
- exact VM/native budget stderr and exit, empty program stdout, versioned JSON
  context, two fresh VM/native test isolates, and standalone native artifact
  execution.

## Exact observations

| Surface | Exit | Observation |
|---|---:|---|
| default run with hostile heap-related environment names | 0 | stdout `cp19 resource limits ok\n`; empty stderr |
| selected 1 MiB VM run | 0 | stdout `cp19 resource limits ok\n`; empty stderr |
| selected 1 MiB `test --engine both` | 0 | `2 passed; 0 failed`; matching VM/native observations |
| selected 1 MiB native artifact | 0 | stdout `cp19 resource limits ok\n`; empty stderr |
| VM with one-byte budget | 70 | exact `error[I0002]: managed heap budget exceeded: live=0 bytes, requested=344 bytes, limit=1 bytes\n` |
| native with one-byte embedded budget | 70 | byte-identical stderr to VM; empty stdout |
| JSON VM budget diagnostic | 70 | schema `ruda.diagnostic/1`, managed-heap budget class, numeric `0/344/1` context |
| forced VM/native host allocator failure | 70 | exact `error[I0002]: runtime host allocation failed\n` |

## Local validation

Environment: macOS Darwin 25.3.0 arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `build/debug/ruda fmt tests/post-preview/cp19-resource-limits --check` | 0 | complete corpus canonical |
| `build/debug/ruda check tests/post-preview/cp19-resource-limits` | 0 | complete package accepted |
| `build/debug/ruda run tests/post-preview/cp19-resource-limits --max-heap-bytes 1048576` | 0 | exact success stdout |
| `build/debug/ruda test tests/post-preview/cp19-resource-limits --engine both --max-heap-bytes 1048576` | 0 | 2/2 fresh VM/native tests |
| `build/debug/ruda build tests/post-preview/cp19-resource-limits --max-heap-bytes 1048576 --output build/debug/cp19-resource-limits` | 0 | native artifact created with embedded budget |
| `build/debug/cp19-resource-limits` | 0 | exact success stdout |
| one-byte VM/JSON/native differential probe | 70 | empty stdout; exact VM/native stderr and class |
| heap-related environment override probe without CLI option | 0 | unchanged default; exact success stdout |
| `ctest --preset debug -R '^ruda_c_unit$' --output-on-failure` | 0 | 1/1 passed in 210.01 seconds before the final aggregate tail |
| `cmake --build --preset debug --parallel` | 0 | final Debug tree built |
| `ctest --preset debug --output-on-failure` | 0 | 2/2 passed in 227.33 seconds; C-unit 225.44 seconds |
| `cmake --build --preset release --parallel` | 0 | final Release tree built |
| `ctest --preset release --output-on-failure` | 0 | 2/2 passed in 222.81 seconds; C-unit 220.99 seconds |
| `cmake --build --preset asan-ubsan --parallel` | 0 | final ASan/UBSan tree built |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | 2/2 passed in 335.31 seconds; C-unit 332.40 seconds |
| `./tools/check-architecture` | 0 | owner/dependency/line-budget policy clean |
| `./tools/check-c-memory-policy` | 0 | central allocator policy clean |
| `./tools/check-generated` | 0 | generated bytecode/diagnostic/Unicode/syntax artifacts current |
| `./tools/check-doc-links` | 0 | evidence/spec/roadmap links valid |
| `./tools/check-spec-terms` | 0 | normative terminology and D055 index clean |
| `./tools/check-syntax-coverage` | 0 | all 75 syntax kinds and 116 grammar productions covered |
| `./tools/check-example-syntax` | 0 | user-facing Ruda examples accepted |
| `git diff --check` | 0 | no whitespace errors |

This local debug/release/sanitizer table closes the checkpoint implementation
and its required local validation tail. Cross-platform Tier 1 CI remains the
governing gate for changing I3–I5 from `in-progress`.
