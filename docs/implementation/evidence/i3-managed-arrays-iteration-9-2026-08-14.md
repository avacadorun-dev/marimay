# I3 managed arrays — iteration 9 — 2026-08-14

Gate: I3 typed-bytecode VM vertical slice, with I2 D045 verifier hardening and
the existing I5 managed heap. Iteration status: complete; milestone gates
remain `in-progress`.

Baseline: `5c23c38` (`refactor: extract checker result ownership`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: accepted [D045](../../design/decisions.md), the normative
[language](../../spec/language.md), [execution](../../spec/execution.md), and
[diagnostic](../../spec/diagnostics.md) contracts. This checkpoint implements
those contracts for an internal matrix-critical backend subset; it does not
change source semantics.

## Delivered slice

- The append-only bytecode registry adds exact `Array<f64>` and
  `Array<Array<f64>>` types plus `array_new`, `array_length`,
  `array_require_index`, `array_get`, and `array_set` opcodes. The codec has an
  explicit arrays feature bit and bounded construction/argument tables; the
  program envelope enforces cumulative array limits while decoding each
  function, before retaining an over-limit function.
- MIR lowering accepts only the two exact layouts needed by the matrix slice,
  preserves construction argument order, lowers every D045 pre-RHS bounds
  barrier, and emits `array_set` followed by `copy` so assignment result
  publication occurs only after a successful store. Other `Array<T>` layouts
  are rejected rather than erased to an unsafe generic tag.
- MIR and bytecode verifiers carry a must-dataflow checked-index capability.
  `array_set` is valid only through the exact carrier and index established by
  a dominating `array_require_index`. Rewriting either register invalidates the
  capability, and CFG joins retain only identical facts from every predecessor.
  This makes D045's `bounds check -> RHS -> store` order part of the verified
  execution boundary rather than a convention of one lowering path.
- The VM uses the existing execution-provenance managed heap and exact static
  descriptors. Every managed register in every active frame has a stable,
  separately allocated shadow-root location; callee results are published into
  the rooted parent before the callee root is removed. The defensive store-time
  bounds check remains even after verifier proof.
- Bounds failures publish exact locale-independent `G4002` text, the complete
  failing access span, and source-first program frames. `INT64_MIN` and
  `INT64_MAX` formatting is defined without signed overflow. Omitting panic
  materialization performs no diagnostic allocation.
- Returned arrays are execution-bound weak handles. A host retaining one
  across collection explicitly roots its value location with the public
  caller-owned `RudaManagedRootFrame`; no hidden singleton result root silently
  invalidates an earlier result.

## Verification coverage

- Codec round trip, disassembly, every relevant malformed type/register/table
  relation, feature-bit mismatch, decode allocation failure, and both hostile
  program aggregate array limits are pinned. Aggregate rejection leaves no
  initialized program and the tracking allocator returns to zero live
  allocations and bytes.
- Verifier negatives cover unchecked store, wrong carrier, wrong index,
  carrier overwrite, index overwrite, and conflicting CFG predecessors. A
  same-carrier/same-index branch join is accepted.
- Encoded-program execution proves that an invalid destination produces
  `G4002` before a `G4000` RHS callee can run, while a valid destination reaches
  that RHS. MIR-oracle and encoded-VM fixtures agree on successful mutation,
  exact f64 bits, and `G4002` code/message/span.
- Managed tests cover flat and nested construction, aliasing, collection before
  allocation, managed return across a call, exact descriptor rejection,
  populated foreign execution rejection, stale-after-dispose rejection, host
  root retention across collection, and staleness after root removal.
- Run and decode allocation failures check both `live_allocations == 0` and
  `live_bytes == 0`. Debug, optimized, ASan+UBSan, and TSan runs complete with
  no report on the covered paths.

## Internal removal gates and deferred work

- The two bytecode array tags are a deliberately bounded matrix checkpoint,
  not the general D045 `Array<T>` representation. Before I3 or I5 can be
  `verified`, replace them with a canonical serialized program type/layout
  table and execution-owned/interned runtime descriptors, then add arbitrary
  exact `Array<T>` codec/verifier/differential coverage.
- `push` remains deferred until its accepted
  `Result<void, ArrayGrowthError>` surface exists; adding growth also requires
  an explicit checked-index invalidation/version rule for alias-mutating calls.
- Source construction/index lowering, portable generated C17 arrays, native
  `G4002`, the rectangular loop-based f64 matrix fixture, and public
  `check`/`run`/`test`/`build` commands are not delivered here. This checkpoint
  therefore does not satisfy I3, I4, I5, or 0.1-dev acceptance by itself.
- Local AppleClang has no libFuzzer runtime, so no successful sanitizer fuzz
  campaign is claimed. Tier 1 CI results, including MSVC, remain external
  evidence rather than a local claim.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build build/debug --target ruda_c_unit -j2 && ctest --test-dir build/debug --output-on-failure` | 0 | Debug warnings-as-errors; 1/1 passed |
| `cmake --build build/release --target ruda_c_unit -j2 && ctest --test-dir build/release --output-on-failure` | 0 | optimized build; 1/1 passed |
| `cmake --build build/asan-ubsan --target ruda_c_unit -j2 && ctest --test-dir build/asan-ubsan --output-on-failure` | 0 | ASan+UBSan; 1/1 passed without report |
| `cmake --build build/tsan --target ruda_c_unit -j2 && ctest --test-dir build/tsan --output-on-failure` | 0 | TSan; 1/1 passed without report |
| `clang --analyze` over modified MIR, bytecode, VM, lowering, and tests | 0 | no diagnostics |
| `./tools/check-generated && ./tools/check-c-memory-policy` | 0 | generated registries current; production heap policy clean |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax` | 0 | documentation checks passed |
| `./tools/generate-bytecode --check && git diff --check` | 0 | bytecode registry current; patch whitespace clean |

Two independent final read-only audits reported no remaining P0/P1 findings on
the stabilized checkpoint. Native arrays and the full matrix acceptance remain
the nearest release risk.
