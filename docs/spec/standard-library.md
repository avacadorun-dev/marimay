# Marimay standard-library contract — design baseline 0.1

Status: normative API direction and production requirements. Exact secondary dot-call sets are frozen during their roadmap gates, but packages, safety properties, and examples in this document are required for 1.0.

The standard library is distributed and versioned with the compiler/runtime. A toolchain cannot substitute an incompatible third-party `std`. Direct-style suspension, cancellation, capabilities, deterministic testing, and VM/native equivalence apply to every package.

## 1. API rules

- Expected failure returns `Result<T,E>`; absence returns `Option<T>`.
- Panic is reserved for violated caller/compiler/runtime invariants.
- Potentially slow I/O is direct-style and cancellation-responsive.
- Every allocating API documents ownership and bounds.
- Every resource has explicit close/finish behavior suitable for lexical `defer`.
- APIs do not accept or return null, ambient `any`, or unchecked untyped maps.
- Platform differences are represented by capabilities/results, not silent behavior changes.
- All collections, parsers, and protocols enforce configurable or documented limits.
- Package examples are compiled and tested under VM and native engines.

## 2. Prelude

The prelude is imported implicitly and intentionally small. It contains primitive aliases and the declarations/names needed for ordinary control flow:

```text
Option, Some, None
Result, Ok, Err
Error
ArrayGrowthError, ArrayTruncateError, CollectionGrowthError, Map, Set, Bytes
Iterator
Task
expect          # @test/@fuzz functions only
panic           # fn panic(message: string): never
```

It does not inject filesystem, network, environment, logging, JSON, reflection, process, or unsafe C APIs.

### 2.1 `Option<T>`

Required dot-call functions:

```marimay
public fn isSome<T>(option: Option<T>): bool;
public fn isNone<T>(option: Option<T>): bool;
public fn map<T, U>(option: Option<T>, mapper: (T) => U): Option<U>;
public fn flatMap<T, U>(option: Option<T>, mapper: (T) => Option<U>): Option<U>;
public fn valueOr<T>(option: Option<T>, fallback: T): T;
public fn valueOrElse<T>(option: Option<T>, fallback: () => T): T;
```

There is no unchecked `unwrap` in the default prelude. Tests and invariant-heavy code may use an explicitly named `expectSome(message)` that panics with a stable code and source span.

### 2.2 `Result<T,E>`

Required dot-call functions:

```marimay
public fn isOk<T, E>(result: Result<T, E>): bool;
public fn isErr<T, E>(result: Result<T, E>): bool;
public fn map<T, U, E>(result: Result<T, E>, mapper: (T) => U): Result<U, E>;
public fn flatMap<T, U, E>(
  result: Result<T, E>,
  mapper: (T) => Result<U, E>,
): Result<U, E>;
public fn mapError<T, E, F>(result: Result<T, E>, mapper: (E) => F): Result<T, F>;
public fn valueOr<T, E>(result: Result<T, E>, fallback: T): T;
```

`expectOk(message)` may panic and is not idiomatic expected-error handling. There is no implicit `From`-style conversion graph. A concrete error may be explicitly boxed into an `Error` interface at a construction or return boundary when it structurally conforms.

### 2.3 Error formatting

The minimum interface is:

```marimay
public interface Error {
  message(): string;
}
```

First-party error enums provide stable machine-readable cases in addition to `message`. `std/error` supplies wrappers for context and cause chains without requiring every domain enum to expose optional signatures:

```marimay
public fn context(error: Error, message: string): Error;
public fn causes(error: Error): Iterator<Error>;
public fn format(error: Error): string;
```

`?` does not automatically allocate or rewrite errors. Applications add domain context at boundaries where it is meaningful.

## 3. Text, bytes, conversion, and paths

`string` is immutable valid UTF-8. The executable CLI-alpha surface is:

```marimay
// std/text
public enum BoundaryError {
  OutOfBounds { start: int, end: int, length: int, },
  NotRuneBoundary { offset: int, },
}
public enum LimitError {
  InvalidLimit { limit: int, },
  LimitExceeded { limit: int, required: int, },
}
public enum SplitError {
  EmptySeparator,
  InvalidLimit { limit: int, },
  LimitExceeded { limit: int, required: int, },
}
public type Builder;
public fn byteLength(text: string): int;
public fn runeCount(text: string): int;
public fn runes(text: string, limit: int): Result<[]rune, LimitError>;
public fn graphemes(text: string, limit: int): Result<[]string, LimitError>;
public fn substring(text: string, start: int, end: int): Result<string, BoundaryError>;
public fn contains(text: string, needle: string): bool;
public fn startsWith(text: string, prefix: string): bool;
public fn endsWith(text: string, suffix: string): bool;
public fn split(text: string, separator: string, limit: int): Result<[]string, SplitError>;
public fn join(parts: []string, separator: string, limit: int): Result<string, LimitError>;
public fn trimAscii(text: string): string;
public fn newBuilder(limit: int): Result<Builder, LimitError>;
public fn append(builder: Builder, text: string): Result<void, LimitError>;
public fn finish(builder: Builder): string;
```

Byte offsets are zero-based. `substring` requires ordered in-range scalar
boundaries and copies the selected bytes. Exact-byte contains/prefix/suffix and
left-to-right non-overlapping split perform no normalization. Split retains
trailing empty parts. The alpha `graphemes` copy uses D076's pinned CRLF plus
zero-formatter-width rule; it is not full UAX #29. `trimAscii` removes exactly
space, tab, LF, CR, vertical tab, and form feed. Limits are accepted only in
the ranges fixed by D076 and are checked before output allocation.

`Bytes` is a nominal managed byte buffer implemented over private `[]byte`
storage. The executable surface is:

```marimay
// std/bytes
public type Bytes;
public type Iterator;
public type Builder;
public enum SliceError { OutOfBounds { start: int, end: int, length: int, }, }
public enum Utf8Error { Invalid { offset: int, }, }
public enum BuildError {
  InvalidLimit { limit: int, },
  LimitExceeded { limit: int, required: int, },
}
public fn new(): Bytes;
public fn fromArray(values: []byte): Bytes;
public fn toArray(bytes: Bytes): []byte;
public fn length(bytes: Bytes): int;
public fn get(bytes: Bytes, index: int): byte;
public fn set(bytes: Bytes, index: int, value: byte): void;
public fn push(bytes: Bytes, value: byte): Result<void, ArrayGrowthError>;
public fn reserve(bytes: Bytes, additional: int): Result<void, ArrayGrowthError>;
public fn append(bytes: Bytes, suffix: Bytes): Result<void, ArrayGrowthError>;
public fn clear(bytes: Bytes): void;
public fn copy(bytes: Bytes): Bytes;
public fn slice(bytes: Bytes, start: int, end: int): Result<Bytes, SliceError>;
public fn iterator(bytes: Bytes): Iterator;
public fn next(iterator: Iterator): Option<byte>;
public fn encodeUtf8(text: string): Bytes;
public fn decodeUtf8(bytes: Bytes): Result<string, Utf8Error>;
public fn newBuilder(limit: int): Result<Builder, BuildError>;
public fn appendBytes(builder: Builder, value: Bytes): Result<void, BuildError>;
public fn appendByte(builder: Builder, value: byte): Result<void, BuildError>;
public fn finish(builder: Builder): Bytes;
```

`fromArray`, `toArray`, copies, and slices own independent storage; `toArray`
therefore exposes a copy, never the owner's private array. Ordinary `Bytes`
aliases retain identity and observe mutation. `reserve` follows the exact D053
additional-capacity and failure-atomic growth contract. Iterator generation
checks use D053/G4005. UTF-8 decoding reports the first malformed byte and
publishes no partial string. Borrowed views and isolate transfer remain outside
this alpha slice.

`std/convert` defines parsing errors and explicit numeric/text conversions:

```marimay
public enum ParseError {
  Empty,
  InvalidDigit { offset: int, },
  OutOfRange,
  InvalidRadix,
}

public fn parseInt(text: string): Result<int, ParseError>;
public fn parseIntRadix(text: string, radix: int): Result<int, ParseError>;
public fn formatF32(value: f32): string;
public fn formatF64(value: f64): string;
public fn formatBool(value: bool): string;
public fn formatI8(value: i8): string;
public fn formatI16(value: i16): string;
public fn formatI32(value: i32): string;
public fn formatI64(value: i64): string;
public fn formatU8(value: u8): string;
public fn formatU16(value: u16): string;
public fn formatU32(value: u32): string;
public fn formatU64(value: u64): string;
```

The boolean and integer formatters are total and locale-independent. Booleans
are `true` and `false`. Integers use base ten, lower-case digits, one leading
minus only when negative, at least one digit, and no prefix, plus sign,
grouping, whitespace, or padding. These are the D065 interpolation spellings.

`parseInt(text)` is equivalent to `parseIntRadix(text, 10)`. Radices 2 through
36 accept ASCII digits `0`–`9`, `a`–`z`, and `A`–`Z`. A single leading minus
is allowed. A plus sign, whitespace, radix prefix, underscore, missing digit,
or trailing byte is invalid. Leading zeroes are accepted but never preserved.
`InvalidDigit.offset` is a zero-based byte offset. Values outside `i64` return
`OutOfRange`; an unsupported radix returns `InvalidRadix` before inspecting
the text. Conversion uses counted bytes and does not consult host locale or
NUL termination.

Under D046, `formatF32` and `formatF64` are total bit-to-text functions. They
return the unique locale-independent ASCII spelling specified by that decision
and do not write stdout, append a newline, or perform I/O. They are not an
implicit conversion into `println`. `formatF32` formats raw binary32 bits; it
does not specify `f32` arithmetic.

`std/path` distinguishes typed logical paths from host operations. The
executable CLI-alpha surface is:

```marimay
public enum Error {
  InvalidCharacter,
  UnsupportedPrefix,
  EscapesRoot,
  TooLong { limit: int, },
}
public type Path;
public fn fromString(text: string): Result<Path, Error>;
public fn string(path: Path): string;
public fn isAbsolute(path: Path): bool;
public fn join(base: Path, child: Path): Result<Path, Error>;
```

`Path` owns a normalized counted UTF-8 spelling of at most 4096 bytes. Empty
input denotes `.`, normalization removes repeated separators and lexical dot
segments, and an absolute path cannot ascend above its root. The accepted
separator/prefix/component spellings are the target-host rules in D077, but
the stored spelling always uses `/`. `join` returns an absolute child
unchanged and otherwise normalizes `base/string(child)`. None of these
operations queries the filesystem, follows links, grants authority, or exposes
a platform-native byte or wide-character handle.

### 3.1 Preview standard output

`std/io` is not in the prelude. The 0.1-dev user stdout surface is:

```marimay
public enum Error {
  CapabilityDenied { capability: string, },
  ReadFailed,
  WriteFailed,
  FlushFailed,
  InvalidUtf8,
  LineTooLong { limit: int, },
}

public fn message(error: Error): string;
public fn readLine(limit: int): Result<Option<string>, Error>;
public fn print(text: string): Result<void, Error>;
public fn println(text: string): Result<void, Error>;
public fn eprint(text: string): Result<void, Error>;
public fn eprintln(text: string): Result<void, Error>;
public fn flushStdout(): Result<void, Error>;
public fn flushStderr(): Result<void, Error>;
public fn readBytes(limit: int): Result<Bytes, Error>;
public fn writeBytes(value: Bytes): Result<void, Error>;
public fn ewriteBytes(value: Bytes): Result<void, Error>;
```

All output functions accept only `string`. `print` and `eprint` publish exact
UTF-8 bytes; `println` and `eprintln` append one LF. `readLine` returns
`Ok(None)` only at EOF before bytes, normalizes LF and CRLF terminators, and
rejects invalid UTF-8 or a line longer than its `0..1048576` byte limit.
`io.stdin`, `io.stdout`, and `io.stderr` are separate capabilities and stdout,
stderr, and compiler diagnostics remain separate byte streams. D075 defines
error spelling, atomicity, partial-operation retries, flush, and the I6
blocking-adapter removal gate.

The line limit counts the returned bytes after terminator removal. Thus a line
whose payload is exactly `limit` bytes followed by LF or CRLF succeeds, while
a CR not followed by LF is ordinary data. EOF after data returns that data as
`Some`; embedded NUL is retained. An invalid signed limit returns
`Err(LineTooLong { limit })` before checking `io.stdin`. Read, UTF-8, and limit
failures publish no partial `string`.

Output calls check only their named stream capability. Empty `print`/`eprint`
requests are successful no-write operations after that check. No print
operation flushes implicitly; the two explicit flush functions affect only
their named stream. Short writes are completed in order, and an interrupted
zero-progress host operation receives at most 1024 retries after its first
attempt. If a later write or flush fails, an already written prefix may remain
visible, but the typed error exposes no partial count or host error text.
Compiler/tool diagnostics never enter either program stream.

`message` returns exactly:

| Variant | Text |
|---|---|
| `CapabilityDenied { capability }` | `capability {capability} is not granted` |
| `ReadFailed` | `standard input read failed` |
| `WriteFailed` | `standard stream write failed` |
| `FlushFailed` | `standard stream flush failed` |
| `InvalidUtf8` | `standard input is not valid UTF-8` |
| `LineTooLong { limit }` | `standard input line exceeds {limit}-byte limit` |
| `InputTooLong { limit }` | `standard input exceeds {limit}-byte limit` |

Allocation, managed-budget, and runtime-integrity failures are classified by
the execution contract and do not masquerade as one of these source errors.

## 4. Collections and iteration

`[]T`, `Map<K,V>`, and `Set<T>` are mutable managed containers. An
iterator detects an intervening invalidating mutation through a runtime
generation check with a stable panic unless the iterator API explicitly owns
that mutation.

D045 fixes the initial `[]T` core surface to compiler-known
`arrayOf(...)`, ordinary free/dot-call `length`, checked indexing and indexed
assignment, and `push` returning `Result<void,ArrayGrowthError>`. Arrays contain
at most `int::MAX` initialized elements. Hitting that logical limit returns
`ArrayGrowthError.LengthLimit`; exceeding a configured isolate heap limit is a
runtime-limit outcome under the execution specification instead.

D053 adds ordinary free/dot-call `pop`, `truncate`, `clear`, and `reserve`.
Their exact signatures and error enums are the language specification's
collection declarations. Empty `pop` is `None`; `truncate` rejects negative or
expanding lengths with `ArrayTruncateError`; `clear` is infallible; and
`reserve` accepts a nonnegative additional-element count and shares
`ArrayGrowthError` with `push`. Removal never implicitly shrinks storage, and
reservation never exposes capacity. Removed reference elements cease to be
traced through the array.

D061 fixes the shared internal planning policy without adding an API:
known-count construction reserves exactly the initialized element count,
whereas unknown-size push growth starts at four and doubles. `reserve` applies
that planner once to `length + additional`; a request already covered by the
current reservation is allocation-free. The policy is identical in VM and
native execution and preserves D055's established resource and failure
boundary.

The normative array iteration spelling is `for item in values`. The
iterator is obtained once, yields each initialized element in ascending index
order, and is closed on every control-flow exit. `item` is immutable and has
the exact `T` type. It captures the D053 mutation generation and checks it
before every subsequent loop test or element production. A mismatch panics with `G4005`;
`reserve` does not invalidate because initialized values do not change.
Algorithms that intentionally mutate an array use an integer range over its
indices or an explicit `while`, because iterator invalidation still applies.

Under D079 the complete map/set surface is:

```marimay
public fn mapOf<K, V>(): Map<K, V>;
public fn length<K, V>(values: Map<K, V>): int;
public fn containsKey<K, V>(values: Map<K, V>, key: K): bool;
public fn get<K, V>(values: Map<K, V>, key: K): Option<V>;
public fn put<K, V>(values: Map<K, V>, key: K, value: V):
  Result<bool, CollectionGrowthError>;
public fn remove<K, V>(values: Map<K, V>, key: K): Option<V>;
public fn clear<K, V>(values: Map<K, V>): void;

public fn setOf<T>(): Set<T>;
public fn length<T>(values: Set<T>): int;
public fn contains<T>(values: Set<T>, value: T): bool;
public fn add<T>(values: Set<T>, value: T):
  Result<bool, CollectionGrowthError>;
public fn remove<T>(values: Set<T>, value: T): bool;
public fn clear<T>(values: Set<T>): void;

public enum CollectionGrowthError {
  LengthLimit { limit: int },
}
```

Map/set iteration is insertion-ordered. A map loop yields `(K, V)` and a set
loop yields `T`. Updating retains position; delete then reinsert appends. Hash
seeding may be process-randomized for attack resistance without changing
visible order. The supported key set is the exact primitive/string and closed
derived-nominal set in D079. Capacity and hashes are never observable.

`put` and `add` return `Ok(true)` for insertion and `Ok(false)` for update or a
duplicate respectively. New entries are bounded by `int::MAX` and use
`CollectionGrowthError.LengthLimit`; D055 budget and host-allocation failures
remain execution outcomes. Map `get`/`remove` return `None` for absence, set
`remove` reports a boolean, and empty clear is a no-op. Every committed map
insert/update/remove and set insert/remove advances a non-wrapping mutation
generation; nonempty clear does likewise. An intervening mutation makes the
next iterator check panic with `G4005` and the exact collection-specific D079
message.

Collection APIs use checked sizes and return `Result` for recoverable logical
growth limits documented by that API. Configured VM/isolate heap ceilings and
process allocation failure remain execution outcomes. Collections never expose
capacity bytes as initialized elements.

D055 fixes the execution boundary for those outcomes. A configured
managed-heap budget counts the collection's managed object, its reserved cell
storage, and managed slot bookkeeping. Exact-boundary growth succeeds;
post-collection over-budget growth is an `I0002` execution failure with
live/requested/limit bytes, while a later host allocator failure is a distinct
`I0002`. Neither adds an `ArrayGrowthError` case or changes D053's
failure-atomic logical state.

Iterators are pull-based direct calls. A separate `Stream<T,E>` protocol models suspending sequences without source-level futures:

```marimay
public interface Stream<T, E> {
  next(): Result<Option<T>, E>;
}
```

`next` may suspend through the ordinary inferred effect.

## 5. Filesystem, environment, and processes

`std/fs` functions require root filesystem capabilities and return the closed
CLI-alpha error set below. Matching and symlink-free traversal follow D077;
host error strings, numeric codes, and partial byte counts are never exposed.

```marimay
public enum Error {
  CapabilityDenied { capability: string, path: string, },
  InvalidLimit { limit: int, },
  LimitExceeded { limit: int, },
  NotFound,
  NotFile,
  NotDirectory,
  Symlink,
  Closed,
  OpenFailed,
  ReadFailed,
  WriteFailed,
  SyncFailed,
  CloseFailed,
  ReplaceFailed,
  InvalidUtf8 { offset: int, },
  TooManyEntries { limit: int, },
  DirectoryFailed,
}
public enum Kind { File, Directory, }
public enum EntryKind { File, Directory, Symlink, Other, }
public type File;
public type Metadata { kind: Kind, size: int, }
public type Entry { name: string, kind: EntryKind, }
public fn message(error: Error): string;
public fn open(path: path.Path): Result<File, Error>;
public fn read(file: File, limit: int): Result<bytes.Bytes, Error>;
public fn close(file: File): Result<void, Error>;
public fn closeBestEffort(file: File): void;
public fn readBytes(path: path.Path, limit: int): Result<bytes.Bytes, Error>;
public fn readText(path: path.Path, limit: int): Result<string, Error>;
public fn writeBytes(path: path.Path, value: bytes.Bytes): Result<void, Error>;
public fn writeText(path: path.Path, value: string): Result<void, Error>;
public fn metadata(path: path.Path): Result<Metadata, Error>;
public fn readDirectory(path: path.Path, limit: int): Result<[]Entry, Error>;
```

`open` is read-only in this bounded slice. `read` and both whole-file helpers
accept `0..1048576`; an over-limit or failed read publishes no bytes or text.
`readText` additionally validates UTF-8 and reports the first malformed byte.
The whole-file helpers consume partial host reads internally. Atomic writes
consume short writes, sync, close, and replace through a same-directory
exclusive temporary; the destination is not changed before replace succeeds.

`File.close()` is idempotent and returns `Result<void,fs.Error>`; it is used
when release failure must affect the operation's result. `File.closeBestEffort()`
is also idempotent, never panics, and returns `void`; it attempts release and
intentionally cannot replace an in-flight return, expected error, panic, or
cancellation outcome. It is suitable for a deferred fallback after acquisition,
not as a substitute for explicit `close()` when the failure matters.
Applications that require observation on every exit defer their own `void`
adapter that matches `close()` and sends failures to an explicit sink.
Convenience helpers close resources internally. Production APIs that read an
entire input require a limit. Runtime teardown best-effort closes every
execution-local handle that source did not close.

Metadata accepts only regular files and directories. File size is an exact
non-negative byte count representable by `int`; directory size is zero.
`readDirectory` accepts `0..65536`, excludes `.` and `..`, owns every name,
retains explicit symlink/other entry kinds without following them, and sorts
the complete result by unsigned UTF-8 name bytes before publication.

`std/env` is lookup-only in CLI alpha. It distinguishes missing from empty,
validates every published value as UTF-8, and requires an exact named
`env.read` capability:

```marimay
public enum Error {
  InvalidName,
  CapabilityDenied { name: string, },
  InvalidUtf8 { offset: int, },
  LimitExceeded { limit: int, },
  ReadFailed,
}
public fn message(error: Error): string;
public fn get(name: string): Result<Option<string>, Error>;
```

Names match `[A-Za-z_][A-Za-z0-9_]*` and contain at most 255 bytes. A missing
name is `Ok(None)` and an empty value is `Ok(Some(""))`. A value may contain at
most 1048576 UTF-8 bytes. On POSIX, grants and lookups are byte-exact. On
Windows they use ASCII case folding. A denied request performs no host lookup.
No error or `message` result contains an environment value or host error text.
Enumerating the environment is unavailable; it requires a future separately
accepted capability and API.

The CLI-alpha process-context surface is:

```marimay
public enum ContextError {
  CurrentDirectoryUnavailable,
  ExecutablePathUnavailable,
  InvalidUtf8 { offset: int, },
  InvalidPath,
}
public fn message(error: ContextError): string;
public fn currentDirectory(): Result<path.Path, ContextError>;
public fn executablePath(): Result<path.Path, ContextError>;
```

Both results are absolute D077 paths. The current directory is captured before
source execution. The executable path identifies the process hosting that
execution. Tests can inject both paths; equal injected process context is part
of the VM/native differential input. Host encoding errors report only the
first invalid byte offset, and platform error codes/text are not observable.

`std/process` uses an explicit command record, no implicit shell, bounded captured output, cancellation, timeout, and process-tree termination policy. Shell evaluation is a separately named unsafe-by-construction convenience and is not used by package builds.

## 6. Time and randomness

`std/time` distinguishes wall clock, monotonic instant, and duration. Deadline calculations use monotonic time. Tests receive a controllable logical clock.

```marimay
public fn now(): Instant;
public fn wallNow(): WallTime;
public fn sleep(duration: Duration): void;
```

`sleep` may suspend and is cancellation-responsive. Duration arithmetic is checked.

Secure randomness is in `std/crypto/random` and fails explicitly when the platform cannot provide it. Deterministic pseudo-random generators require an explicit seed and are reproducible across engines for a declared algorithm/version.

## 7. Tasks, timeouts, and channels

`scope`, `spawn`, `Task<T>`, and `wait` are compiler/runtime primitives specified in `execution.md`. `std/task` adds policies without allowing escape:

```marimay
public fn yield(): void;
public fn withTimeout<T>(duration: time.Duration, operation: () => T): Result<T, Timeout>;
public fn channel<T>(capacity: int): Channel<T>;
```

The general direct-style channel surface is:

```marimay
public fn send<T>(channel: Channel<T>, value: T): Result<void, ChannelClosed>;
public fn receive<T>(channel: Channel<T>): Result<Option<T>, ChannelError>;
public fn close<T>(channel: Channel<T>): void;
```

`receive` returning `Ok(None)` means all senders are closed and the buffer is drained. Capacity must be positive or explicitly zero for rendezvous semantics; there is no unbounded safe channel.

Timeout cancellation joins the nested work before returning. The D029 structured coordination surface additionally provides a first-completion race over nested operations and readiness selection over a finite set of task-completion, channel, timer, or cancellation-aware I/O cases. The race cancels and joins losing nested work. Selection unregisters losing waits but does not cancel an independently owned task. Both return a tagged winner rather than erasing result types; the I6 prototype freezes exact generic signatures only after inference, diagnostics, allocation, and cancellation measurements.

This ordinary function may be called as `yield()` or by its explicitly
qualified package name. `yield` has no grammar role and the scheduler
hint/safepoint does not promise another specific task will run.

## 8. Isolates

`std/isolate` provides structured CPU-parallel execution:

```marimay
public fn run<T, U>(input: T, operation: (T) => U): U;
```

The operation must be a statically named non-capturing function and input/output must be compiler-verified sendable. `run` may suspend. Worker-pool size and resource limits are runtime configuration, not application thread management.

Persistent actor/supervisor APIs are deferred until their lifecycle and upgrade semantics are separately specified. `isolate.run` is sufficient for 1.0 CPU fan-out and multi-worker service internals.

## 9. Networking and HTTP

`std/net` provides DNS, TCP, UDP, address parsing, deadlines, streaming I/O, half-close, and listener backpressure. All waits are cancellation-responsive. Accept/connect/read/write return structured cases and retain partial-progress counts where needed.

`std/http` provides a default convenience client plus configurable `Client`, streaming bodies, bounded headers, redirects, proxy/TLS configuration, cancellation/deadlines, connection pooling, server routing, graceful shutdown, and observability hooks.

Baseline signatures used by examples:

```marimay
import { Error as CoreError } from "std/core";

public enum Error {
  InvalidUrl { reason: string, },
  Dns(net.Error),
  Connect(net.Error),
  Protocol { reason: string, },
  Timeout,
  BodyLimit { limit: int, },
}

public fn message(error: Error): string;

public fn get(url: string): Result<Response, http.Error>;

public type Response {
  public status: int,
  public headers: Headers,
  public body: Bytes,
}

public fn pathParameter(request: Request, name: string): Option<string>;
public fn jsonHeaders(): Headers;

public fn getRoute<E: CoreError>(
  pattern: string,
  handler: (Request) => Result<Response, E>,
): Route;

public fn serve(server: Server): Result<void, http.Error>;
```

The full body-in-memory `get` convenience enforces a conservative default size. Streaming production code uses `Client.send` and a body `Stream<Bytes,http.Error>`. Server request handlers run as children of a server scope; graceful shutdown stops acceptance, cancels or drains according to policy, and joins every handler before returning.

HTTP parsing rejects request smuggling ambiguities and configurable-limit violations. TLS defaults follow current security policy and are updated as data, not by changing application source signatures.

## 10. JSON and serialization

`std/json` exposes a bounded streaming token reader/writer and typed codec functions:

```marimay
public fn decode<T>(input: Bytes): Result<T, json.Error>;
public fn encode<T>(value: T): Result<Bytes, json.Error>;
```

Typed codecs require compiler derives or explicit codec values; there is no unrestricted runtime reflection into private fields. Unknown/missing/duplicate-field behavior is explicit per codec policy. Nesting, number size, string size, and total bytes are limited.

Derive metadata supports field rename/omit/default only through a closed declarative attribute set added by a separate decision before freeze. Arbitrary serialization macros are not introduced.

## 11. Logging, metrics, and tracing

`std/log` offers leveled messages, structured typed fields, sinks, redaction markers, and task-context propagation. A simple call is valid:

```marimay
log.info($"service listening on {server.address}");
```

Logging is backpressured or drops according to an explicit sink policy; it never creates unbounded tasks/queues. Secrets can be marked so default renderers redact them.

Runtime task, GC, I/O, and capability trace events use stable schemas. Metrics APIs define counters, gauges, and histograms without requiring a particular exporter.

## 12. Testing surface

`std/test` and its implicit `expect` namespace implement the toolchain specification. Tests get deterministic clock/random/filesystem/output and a root task/resource tracker.

Expectation structural diffs respect derived `Debug`/`Eq`, bound output size, and never invoke arbitrary mutating functions. Panic assertions run the closure in a nested task so the harness survives while still verifying cleanup.

`std/test/property` provides deterministic generators/shrinkers. The same test declaration and process are used by `mmay test`; no plugin runner is required.

## 13. C interoperation

`std/c` is not in the prelude. It contains explicit-width C ABI integers, `Opaque`, raw pointer wrappers, C strings, owned/borrowed buffers, handle scopes, callbacks, layout queries, and dynamic-library operations where supported.

Every operation requiring caller invariants is `unsafe`. Safe wrapper packages expose normal nominal types and `Result` errors. C views cannot cross a suspension; `@blocking` calls copy or otherwise marshal data before leaving the isolate.

## 14. Production acceptance for a package

A first-party package is complete only when it has:

- public API documentation and executable examples;
- VM/native conformance tests;
- cancellation, timeout, cleanup, and resource-limit tests where relevant;
- capability-denial tests;
- fuzzing for parsers/protocol boundaries;
- platform interop tests on every claimed Tier 1 target;
- stable error cases and no string parsing required for control flow;
- benchmark/load budgets for performance-sensitive paths;
- compatibility fixtures once released.

An API may ship experimental behind an explicit language/module opt-in, but it cannot be silently treated as stable standard library.
