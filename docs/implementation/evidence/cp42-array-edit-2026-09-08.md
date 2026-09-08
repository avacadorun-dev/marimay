# CP42: shallow array copy, stable removal and Notes migration

D097 implements `copy(values): []T` and `removeAt(values, index): Option<T>`
with exact free/dot receiver selection. Copy allocates an independent outer
array at exact length; reference elements retain their identity. Removal
preserves order, clears the vacated slot and commits one generation increment.
An absent index returns None without mutation. Source lowering constructs
Some before the allocation-free removal instruction, so failure to allocate
the result cannot leave a partially changed array.

The focused checker and native-emission owners use shared typed MIR operations.
Managed storage owns source rooting, exact capacity/accounting and publication;
managed mutation owns the checked stable commit. App loops were not moved into
the compiler. Predicate search and its callback/lifetime contract remain open.

## Correctness evidence

The local Release suite's 78 tests all have passing results (focused/tail and
two corrected reruns below); full macOS ASan/UBSan passed 77/77 in 755.37 s.
The sanitizer preset omits the separate source-packaging test that passed in
Release. This is not Linux LeakSanitizer or a Tier 1 matrix. The results and
source/log hashes are recorded in the
[raw packet](cp42-array-edit-2026-09-08.json). This change adds:

- Runtime tests for all five dense raw/reference layouts, empty and nonempty
  copy, exact capacity, shared children, stable removal, slot clearing,
  generation exhaustion, signed bounds, invalid handles/layouts/children,
  object/array/byte limits and all reachable deterministic allocation failures.
  Forced GC verifies that copy roots its source and transitive children.
- Checker rejection and allocation sweeps for free/dot/generic calls, arity,
  exact index and result types, ordinary-function collisions and never input.
  MIR inspection requires Some construction immediately before the mutation.
- MIR/wire round trips, hostile operands and invalidated bounds proofs; MIR
  and VM execution allocation sweeps; native Debug/Release exact bounds panics.
  A generated-C caller forces GC and injects every reachable allocation failure
  while retaining an input reference array, checking unchanged contents and
  generation on failure and exact successful removal. Teardown checks both
  `live_allocations == 0` and `live_bytes == 0`.
- Nine public source tests, formatter/check/test/run/build commands and exact
  iterator-invalidation panic plus deferred cleanup on VM/native Debug/Release.

Release verification covered all 78 registered tests: the four focused groups
passed, and the remaining 74 initially reported two failures. The opcode-model
expectation omitted the two newly appended entries; its exact metadata table
was updated and the bytecode group passed. Source packaging excludes untracked
files, so its first archive omitted the new owners; after staging those files,
the unchanged packaging check passed. The other 72 tail tests passed. A final
formatting-only cleanup of MIR predicates left the Release compiler SHA-256
unchanged from the paired measurement and focused run.

Config regression also passes: seven source tests, 32 process cases and
23 filesystem cases, each across the prescribed VM/native modes. These are
correctness runs under concurrent verification, not new performance baselines.

## Notes: one model for add and remove

Notes now uses `copy` for addition and `copy` plus `removeAt` for removal.
`remove` returns `Result<Database, string>` and validates its input; the caller
saves the returned database. Existing outer-array aliases remain unchanged,
surviving records and tags remain shared, ordering and nextId are preserved,
and missing IDs remain explicit errors. This is shallow copying, not a deeply
immutable snapshot. The manual search loop remains until the callback contract
is implemented. CLI commands, JSON bytes and Russian output remain unchanged.

Sixteen source tests pass with both engines. The process adapter retains all
27 cases across VM/native Debug/Release plus 15 closed-pipe checks, including
exact file effects and output failures after a committed write. Existing dirty
work in the external app repositories is not automatically committed; the raw
packet captures only the explicit safe source allowlist. No private notes
database was read or copied.

## Paired cost on a near-limit input

Before changing Notes, the test froze 6,098 synthetic Unicode notes in
1,047,786 bytes, SHA-256
`2c48707982e285d3253064a0c886224e5685b16aa07fcf4e8e353b84f6d2a3ad`.
Both app versions use the same compiler binary and input bytes. Each command
restores the original file, runs one excluded warm-up and three timed processes,
and checks exact stdout/stderr/status and resulting JSON. Build time is excluded;
each timed process includes parsing, validation, editing, encoding and atomic
write. `/usr/bin/time -l` records process peak RSS on macOS arm64. The two batches
ran before concurrent test/build work; this is a small sequential paired sample,
not cold-start acceptance or a statistically established cross-platform gain.

| Operation | VM median, before → after | Native Debug median | Native Release median | Native Release median peak RSS |
|---|---:|---:|---:|---:|
| Add | 5757.01 → 5681.81 ms | 1437.35 → 1425.45 ms | 239.89 → 243.72 ms | 70.95 → 71.20 MiB |
| Remove first | 5768.77 → 5711.45 ms | 2323.38 → 1486.86 ms | 420.26 → 243.29 ms | 70.25 → 71.02 MiB |
| Remove middle | 5684.87 → 5758.91 ms | 1854.02 → 1436.18 ms | 329.01 → 251.40 ms | 70.22 → 70.98 MiB |
| Remove last | 5855.36 → 5772.39 ms | 1413.71 → 1421.47 ms | 238.31 → 239.09 ms | 70.22 → 70.98 MiB |

In this sample, native Release first/middle removal costs fell by 42.1%/23.6%
despite retaining the input array and validating the new operation's input.
Functional removal increases observed peak RSS by about 0.76 MiB; that
aggregate includes the copied array and the additional input validation.
Add and last removal show no material improvement; VM totals stay around
5.7 seconds. These end-to-end numbers do not locate the remaining cost in a
particular subsystem. Profiling that cost and named application budgets remain
required; this dataset does not replace an existing acceptance dataset/budget.

## Remaining gates

CP42 stays the sole in-progress checkpoint. Notes search, resource boundaries,
performance/claimed-platform evidence, all ten apps, CP41, CP39, CP40 and N0
remain open. No Tier 1-complete or pushed-D097 sanitizer result is claimed by
local tests. Run 216 on the earlier cache-install fix passed Linux sanitizer
76/76; that is evidence for its own revision, not this implementation.

No new idea from the Vladimirov book was used here. The sources were the
repository's accepted contracts, focused owners and application behavior.
