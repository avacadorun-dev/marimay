# Checkpoint 36: ordered Map, Set, and iterators

Date: 2026-08-30.

Status: verified local checkpoint evidence.

Verified subject: source base `96189371146fe05f07b2b7caa6b28c4ae381a1b4`
plus the single CP36 change described here. The user separately authorized the
checkpoint commit and push. No tag, publication, external identity operation,
parent product-gate claim, or cross-platform claim is authorized by this
report.

Host: macOS arm64 (macOS 26.3.1), CMake 4.4.2, Apple Clang 17.0.0.

## Authority and delivered contract

Accepted [D079](../../design/decisions.md#d079-ordered-map-set-and-checked-hash-key-coherence)
and the corresponding normative changes in
[language](../../spec/language.md),
[standard library](../../spec/standard-library.md),
[execution](../../spec/execution.md), and
[diagnostics](../../spec/diagnostics.md) define this boundary:

- compiler-known `Map<K,V>` and `Set<T>` are stable managed reference types;
- `mapOf`, `setOf`, `length`, contains, get, put/add, remove, and clear have
  exact free/dot-call-equivalent signatures, with typed `Option`, `Result`,
  and `CollectionGrowthError` results;
- iteration is insertion ordered, updates retain position, and remove followed
  by reinsert appends; hash seeds, buckets, tombstones, and capacity are not
  observable;
- the supported closed key set is bool, exact-width integers, rune, counted
  UTF-8 string, and immutable nominal records/enums with the complete
  `@derive(Eq, Hash)` pair and recursively eligible payloads;
- every committed mutation follows D079 generation rules. A generation change
  during iteration panics with exact G4005 code, message, and loop-header span;
- growth and allocation failures are failure-atomic, tracing is precise, and
  managed capacity remains bounded by the existing execution resource policy.

No arbitrary user hashing, partial derive pair, floating-point key, mutable or
unconstrained key, concurrent collection, weak reference, custom allocator, or
capacity-inspection surface is added.

## Implementation and ownership

Four focused checker owners recognize collection types, validate the closed
derive/key rules, type and lower operations, and construct insertion-order loop
CFGs. Typed MIR and its independent execution oracle carry collection layouts
and operations without backend semantic repair. Bytecode adds an append-only
collection-layout table and opcodes 141-152 with encode/decode, hostile
verification, and managed VM execution. The portable C17 emitter consumes the
same verified MIR and uses the same runtime descriptors and generation checks.

`runtime/common/managed_map.c` owns SipHash-based coherent equality/hash,
insertion order, growth, mutation generation, and tracing. The narrow Set owner
delegates storage to that exact implementation. `bootstrap/src/base/hash.c` is
included in both native runtime archives, which restores the CP29-CP35 native
link path that would otherwise fail on `_mmay_hash_sip24`. No architecture
exception was added, and all focused owners remain below their hard ceilings.

## Test inventory and exact observations

Focused C and public tests cover:

- empty, update, remove, reinsert, clear, duplicate, absent, length-limit, and
  64/96-entry growth cases, including dual injected hash seeds with identical
  visible order;
- bool, every signed and unsigned integer width, rune, counted UTF-8 string
  keys containing an interior NUL, imported derived records, and derived enums;
  equal nominal values created as three distinct objects resolve coherently;
- rejected f64, mutable nominal, incomplete derive, unconstrained generic, and
  uninferred generic key forms with exact typed diagnostics;
- map and set iteration, duplicate no-op generation, update invalidation,
  remove/reinsert position, empty-clear stability, and exact map/set G4005
  panic messages and spans in the MIR oracle, bytecode VM, and native C17;
- every collection MIR instruction, every collection opcode, invalid layout
  mutations, program encode/decode round trip, and VM/native result parity;
- precise tracing across nested cyclic Map graphs, deterministic randomized
  two-component graphs, collect-before-every-allocation, and reclamation after
  removing the final root;
- every reachable deterministic checker, MIR oracle, VM, native emitter,
  managed Map, and managed Set allocation-failure index. Failed insertions
  retain length and generation; after teardown every allocator-backed sweep
  asserts both `live_allocations == 0` and `live_bytes == 0`;
- formatter snapshot and public package-boundary acceptance. The public corpus
  passes three tests under both engines and the separately built Release
  artifact exits successfully with empty output.

The supplied CP29-CP35 regression audit was rerun against the rebuilt tree:
the full debug, Release, and ASan/UBSan suites include all CP29-CP35 native and
process contracts and passed. The stale implemented-surface statement in
`docs/known-limitations.md` now lists CP31-CP36 as implemented and records the
still-open equality, general derive, test-helper, and I0001 precision gaps
without presenting them as CP36 delivery.

## Sustained fuzz and validation

The applicable ASan/UBSan bytecode libFuzzer campaign completed its required
600-second threshold without a crash, timeout, sanitizer report, or saved
failure artifact: 30,846,704 executions in 601 seconds, final `cov: 5439`,
`ft: 16904`, and an in-memory corpus of 1,606 inputs / 189 KiB. The campaign
entered the new collection layout and verifier owners directly.

The final local validation passed on the same production tree. After the full
suites, the strengthened final CP36 test owner was rebuilt and rerun in all
three presets.

| Command | Result |
|---|---|
| CP36 public `fmt --check`, `check`, `run`, `test --engine both`, Release native build and artifact run | passed; 3/3 public tests and empty program output |
| `ctest --preset debug --output-on-failure` | 38/38 passed in 238.04 s; final CP36 focused rerun passed 2/2 in 17.17 s |
| `ctest --preset release --output-on-failure` | 38/38 passed in 284.24 s; final CP36 focused rerun passed 2/2 in 9.60 s |
| `ctest --preset asan-ubsan --output-on-failure` | 38/38 passed in 1011.41 s; final CP36 focused rerun passed 2/2 in 51.36 s |
| roadmap, architecture, C-memory-policy, generated-file, doc-link, spec-term, example, syntax-coverage, release-contract, and diff checks | passed |

Final artifact SHA-256 values:

- Debug compiler: `6ce3fb2be615cb5b25bf13a16e246ccaeea64c8aa45dce0971d9161146a7da88`;
- Release compiler: `14309da6093af591dad90b1bfdc6051f3387722f34c7e9dae1e96d47c8d70910`;
- ASan/UBSan compiler: `26abf7c0c934851a91b63b458d9ea6ea956ef6de4838583b2be782ae7f599280`;
- public CP36 Release artifact: `4f4df11b3cf7fb3467c1ab0a4dc4b3de50195b511f0e4bb137d049ce3211a0b5`.

## Validation scope and remaining limitations

This is local macOS arm64 evidence only. Apple sanitizer success does not
substitute for Linux LeakSanitizer or the complete claimed Tier 1 matrix. The
closed derive pair exists only for coherent collection keys; general
`@derive(Debug)`, standalone derived equality, ordinary `==`/`!=` for
string/enum/Option, the full specified `expect` helper surface, and precise
construct-specific I0001 diagnostics remain open and are now explicit in
`docs/known-limitations.md`. Concurrent collections, weak keys, custom hashing,
and capacity observation remain outside CP36.

CP36 therefore verifies only its local checkpoint scope. CLI0 and all parent
product gates retain their prior states. CP37 becomes the sole ready checkpoint.
