# Checkpoint 32: bounded typed standard streams and flush

Date: 2026-08-25–26.

Status: verified local checkpoint evidence.

Verified subject: source base `aa6b5d8e14484936a06ab562b22e173ec688911c`
plus the single CP32 change described here. No commit, push, tag,
publication, external identity operation, or broader CLI0/product-gate claim
is authorized by this report.

Host: macOS arm64 (`Darwin 25.3.0`, macOS 26.3.1), CMake 4.4.2,
Apple Clang 17.0.0.

## Authority and delivered contract

Accepted [D075](../../design/decisions.md#d075-bounded-typed-standard-streams)
and the corresponding normative changes in
[standard library](../../spec/standard-library.md),
[execution](../../spec/execution.md), and
[toolchain](../../spec/toolchain.md) define this boundary:

- `std/io` exposes typed `readLine`, `print`, `println`, `eprint`, `eprintln`,
  `flushStdout`, and `flushStderr`; the earlier planning phrase
  “read-to-limit” did not add an API absent from accepted D075;
- `readLine` accepts `0..1048576` returned bytes, distinguishes EOF-before-
  bytes from an unterminated final line, strips LF/CRLF, retains NUL, and
  rejects invalid UTF-8 without publishing partial text;
- a CRLF terminator does not consume the returned-byte limit, while a CR at
  EOF remains data; invalid signed limits preserve the requested value and
  precede the `io.stdin` capability check;
- stdout and stderr remain independent ordered byte streams; print operations
  never flush implicitly, and empty writes are capability-checked no-ops;
- interrupted zero-progress transfers receive at most 1024 retries after the
  initial attempt; positive short writes reset that budget and complete in
  order;
- late write/flush failure may leave a prefix visible, but typed errors expose
  neither a byte count nor host error text; allocation/resource failures stay
  classified runtime outcomes rather than invented `std/io.Error` variants;
- `io.stdin`, `io.stdout`, and `io.stderr` are exact separate capabilities,
  and compiler/tool diagnostics are not program stream bytes.

The requirements projection refines EXEC-021 and STD-011 from D046's
stdout-only preview to D075's complete bounded stream boundary.

## Implementation and ownership

The runtime now uses a typed transfer result (`OK`, `END`, `RETRY`, `FAILED`)
for reads, writes, and flushes. `runtime/common/io.c` owns retry bounds,
short-write completion, UTF-8/line handling, and error classification. The VM,
MIR oracle, and native C17 helpers consume that same contract; verifier owners
reject hostile operation tags and incorrect exact `Result`/`Option`/`Error`
layouts rather than repairing them in a backend.

`bootstrap/src/check/checker_expr_io.c` recognizes the exact imported D075
symbols and lowers canonical print operations to typed MIR write operations.
The bytecode VM receives independent stdin/stdout/stderr callbacks. CLI run and
test adapters keep the two outputs separate and use explicit flush callbacks.
Generated C17 uses counted `fread`/`fwrite`/`fflush` adapters with interrupted
operation retry. On POSIX, both `mmay` and a generated executable ignore
`SIGPIPE` at host startup so a closed reader reaches typed `WriteFailed` or
`FlushFailed` instead of terminating the process.

The process-capture API accepts exact counted stdin, including NUL and invalid
UTF-8. POSIX uses an unlinked seekable temporary file so a large input cannot
deadlock against output capture; Windows uses an inherited delete-on-close
temporary handle. Both keep stdout and stderr on separate bounded pipes.

## Test inventory and exact observations

Focused runtime and vertical tests cover:

- empty input, empty line, EOF before bytes and after data, LF, exact-limit
  CRLF, CR at EOF, embedded NUL, invalid UTF-8, zero and maximum limits;
- partial transfers, interrupted retries, retry exhaustion, malformed
  zero-progress success, denied capabilities, injected read/write/flush
  failures, and independent stdout/stderr ordering;
- checker success plus arity/type/type-argument negatives; typed MIR oracle,
  invalid MIR operation/Option metadata, bytecode round trip, hostile bytecode
  layouts/operations, and VM execution;
- exact `CapabilityDenied.capability` payloads in VM and generated native C17
  for all three names, compiling native cases at O0 and O2 with each grant
  disabled independently;
- every reachable deterministic allocation failure in the stream runtime and
  VM path, with `live_allocations == 0` and `live_bytes == 0` after teardown;
- exact binary process stdin and three-field VM/native process comparison for
  CRLF, EOF, invalid UTF-8, line limit, and a closed pipe reader.

For input `hello\r\n`, both `mmay run` and the Release native artifact produced
exact stdout `out line\nhello\n`, stderr `err line\n`, and status 0. EOF
substituted `<eof>`. Invalid UTF-8 and a 33-byte line returned status 1 with
exact `G4003` messages and empty program stdout. With the stdout pipe reader
closed, both engines returned status 1 and stderr:

```text
err line
error[G4003]: standard stream flush failed
```

They were not reported as signaled processes. A PTY-hosted output smoke and
ordinary pipe/file redirection completed on the local host. `mmay test
--engine both` passed four public CP32 tests while keeping stderr outside
`expect.output`.

## Validation scope and limitations

The final local validation passed on the same tree:

| Command | Result |
|---|---|
| `ctest --preset debug --output-on-failure` | 7/7 passed in 274.28 s; full C unit 264.23 s, focused CP32 unit 4.87 s |
| `ctest --preset release --output-on-failure` | 7/7 passed in 245.58 s; full C unit 235.93 s, focused CP32 unit 4.73 s |
| `ctest --preset asan-ubsan --output-on-failure` | 7/7 passed; full C unit 471.72 s, focused CP32 unit 7.12 s |
| public/example `fmt --check`, `check`, `test --engine both`, Release native build, VM/native pipe/file run, and PTY output smoke | passed |
| roadmap, architecture, C-memory-policy, generated-file, doc-link, spec-term, example, syntax-coverage, release-contract, and diff checks | passed |

Final artifact SHA-256 values:

- Debug compiler: `0fcad696aea5cd46d621cd09440f3759a63319acc54e6192ce3c5a0adbbac5e8`;
- Release compiler: `8165c34de0c6bb6c4ef681ea6105595e96900eaf3bcf9e69484de56a9522ca22`;
- public CP32 Release artifact: `b1c01711c41ccd8e9a66122842a6f1cf15e29895ad833da9ca81a7eb70468682`.

The process-capture implementation is single-threaded and adds no shared
capture/synchronization state, so the CP32 conditional TSan trigger did not
apply. The Windows owner is conditionally compiled and source-covered, but
this macOS report is not a Windows execution claim. Apple sanitizer success
does not substitute for Linux LeakSanitizer or the complete Tier 1 matrix.

CP32 therefore verifies only its local checkpoint scope. CLI0 and all parent
product gates retain their prior states. The blocking adapter remains an I6
removal gate, binary stream APIs remain CP33 work, and CP33 becomes the sole
ready checkpoint.
