# Ruda standard-library contract — design baseline 0.1

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
Array, ArrayGrowthError, Map, Set, Bytes
Iterator
Task
expect          # @test/@fuzz functions only
panic           # fn panic(message: string): never
```

It does not inject filesystem, network, environment, logging, JSON, reflection, process, or unsafe C APIs.

### 2.1 `Option<T>`

Required dot-call functions:

```ruda
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

```ruda
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

```ruda
public interface Error {
  message(): string;
}
```

First-party error enums provide stable machine-readable cases in addition to `message`. `std/error` supplies wrappers for context and cause chains without requiring every domain enum to expose optional signatures:

```ruda
public fn context(error: Error, message: string): Error;
public fn causes(error: Error): Iterator<Error>;
public fn format(error: Error): string;
```

`?` does not automatically allocate or rewrite errors. Applications add domain context at boundaries where it is meaningful.

## 3. Text, bytes, conversion, and paths

`string` is immutable valid UTF-8. Packages expose explicit iteration/views for bytes, Unicode scalars, and grapheme clusters. Slicing at a non-boundary returns an error; integer indexing is absent.

`Bytes` is a managed byte buffer with bounded growth, slicing, copy, and explicit isolate-transfer support. A borrowed byte view cannot cross a suspension point or outlive its owner.

`std/convert` defines parsing errors and explicit numeric/text conversions:

```ruda
public enum ParseError {
  Empty,
  InvalidDigit { offset: int; },
  OutOfRange,
}

public fn parseInt(text: string): Result<int, ParseError>;
public fn parseIntRadix(text: string, radix: int): Result<int, ParseError>;
```

`std/path` distinguishes portable logical paths from platform-native behavior. Normalization is lexical and never claims to resolve symlinks; filesystem canonicalization is a separate capability-checked operation.

## 4. Collections and iteration

`Array<T>`, `Map<K,V>`, and `Set<T>` are mutable managed containers. Mutation while an iterator over the same container is active is rejected by a runtime generation check with a stable panic, unless the iterator API explicitly owns the mutation operation.

D045 fixes the initial `Array<T>` core surface to compiler-known
`arrayOf(...)`, ordinary free/dot-call `length`, checked indexing and indexed
assignment, and `push` returning `Result<void,ArrayGrowthError>`. Arrays contain
at most `int::MAX` initialized elements. Hitting that logical limit returns
`ArrayGrowthError.LengthLimit`; exceeding a configured isolate heap limit is a
runtime-limit outcome under the execution specification instead.

Map/set iteration is insertion-ordered. Updating retains position; delete then reinsert appends. Hash seeding may be process-randomized for attack resistance without changing visible order.

Collection APIs use checked sizes and return `Result` for recoverable logical
growth limits documented by that API. Configured VM/isolate heap ceilings and
process allocation failure remain execution outcomes. Collections never expose
capacity bytes as initialized elements.

Iterators are pull-based direct calls. A separate `Stream<T,E>` protocol models suspending sequences without source-level futures:

```ruda
public interface Stream<T, E> {
  next(): Result<Option<T>, E>;
}
```

`next` may suspend through the ordinary inferred effect.

## 5. Filesystem, environment, and processes

`std/fs` functions require root filesystem capabilities and return structured `fs.Error` cases. Paths are checked against capabilities after safe canonicalization rules appropriate to the operation. APIs cover partial reads/writes and do not assume one call transfers the entire buffer.

```ruda
public fn open(path: path.Path, options: OpenOptions): Result<File, Error>;
public fn readText(path: path.Path, limit: int): Result<string, Error>;
public fn writeText(path: path.Path, value: string): Result<void, Error>;
public fn close(file: File): Result<void, Error>;
public fn closeBestEffort(file: File): void;
```

`File.close()` is idempotent and returns `Result<void,fs.Error>`; it is used
when release failure must affect the operation's result. `File.closeBestEffort()`
is also idempotent, never panics, and returns `void`; it attempts release and
intentionally cannot replace an in-flight return, expected error, panic, or
cancellation outcome. It is suitable for a deferred fallback after acquisition,
not as a substitute for explicit `close()` when the failure matters.
Applications that require observation on every exit defer their own `void`
adapter that matches `close()` and sends failures to an explicit sink.
Convenience helpers close resources internally. Production APIs that read an
entire input require a limit.

`std/env` distinguishes missing values from invalid encoding and requires named `env.read` capabilities. Enumerating the entire environment needs a separate grant.

`std/process` uses an explicit command record, no implicit shell, bounded captured output, cancellation, timeout, and process-tree termination policy. Shell evaluation is a separately named unsafe-by-construction convenience and is not used by package builds.

## 6. Time and randomness

`std/time` distinguishes wall clock, monotonic instant, and duration. Deadline calculations use monotonic time. Tests receive a controllable logical clock.

```ruda
public fn now(): Instant;
public fn wallNow(): WallTime;
public fn sleep(duration: Duration): void;
```

`sleep` may suspend and is cancellation-responsive. Duration arithmetic is checked.

Secure randomness is in `std/crypto/random` and fails explicitly when the platform cannot provide it. Deterministic pseudo-random generators require an explicit seed and are reproducible across engines for a declared algorithm/version.

## 7. Tasks, timeouts, and channels

`scope`, `spawn`, `Task<T>`, and `wait` are compiler/runtime primitives specified in `execution.md`. `std/task` adds policies without allowing escape:

```ruda
public fn yield(): void;
public fn withTimeout<T>(duration: time.Duration, operation: () => T): Result<T, Timeout>;
public fn channel<T>(capacity: int): Channel<T>;
```

The general direct-style channel surface is:

```ruda
public fn send<T>(channel: Channel<T>, value: T): Result<void, ChannelClosed>;
public fn receive<T>(channel: Channel<T>): Result<Option<T>, ChannelError>;
public fn close<T>(channel: Channel<T>): void;
```

`receive` returning `Ok(None)` means all senders are closed and the buffer is drained. Capacity must be positive or explicitly zero for rendezvous semantics; there is no unbounded safe channel.

Timeout cancellation joins the nested work before returning. The D029 structured coordination surface additionally provides a first-completion race over nested operations and readiness selection over a finite set of task-completion, channel, timer, or cancellation-aware I/O cases. The race cancels and joins losing nested work. Selection unregisters losing waits but does not cancel an independently owned task. Both return a tagged winner rather than erasing result types; the I6 prototype freezes exact generic signatures only after inference, diagnostics, allocation, and cancellation measurements.

This ordinary function may be called as `yield()` or by its explicitly
qualified package name. It is distinct from the contextual
`yield expression;` transfer inside a block match arm. The scheduler
hint/safepoint does not promise another specific task will run.

## 8. Isolates

`std/isolate` provides structured CPU-parallel execution:

```ruda
public fn run<T, U>(input: T, operation: (T) => U): U;
```

The operation must be a statically named non-capturing function and input/output must be compiler-verified sendable. `run` may suspend. Worker-pool size and resource limits are runtime configuration, not application thread management.

Persistent actor/supervisor APIs are deferred until their lifecycle and upgrade semantics are separately specified. `isolate.run` is sufficient for 1.0 CPU fan-out and multi-worker service internals.

## 9. Networking and HTTP

`std/net` provides DNS, TCP, UDP, address parsing, deadlines, streaming I/O, half-close, and listener backpressure. All waits are cancellation-responsive. Accept/connect/read/write return structured cases and retain partial-progress counts where needed.

`std/http` provides a default convenience client plus configurable `Client`, streaming bodies, bounded headers, redirects, proxy/TLS configuration, cancellation/deadlines, connection pooling, server routing, graceful shutdown, and observability hooks.

Baseline signatures used by examples:

```ruda
import { Error as CoreError } from "std/core";

public enum Error {
  InvalidUrl { reason: string; },
  Dns(net.Error),
  Connect(net.Error),
  Protocol { reason: string; },
  Timeout,
  BodyLimit { limit: int; },
}

public fn message(error: Error): string;

public fn get(url: string): Result<Response, http.Error>;

public type Response {
  public status: int;
  public headers: Headers;
  public body: Bytes;
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

```ruda
public fn decode<T>(input: Bytes): Result<T, json.Error>;
public fn encode<T>(value: T): Result<Bytes, json.Error>;
```

Typed codecs require compiler derives or explicit codec values; there is no unrestricted runtime reflection into private fields. Unknown/missing/duplicate-field behavior is explicit per codec policy. Nesting, number size, string size, and total bytes are limited.

Derive metadata supports field rename/omit/default only through a closed declarative attribute set added by a separate decision before freeze. Arbitrary serialization macros are not introduced.

## 11. Logging, metrics, and tracing

`std/log` offers leveled messages, structured typed fields, sinks, redaction markers, and task-context propagation. A simple call is valid:

```ruda
log.info($"service listening on {server.address}");
```

Logging is backpressured or drops according to an explicit sink policy; it never creates unbounded tasks/queues. Secrets can be marked so default renderers redact them.

Runtime task, GC, I/O, and capability trace events use stable schemas. Metrics APIs define counters, gauges, and histograms without requiring a particular exporter.

## 12. Testing surface

`std/test` and its implicit `expect` namespace implement the toolchain specification. Tests get deterministic clock/random/filesystem/output and a root task/resource tracker.

Expectation structural diffs respect derived `Debug`/`Eq`, bound output size, and never invoke arbitrary mutating functions. Panic assertions run the closure in a nested task so the harness survives while still verifying cleanup.

`std/test/property` provides deterministic generators/shrinkers. The same test declaration and process are used by `ruda test`; no plugin runner is required.

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
