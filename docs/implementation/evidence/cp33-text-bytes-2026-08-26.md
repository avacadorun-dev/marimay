# Checkpoint 33: owned UTF-8 text, Bytes, and bounded builders

Date: 2026-08-26.

Status: verified local checkpoint evidence.

Verified subject: source base `569c29dc6fe6cdd2450c63cf1da16cd17cb983bc`
plus the single CP33 change described here. No commit, push, tag,
publication, external identity operation, parent product-gate claim, or
cross-platform claim is authorized by this report.

Host: macOS arm64 (`Darwin 25.3.0`, macOS 26.3.1), CMake 4.4.2,
Apple Clang 17.0.0.

## Authority and delivered contract

Accepted [D076](../../design/decisions.md#d076-owned-utf-8-views-nominal-bytes-and-bounded-builders)
and the corresponding normative changes in
[language](../../spec/language.md),
[standard library](../../spec/standard-library.md),
[execution](../../spec/execution.md),
[diagnostics](../../spec/diagnostics.md), and
[toolchain](../../spec/toolchain.md) define this boundary:

- `std/text` exposes counted byte and Unicode-scalar lengths, alpha grapheme
  copies, scalar-boundary substrings, exact-byte contains/prefix/suffix,
  split/join, and ASCII-only trim. Returned strings and arrays are owned;
  integer string indexing remains invalid.
- The alpha grapheme policy keeps CRLF together and attaches Unicode 17
  zero-width scalars to the preceding cluster. It is deliberately not a claim
  of complete UAX #29 segmentation, normalization, or locale behavior.
- `std/bytes.Bytes` is nominal managed identity over private `[]byte` storage.
  Assignment aliases, while `fromArray`, `toArray`, `copy`, and `slice` own
  independent storage. Mutation uses the D053 generation contract, and a
  stale iterator produces exact `G4005`.
- `encodeUtf8` and `decodeUtf8` are explicit counted copies. Invalid input
  reports the first byte offset; embedded NUL is ordinary data and no
  replacement or implicit text/byte conversion exists.
- `std/text.Builder` and `std/bytes.Builder` accept limits in `0..1048576`,
  append failure-atomically, and return owned snapshots. Split accepts at most
  65536 parts; source tests exercise both exact maximums and maximum plus one.
- D075 `std/io` now includes bounded `readBytes`, `writeBytes`, and
  `ewriteBytes`. Reads publish no prefix on `InputTooLong`; binary output uses
  the exact stream capability and the existing partial-write/retry order.

The requirements projection adds the CP33 text/Bytes/builder and binary-stream
contracts without raising I1-I5, CLI0, or any public-release state.

## Implementation and ownership

`bootstrap/src/base/text.c` owns the allocation-free counted UTF-8 primitives.
Focused checker owners recognize only exact imported `std/text`, `std/bytes`,
and `std/io` symbols. Typed MIR carries a closed text-operation enum; dedicated
MIR verifier/oracle, bytecode lowering/verifier/VM, and native C17 emitter
owners consume that same operation set. Bytecode opcodes 108-127 are generated
from the canonical table and reject malformed registers, types, operation
tags, and managed-array layouts before execution.

The package checker now includes already-loaded dependency record/enum layouts
in imported nominal ABI closure without exposing their names at the source
lookup boundary. This is required for `std/io.readBytes` to return the public
`std/bytes.Bytes` nominal through a transitive package dependency. The native
array mutation emitter also retains generation checks across calls; eliding a
check from immutable MIR shape alone was invalid because a called function may
mutate an aliased array before iterator use.

To preserve the architecture ceiling, text operations have narrow public and
owner-private contracts, bytecode terminator verification is independently
compiled, and MIR/bytecode/native text owners are separate translation units.
`tools/check-architecture` accepts the resulting ownership graph with no
registered exception.

## Test inventory and exact observations

Focused C and public tests cover:

- 512 deterministic randomized valid UTF-8 token sequences containing ASCII,
  NUL, multibyte scalars, combining marks, emoji, CR, and LF; each sequence
  checks every byte boundary, rune count, grapheme partition, and owned engine
  result. Hostile continuation, overlong, surrogate, and out-of-range encodings
  are rejected at the first byte.
- All 18 MIR text operations, every corresponding bytecode target, bytecode
  encode/decode, malformed operation/type/register/array cases, and identical
  MIR-oracle/VM/native result bytes.
- Empty values, exact maxima and maxima plus one, boundary ordering and
  multibyte offsets, trailing split empties, CRLF/combining graphemes, owned
  snapshots, alias/copy/slice/self-append, reserve/clear, invalid UTF-8, and
  embedded NUL.
- Every reachable deterministic allocation failure in the changed runtime-I/O,
  MIR-oracle, MIR-to-bytecode lowering, VM, and native-emitter APIs. Every
  teardown asserts `live_allocations == 0` and `live_bytes == 0`. Generated C
  additionally runs with collect-before-allocation and allocation failures 0
  and 1 at O0 and O2.
- The broad CLI allocation tests first measure a successful path (10231
  attempts for CP31 run and 6791 for clean check on this host), keep their
  historical 4096/2048 orchestration prefixes, and sample the late
  program-argument allocation phase. Exhaustive CP33 owner sweeps remain
  separate so the complete C unit stays inside its configured timeout.

The public CP33 corpus passed six tests under both engines and printed exact
`cp33\n` from both VM and the Release native artifact. Process-level VM/native
comparison observed:

| Input | Status | Exact stdout | Exact stderr |
|---|---:|---|---|
| `abc` | 0 | `61 62 63` | empty |
| `12345678` | 0 | `31 32 33 34 35 36 37 38` | empty |
| `123456789` with limit 8 | 1 | empty | `error[G4003]: standard input exceeds 8-byte limit\n` |
| `41 ff 42` | 0 | `41 ff 42` | empty |
| mutate then advance `Bytes.Iterator` | 1 | empty | `panic[G4005]: array iterator invalidated by mutation\n` |

## Sustained fuzz and validation

The applicable ASan/UBSan libFuzzer campaigns completed their required 600
seconds without a crash, timeout, sanitizer report, or CP33 artifact:

- frontend: 1,956,627 executions in 601 seconds;
- bytecode: 42,751,006 executions in 601 seconds, final `cov: 7152`,
  `ft: 23202`, corpus 1591 inputs / 122 KiB. The campaign entered the new
  `bytecode_verify_text.c` owners directly.

The final local validation passed on the same tree:

| Command | Result |
|---|---|
| CP33 public `fmt --check`, `check`, `run`, `test --engine both`, Release native build and artifact run | passed; 6/6 public tests and exact `cp33\n` |
| `ctest --preset debug --output-on-failure` | 9/9 passed in 631.71 s; full C unit 535.91 s, focused CP33 unit 4.76 s, process parity 26.00 s |
| `ctest --preset release --output-on-failure` | 9/9 passed in 384.89 s; full C unit 344.73 s, focused CP33 unit 4.91 s, process parity 8.65 s |
| `ctest --preset asan-ubsan --output-on-failure` | 9/9 passed in 2126.53 s; full C unit 1744.48 s, focused CP33 unit 5.04 s, process parity 114.05 s |
| roadmap, architecture, C-memory-policy, generated-file, doc-link, spec-term, example, syntax-coverage, release-contract, and diff checks | passed |

Final artifact SHA-256 values:

- Debug compiler: `54fe4f1a1404076b2b8de57e92020f6839b7e2a58eca02c485327bfd642040fb`;
- Release compiler: `bcf6b8bc0aaae6fbdb960fcfeee1e3f7816bdecf2bb352f5e7831e616a40d7c1`;
- public CP33 Release artifact: `7fce2d44ee89a8291ec939e0619e3382ae7a6f8f9d947bc9ecd0c0a17a9094f1`.

## Validation scope and remaining limitations

This is local macOS arm64 evidence only. Conditional Windows owners are source-
covered but not executed here; Apple sanitizer success does not substitute for
Linux LeakSanitizer or the complete Tier 1 matrix. No borrowed string/byte view,
normalization, full grapheme algorithm, regex, locale behavior, or unbounded
builder was added. Standard streams remain synchronous blocking adapters until
I6, and path/filesystem behavior remains CP34 work.

CP33 therefore verifies only its local checkpoint scope. CLI0 and all parent
product gates retain their prior states. CP34 becomes the sole ready checkpoint.
