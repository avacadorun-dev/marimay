# CP42: exact function values and closure lifetime

D098 implements ordinary function references and explicitly annotated closures
as exact function values. They can be passed, returned, rebound, and stored in
arrays, records, Map and Option. Calls evaluate the callee once before arguments
from left to right. Const captures retain values and reference identity; let
captures share managed cells after their declaring function returns. Loop
iterations create fresh bindings, and nested closures retain transitive captures.

The checker converts closures to one-method typed MIR interfaces with private
managed environment records. Named functions get an empty-environment adapter.
An entry prologue dominates capture uses even when their first source reference
is inside a branch. Initialized mutable captures use direct record cells;
initially uninitialized declarations use an Option payload until assignment.
Each body has its own return, loop and defer scope. No engine reconstructs
lexical captures, and no new MIR opcode or wire format is needed.

Void interface calls use the VM's existing discarded-return path. Never-returning
values retain their source type and use a private result slot plus an unreachable
return guard. An unused higher-order function may have an empty closed-world
construction set; every actual construction still has exact signature checks,
and the VM accepts only program-owned tables. Synthetic source-origin metadata
preserves precise failing expectation locations in all test engines.

## Verification

The final local Release suite passed **79/79** in 328.09 s, including source
packaging; the final macOS ASan/UBSan suite passed **78/78** in 706.30 s.
The sanitizer preset excludes the Release-only packaging test. Architecture,
roadmap and C memory-policy gates pass.

The machine-readable [packet](cp42-function-values-2026-09-08.json) records the
final local commands, results and source/log hashes. This is local macOS arm64
evidence, not a completed Tier 1 matrix or Linux LeakSanitizer claim.

- Fifteen public tests cover qualified/renamed and nominal-overloaded references,
  generic enclosing instances, shared escaping/transitive cells, fresh range and
  array-loop bindings, record/container storage, shared reference identity,
  conditional initialization, lexical shadowing, evaluation order and rebinding,
  compound assignment, void bodies, defer capture timing and managed cycles.
- The source process test checks formatting, checking, VM/native test parity,
  native Debug/Release executables, panic identity and deferred output. A failing
  expectation in a closure must report `failure_test.my:3:27-3:45` in each engine;
  missing preparation metadata cannot become a silent successful test run.
- The checker covers wrong signatures, arity, annotation omissions, initialization,
  const assignment, duplicate parameters, missing returns, cross-body loop control,
  unsupported generic/unsafe references, seven resource ceilings and successive
  type-interning limits through successful compilation. Imported
  unsafe references preserve the same rejection as local ones.
- Allocation-failure sweeps cover checker success and rejection, graph imports,
  MIR interpretation, lowering, bytecode encoding/decoding, VM execution, native
  emission and test-location preparation. VM and generated native harnesses force
  collection before allocations, including shared and cyclic environments.
  Every sweep reaches success/rejection and checks both `live_allocations == 0`
  and `live_bytes == 0` after teardown. Hostile method slots and payloads are rejected.

Notes' sixteen source tests and config's seven source tests also pass with both
engines on the changed compiler. These runs do not read the private Notes database
or establish a new performance baseline. The new frontend/formatter corpus seed
passes fixed-input replay in both sanitizer-instrumented libFuzzer targets;
this replay is not a fuzz campaign. The ignored local Clang resource links were
refreshed after a toolchain update left them pointing at the removed version 17.

## Remaining gate and next consumer

The nonfatal warning required by language §6.3 for an escaping mutable capture
is still absent. D098 and CP42 explicitly require diagnostic severity, escape
classification and positive/negative warning tests before full closure conformance
or callback acceptance. Legal escaping closures remain legal. The conservative
name-based storage plan is isolated in the cell owner and bounded by a per-function
work ceiling; it may allocate an unnecessary cell under shadowing.

Predicate `findIndex` and the Notes CLI search command remain unimplemented;
Notes' existing ID lookup still uses its manual loop. Their next decision
must settle generation checks before/after callbacks and original-panic precedence.
General contextual annotation omission, uninstantiated generic/unsafe/extern
references, suspension and callback FFI are outside this slice. Result.mapError
retains D094's bounded mapper forms; adapting that consumer to stored function
values is also still open. CP42 remains the
sole in-progress checkpoint; CLI0/CP41/CP39/CP40 and application/platform gates
are not closed by this packet. No new performance gain is claimed.

No idea from the Vladimirov book was used for this change; no page citation is
claimed. The implementation follows the existing language and MIR contracts.
