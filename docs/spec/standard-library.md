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

## 5. Command-line parsing

D081 defines `std/cli` as the first-party declarative application parser. It
is an ordinary source package over `std/text`, arrays, `Map`, `Set`, `Option`,
and `Result`; importing it introduces no compiler-known operation or host
parser. Its public model and constructors are:

```marimay
public enum ValueMode {
  Flag,
  Required { name: string },
  Optional { name: string },
}

public type OptionSpec {
  public long: string,
  public short: Option<string>,
  public value: ValueMode,
  public required: bool,
  public repeatable: bool,
  public help: string,
}

public type PositionalSpec {
  public name: string,
  public required: bool,
  public repeatable: bool,
  public help: string,
}

public type Limits {
  public arguments: int,
  public options: int,
  public positionals: int,
  public helpBytes: int,
}

public type Parser;
public type Parsed;

public fn defaultLimits(): Limits;
public fn limits(
  arguments: int,
  options: int,
  positionals: int,
  helpBytes: int,
): Limits;
public fn flagOption(
  long: string,
  short: Option<string>,
  required: bool,
  repeatable: bool,
  help: string,
): OptionSpec;
public fn requiredValueOption(
  long: string,
  short: Option<string>,
  valueName: string,
  required: bool,
  repeatable: bool,
  help: string,
): OptionSpec;
public fn optionalValueOption(
  long: string,
  short: Option<string>,
  valueName: string,
  required: bool,
  repeatable: bool,
  help: string,
): OptionSpec;
public fn positional(
  name: string,
  required: bool,
  repeatable: bool,
  help: string,
): PositionalSpec;
```

`Parser` and `Parsed` have no public fields. `newParser` validates and copies
the declarative boundary into lookup state; its array order remains the help
and positional order:

```marimay
public fn newParser(
  program: string,
  summary: string,
  options: []OptionSpec,
  positionals: []PositionalSpec,
  limits: Limits,
): Result<Parser, DefinitionError>;

public fn parse(
  parser: Parser,
  arguments: []string,
): Result<Parsed, UsageError>;
public fn count(parsed: Parsed, name: string): int;
public fn occurrences(
  parsed: Parsed,
  name: string,
): []Option<string>;
public fn positionalValues(
  parsed: Parsed,
  name: string,
): []string;
public fn help(parser: Parser, width: int): Result<string, HelpError>;
public fn usageExitStatus(): int;
public fn message(error: DefinitionError): string;
public fn message(error: UsageError): string;
public fn message(error: HelpError): string;
```

`defaultLimits()` is `{ arguments: 4096, options: 128, positionals: 64,
helpBytes: 65536 }`. Custom argument, option, and positional limits are
nonnegative and at most 4096, 256, and 256. The help limit is in
`256..1048576`. A program or declaration name is nonempty, at most 64 UTF-8
bytes, does not begin with `-`, and contains no `=`, ASCII space, tab, CR, or
LF. Unicode long and positional names are exact counted-UTF-8 names. A short
name is exactly one ASCII letter or digit. Summaries are single-line and at
most 4096 bytes. Help strings additionally have no leading/trailing ASCII
whitespace or adjacent ASCII spaces. Value names follow the declaration-name
grammar. Duplicate names are compared as exact counted strings. A repeatable
positional is last, and no required positional follows an optional one.

The closed definition errors are:

```marimay
public enum DefinitionError {
  InvalidLimits,
  InvalidProgram,
  TooManyOptions { limit: int },
  TooManyPositionals { limit: int },
  InvalidLongName { index: int },
  InvalidShortName { index: int },
  InvalidValueName { index: int },
  InvalidOptionHelp { index: int },
  DuplicateLong { name: string },
  DuplicateShort { name: string },
  InvalidPositionalName { index: int },
  InvalidPositionalHelp { index: int },
  DuplicatePositional { name: string },
  RequiredPositionalAfterOptional { index: int },
  RepeatedPositionalNotLast { index: int },
}
```

`parse` excludes an executable name; it receives exactly the D066 program
argument array. Long options use `--name` or `--name=value`. Flags reject an
attached value. A required value uses its attached suffix or consumes the next
argument even when that argument begins with `-`; `--name=` is a present empty
value. An optional value is present only when attached and never consumes the
next argument. Short flags cluster. A short required/optional value consumes
the rest of that token when present and ends the cluster; otherwise only a
required value consumes the next argument. `--` disables option recognition,
is not emitted as positional data, and makes all later strings positional. A
lone `-` is positional. Parsing performs no shell expansion, abbreviation,
case folding, environment/config lookup, or type conversion.

Occurrences and positionals retain input order. Nonrepeatable options reject a
second occurrence regardless of spelling. Positional declarations consume in
order; a repeatable final positional consumes the remainder. Required options
and positionals are checked after input consumption. Error indices are
zero-based indices into `arguments`. Unknown names passed to `count`,
`occurrences`, or `positionalValues` return zero or a fresh empty array. The
closed usage errors are:

```marimay
public enum UsageError {
  TooManyArguments { limit: int },
  UnknownOption { argument: string, index: int },
  UnexpectedOptionValue { name: string, index: int },
  MissingOptionValue { name: string, index: int },
  RepeatedOption { name: string, index: int },
  UnexpectedPositional { value: string, index: int },
  MissingRequiredOption { name: string },
  MissingRequiredPositional { name: string },
}
```

`usageExitStatus()` is exactly 2. `message` is deterministic English text and
contains only the stored spelling, zero-based index, or declared limit.
Managed-budget or host-allocation failure remains `I0002`, not `UsageError`.

Help width is a byte width in `40..120`. `help` emits `Usage:`, then the
optional `[options]` token, positional synopsis tokens, the summary, `Options:`
entries, and `Arguments:` entries in declaration order. Descriptions wrap only
at their validated single ASCII spaces, use a four-space continuation indent,
and never split a token or word. The result always ends with LF. Its errors are:

```marimay
public enum HelpError {
  InvalidWidth { width: int },
  LineTooLong { width: int, required: int },
  LimitExceeded { limit: int },
}
```

Subcommands, multiple aliases, automatic completion, localized help, and
terminal column-width or grapheme-width measurement are not part of this
surface.

## 6. Filesystem, environment, and processes

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

## 7. Time and randomness

`std/time` distinguishes wall clock, monotonic instant, and duration. Deadline calculations use monotonic time. Tests receive a controllable logical clock.

```marimay
public fn now(): Instant;
public fn wallNow(): WallTime;
public fn sleep(duration: Duration): void;
```

`sleep` may suspend and is cancellation-responsive. Duration arithmetic is checked.

Secure randomness is in `std/crypto/random` and fails explicitly when the platform cannot provide it. Deterministic pseudo-random generators require an explicit seed and are reproducible across engines for a declared algorithm/version.

## 8. Tasks, timeouts, and channels

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

## 9. Isolates

`std/isolate` provides structured CPU-parallel execution:

```marimay
public fn run<T, U>(input: T, operation: (T) => U): U;
```

The operation must be a statically named non-capturing function and input/output must be compiler-verified sendable. `run` may suspend. Worker-pool size and resource limits are runtime configuration, not application thread management.

Persistent actor/supervisor APIs are deferred until their lifecycle and upgrade semantics are separately specified. `isolate.run` is sufficient for 1.0 CPU fan-out and multi-worker service internals.

## 10. Networking and HTTP

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

## 11. JSON and serialization

`std/json` is an ordinary source package implementing the D082 bounded cursor
and explicit-codec model. It does not expose a DOM or dynamic `any` value and
does not use compiler or runtime reflection.

### 11.1 Public model and limits

The public model is:

```marimay
public type Span {
  public start: int,
  public end: int,
}

public type Limits {
  public inputBytes: int,
  public outputBytes: int,
  public stringBytes: int,
  public numberBytes: int,
  public nesting: int,
  public items: int,
}

public enum UnknownFields { Reject, Skip }

public enum Error {
  InvalidLimits,
  InvalidUtf8 { span: Span, },
  UnexpectedToken { expected: string, span: Span, },
  UnexpectedEnd { expected: string, span: Span, },
  InvalidEscape { span: Span, },
  InvalidUnicodeEscape { span: Span, },
  InvalidNumber { span: Span, },
  NumberOutOfRange { span: Span, },
  DuplicateField { name: string, span: Span, },
  UnknownField { name: string, span: Span, },
  MissingField { name: string, span: Span, },
  LimitExceeded { resource: string, limit: int, span: Span, },
  InvalidEncoderState { message: string, },
}

public type DecoderFrame;
public type EncoderFrame;
public type Decoder;
public type Encoder;

public fn defaultLimits(): Limits;
public fn limits(
  inputBytes: int,
  outputBytes: int,
  stringBytes: int,
  numberBytes: int,
  nesting: int,
  items: int,
): Limits;
public fn message(error: Error): string;
```

`Decoder` and `Encoder` are public nominal handles whose stored state is
private. The public `DecoderFrame` and `EncoderFrame` nominal declarations are
opaque state carriers needed by the Stage 0 imported-record layout contract;
their fields are private and application code cannot construct or inspect
them. This representation detail grants no reflective access.

`Span` is a half-open byte range in the decoder's owned input. Encoder errors
that do not correspond to input use `Span { start: 0, end: 0 }`. Default limits
are:

| Resource | Default | Accepted custom range |
|---|---:|---:|
| input bytes | 1,048,576 | 0..1,048,576 |
| output bytes | 1,048,576 | 0..1,048,576 |
| decoded/encoded string bytes | 262,144 | 0..1,048,576 |
| number bytes | 128 | 1..1,024 |
| nested containers | 128 | 1..256 |
| members in one object or array | 65,536 | 0..1,048,576 |

`items` is per open container, not a whole-document allocation allowance. The
managed-heap budget remains the whole-execution bound. Invalid limit tuples
return `InvalidLimits`. Managed-budget and allocator exhaustion remain
`I0002`; a JSON operation does not translate them into `InvalidLimits` or
`LimitExceeded`.

### 11.2 Decoder

The decoder API is:

```marimay
public fn newDecoder(input: Bytes, limits: Limits): Result<Decoder, Error>;
public fn finishDecoder(decoder: Decoder): Result<void, Error>;
public fn readString(decoder: Decoder): Result<string, Error>;
public fn readBool(decoder: Decoder): Result<bool, Error>;
public fn readNull(decoder: Decoder): Result<void, Error>;
public fn readNumber(decoder: Decoder): Result<string, Error>;
public fn readInt(decoder: Decoder): Result<int, Error>;
public fn beginObject(decoder: Decoder): Result<void, Error>;
public fn nextField(decoder: Decoder): Result<Option<string>, Error>;
public fn beginArray(decoder: Decoder): Result<void, Error>;
public fn nextElement(decoder: Decoder): Result<bool, Error>;
public fn readBytes(decoder: Decoder): Result<Bytes, Error>;
public fn skipValue(decoder: Decoder): Result<void, Error>;
public fn handleUnknown(
  decoder: Decoder,
  name: string,
  policy: UnknownFields,
): Result<void, Error>;
public fn missingField(decoder: Decoder, name: string): Error;
public fn unknownVariant(decoder: Decoder, name: string): Error;
```

`newDecoder` validates the complete input as UTF-8, applies the input-byte
limit, and owns a copy before returning. The JSON grammar is strict: only the
four JSON whitespace bytes are ignored; strings reject unescaped controls;
escapes are `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, and `\u` with
exactly four hexadecimal digits; UTF-16 surrogate escapes must be a valid
high/low pair. Other input UTF-8 remains unchanged and is not normalized.

`readNumber` accepts exactly the JSON number grammar and returns the original
number spelling after leading/trailing whitespace handling. Leading zeroes,
missing integer/fraction/exponent digits, and a following non-delimiter byte
are `InvalidNumber`. `readInt` additionally parses the value as signed 64-bit
`int` and reports `NumberOutOfRange`. Floating-point conversion is not implied
by either operation.

`beginObject`/`nextField` and `beginArray`/`nextElement` are paired cursor
operations. `nextField` returns the decoded key, consumes its colon, and
positions the cursor at the value; `None` consumes the closing brace.
`nextElement` returns `true` at the next value and `false` after consuming the
closing bracket. Trailing commas are rejected. Every object tracks complete
decoded key strings and rejects a duplicate with the second key's span before
its value is decoded. Duplicate behavior is not configurable.

`skipValue` recognizes one complete value iteratively and applies the same
grammar and limits as typed reads. `handleUnknown` either returns
`UnknownField` at the most recent key span or calls `skipValue`.
`missingField` reports the current cursor as an empty span.
`unknownVariant` uses the same typed `UnknownField` shape at the current
cursor; the variant name is codec-supplied. `readBytes` is a JSON array whose
members must be signed integers in `0..255`.

`finishDecoder` succeeds only when no container remains open and, after JSON
whitespace, the owned input is exhausted. A caller must not publish a decoded
root value before both its codec and `finishDecoder` succeed.

### 11.3 Encoder and deterministic form

The encoder API is:

```marimay
public fn newEncoder(limits: Limits): Result<Encoder, Error>;
public fn finishEncoder(encoder: Encoder): Result<Bytes, Error>;
public fn writeNull(encoder: Encoder): Result<void, Error>;
public fn writeBool(encoder: Encoder, value: bool): Result<void, Error>;
public fn writeInt(encoder: Encoder, value: int): Result<void, Error>;
public fn writeNumber(encoder: Encoder, value: string): Result<void, Error>;
public fn writeString(encoder: Encoder, value: string): Result<void, Error>;
public fn beginObject(encoder: Encoder): Result<void, Error>;
public fn writeKey(encoder: Encoder, key: string): Result<void, Error>;
public fn endObject(encoder: Encoder): Result<void, Error>;
public fn beginArray(encoder: Encoder): Result<void, Error>;
public fn endArray(encoder: Encoder): Result<void, Error>;
public fn writeBytes(encoder: Encoder, value: Bytes): Result<void, Error>;
```

The encoder accepts exactly one root value and enforces the object key/value
and container state machine. Exceeding any limit or violating that state
poisons the encoder. `finishEncoder` publishes bytes only when one root is
complete, every container is closed, and no earlier error occurred.

Output contains no insignificant whitespace. Object order is exactly
`writeKey` call order; array order is write call order. A codec encoding a
`Map` inherits D079 insertion order. Strings escape quote and backslash, use
the short escape for backspace/form-feed/newline/carriage-return/tab, use
lowercase `\u00xx` for other controls, leave `/` unescaped, and emit all other
valid UTF-8 literally. `writeNumber` first validates one complete JSON number
under the encoder's number limit. `writeBytes` emits a JSON array of decimal
integers.

This is a deterministic compact form, not a general canonicalization scheme:
it does not reorder object fields, normalize Unicode, or rewrite a valid
`writeNumber` spelling.

### 11.4 Explicit typed codecs, privacy, and integration

A typed codec is ordinary package-owned code. The conventional shape is:

```marimay
public type Codec { /* policy owned by this package */ }

public fn decode(
  codec: Codec,
  decoder: json.Decoder,
): Result<Record, json.Error>;

public fn encode(
  codec: Codec,
  encoder: json.Encoder,
  value: Record,
): Result<void, json.Error>;
```

These functions are dot-call eligible through their exact first parameter;
the convention does not create a special method or interface. A codec owns
field names, declaration/write order, required/default/omitted behavior,
unknown-field policy, enum tag representation, and conversions. It explicitly
builds records/enums/arrays/maps from the cursor operations. Because normal
visibility rules apply, it can access private fields only when it is declared
in their owning package. `std/json` cannot enumerate or mutate them.

Files and streams are composed explicitly: bounded bytes come from `std/fs`
or `std/io`, are passed to `newDecoder`, and encoded bytes are passed back to
those packages. JSON grants no filesystem, standard-stream, environment, or
network capability. There is no host JSON library, unbounded DOM, dynamic
tree, schema registry, arbitrary macro, or reflection escape hatch in this
surface.

For untrusted input, callers choose limits no larger than the enclosing file,
stream, and managed-heap budgets, reject or deliberately skip unknown fields,
require `finishDecoder`, and avoid echoing complete attacker-controlled input
in surrounding diagnostics. JSON error messages are bounded structural text
containing a fixed expectation/resource, a limit, a field/tag name, and/or a
byte offset; they never include host error codes, ambient paths, capability
roots, or unrelated input bytes.

Any future derive or attribute syntax requires its own accepted decision and
the complete semantic-change protocol; it cannot silently replace the D082
explicit codec contract.

## 12. Logging, metrics, and tracing

`std/log` offers leveled messages, structured typed fields, sinks, redaction markers, and task-context propagation. A simple call is valid:

```marimay
log.info($"service listening on {server.address}");
```

Logging is backpressured or drops according to an explicit sink policy; it never creates unbounded tasks/queues. Secrets can be marked so default renderers redact them.

Runtime task, GC, I/O, and capability trace events use stable schemas. Metrics APIs define counters, gauges, and histograms without requiring a particular exporter.

## 13. Testing surface

`std/test` and its implicit `expect` namespace implement the toolchain specification. Tests get deterministic clock/random/filesystem/output and a root task/resource tracker.

Expectation structural diffs respect derived `Debug`/`Eq`, bound output size, and never invoke arbitrary mutating functions. Panic assertions run the closure in a nested task so the harness survives while still verifying cleanup.

`std/test/property` provides deterministic generators/shrinkers. The same test declaration and process are used by `mmay test`; no plugin runner is required.

## 14. C interoperation

`std/c` is not in the prelude. It contains explicit-width C ABI integers, `Opaque`, raw pointer wrappers, C strings, owned/borrowed buffers, handle scopes, callbacks, layout queries, and dynamic-library operations where supported.

Every operation requiring caller invariants is `unsafe`. Safe wrapper packages expose normal nominal types and `Result` errors. C views cannot cross a suspension; `@blocking` calls copy or otherwise marshal data before leaving the isolate.

## 15. Production acceptance for a package

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
