# Checkpoint 34: typed paths and capability-confined files

Date: 2026-08-29.

Status: verified local checkpoint evidence.

Verified subject: source base `e03bc771d5a79bc6ff210aa5d2b4f39e4ec74349`
plus the single CP34 change described here. No commit, push, tag,
publication, external identity operation, parent product-gate claim, or
cross-platform claim is authorized by this report.

Host: macOS arm64 (macOS 26.3.1), CMake 4.4.2, Apple Clang 17.0.0.

## Authority and delivered contract

Accepted [D077](../../design/decisions.md#d077-typed-paths-and-capability-confined-synchronous-files)
and the corresponding normative changes in
[language](../../spec/language.md),
[standard library](../../spec/standard-library.md),
[execution](../../spec/execution.md),
[diagnostics](../../spec/diagnostics.md), and
[toolchain](../../spec/toolchain.md) define this boundary:

- `std/path.Path` is an owned, validated UTF-8 lexical path. Construction and
  normalization reject NUL, empty absolute input where forbidden, platform
  prefix ambiguity, and escapes above a normalized root. Path operations do
  not query the host.
- `--allow-read PATH` and `--allow-write PATH` are separate, repeatable grants.
  Matching uses normalized absolute components rather than byte-prefix tests;
  a denied request does not reach the host adapter.
- `std/fs` exposes bounded text/byte reads, failure-atomic text/byte writes,
  explicit open/read/close handles, metadata, and directory enumeration.
  Whole-file limits are exact, invalid UTF-8 remains an error, directory
  entries are sorted by exact UTF-8 bytes, and handles are execution-local.
- Traversal rejects a symlink/reparse component instead of following it.
  Atomic writes use a same-directory exclusive temporary, sync, close, and
  replacement sequence; every failure before replacement preserves the old
  destination and removes the temporary file.
- Paths/files remain synchronous blocking operations. The accepted I6 removal
  gate later parks blocking adapters without changing these results.

The public surface keeps `Metadata` and `Entry` nominal, has no ambient
filesystem access, and does not expose host descriptors, permission mutation,
watchers, mmap, subprocesses, or toolchain-private path helpers.

## Implementation and ownership

`runtime/common/fs.c` owns lexical normalization, capability matching, handle
slots, bounded partial-I/O loops, failure-atomic publication, metadata, sorted
directories, and cleanup. Narrow POSIX and Windows platform owners supply file
operations and metadata without moving source semantics into an adapter.

Focused checker ownership recognizes only exact imported `std/path` and
`std/fs` functions. Typed MIR carries ten closed filesystem operations;
dedicated MIR verifier/oracle, bytecode lowering/verifier/VM, and native C17
emitter owners consume the same operations. Bytecode opcodes 128-137 are
generated from the canonical table and reject invalid operations, types,
registers, or nominal layouts before execution.

CLI capability parsing is shared by `run`, `test`, and `build`; it rejects
missing, empty, invalid, or excessive roots with exact diagnostics. Generated
native C embeds only the normalized granted roots and uses the same runtime
contract as the VM. Architecture ownership remains below every hard ceiling;
no architecture exception was added.

## Test inventory and exact observations

Focused C tests cover:

- relative/absolute normalization, `.` and `..`, Unicode and spaces, root and
  separator boundaries, byte-prefix escapes, distinct read/write grants, and
  the invariant that denied requests make zero host calls;
- missing, wrong-kind, symlink, invalid UTF-8, input limit, zero-progress,
  partial read/write, close, replacement, collision, and cleanup failures;
- reusable simultaneous-handle bounds, explicit close idempotence,
  best-effort close after a primary panic, and old-destination preservation at
  every atomic-write phase before replacement;
- metadata projection and deterministic directory order;
- every reachable deterministic allocation-failure index in initialization,
  whole-file read, atomic write, and directory enumeration. After each full
  teardown, both `live_allocations == 0` and `live_bytes == 0` are asserted;
- all ten MIR operations, exact checker signatures, hostile MIR operation and
  type/layout mutations, all ten hostile bytecode opcode mutations, and
  bytecode encode/decode verification.

The real temporary-filesystem fixture additionally covers paths containing
spaces and Unicode, repeated open/read/close, bounded text and byte reads,
atomic write/readback with no surviving temporary file, metadata, sorted
directory entries, missing and denied paths, wrong file kinds, invalid UTF-8,
and a real symlink on POSIX. VM and native output/status are byte-identical.
The cleanup fixture preserves exact `panic: cp34 primary` precedence while
closing the outstanding file in both engines.

CLI process tests cover repeatable roots plus missing, empty, invalid, excess,
and command-inapplicable capability options. The public corpus passes three
tests under both engines and prints exact `cp34\n` under its granted fixture
root.

## Validation

The final local validation passed on the same tree:

| Command | Result |
|---|---|
| CP34 public `fmt --check`, `check`, `test --engine both`, granted VM run, Release native build and artifact run | passed; 3/3 public tests and exact `cp34\n` |
| `ctest --preset debug --output-on-failure` | 34/34 passed in 216.47 s; CP34 unit and real process coverage included |
| `ctest --preset release --output-on-failure` | 34/34 passed in 212.42 s; CP34 unit and real process coverage included |
| `ctest --preset asan-ubsan --output-on-failure` | 34/34 passed in 786.27 s; CP34 process 110.32 s and focused unit 0.05 s |
| roadmap, architecture, C-memory-policy, generated-file, doc-link, spec-term, example, syntax-coverage, release-contract, and diff checks | passed |

Final artifact SHA-256 values:

- Debug compiler: `ebe6b94c9a2ca98145c1482bd306cb0b524651dd16ceee58fb30dfc5fc990434`;
- Release compiler: `74bc0adfdcffeccb78893212d563341f28a73006e89e7878934da1e32819e541`;
- public CP34 Release artifact: `8563f3b2458de2ae54f4170678aa1212c0614f659f2179209500c416d8e4702e`.

## Validation scope and remaining limitations

This is local macOS arm64 evidence only. The POSIX adapter and real symlink
behavior ran on this host; conditional Windows owners are source- and build-
covered but were not executed here. Apple sanitizer success does not substitute
for Linux LeakSanitizer or the complete claimed Tier 1 matrix. Network
filesystem replacement guarantees, concurrent hostile path-renaming races,
watchers, mmap, permission management, and asynchronous/non-blocking files are
outside CP34.

CP34 therefore verifies only its local checkpoint scope. CLI0 and all parent
product gates retain their prior states. CP35 becomes the sole ready checkpoint.
