# Ruda diagnostics specification — design baseline 0.1

Status: normative user-facing contract.

Diagnostics are part of the language/toolchain, not incidental compiler strings. A rejected program must make the failure, location, reason, and likely repair discoverable without requiring knowledge of compiler internals.

## 1. Diagnostic identity

Every compiler, verifier, formatter, package, test, vet, and runtime diagnostic has a stable code:

```text
Lxxxx   lexical/source encoding
Pxxxx   parser/syntax
Rxxxx   name/package resolution
Txxxx   type/interface/generic
Mxxxx   match/exhaustiveness
Exxxx   effect/task/escape
Fxxxx   formatter
Bxxxx   MIR/bytecode/backend/build
Gxxxx   managed memory/runtime
Kxxxx   package/lock/registry
Cxxxx   capability/unsafe/FFI
Qxxxx   test/vet/fuzz/benchmark
Uxxxx   command-line usage and invocation
Ixxxx   internal compiler/runtime failure
```

Codes are never silently reused for a semantically unrelated condition. Splitting a broad code is allowed; machine consumers can inspect the schema-versioned category and fields.

Formatter workflow failures use `F0001` when an input cannot be read, `F0002` when canonical bytes cannot be written or atomically committed, `F0003` when a documented defensive limit is exhausted, and `F0004` when `ruda fmt --check` finds non-canonical input. Lexical and parser failures discovered while formatting retain their original `Lxxxx` or `Pxxxx` identities and source spans; the formatter does not collapse them into a generic `Fxxxx` error.

The allocated codes and summaries are published in the generated [diagnostic code registry](../generated/diagnostic-codes.md).

`ruda explain T1234` prints the rule, minimal bad/good examples, common causes, and links to the installed version-matched reference. Explanations are available offline.

## 2. Structured model

A diagnostic contains:

- schema version and stable code;
- severity: error, warning, information, or help;
- concise primary message;
- a required nullable primary source span: a span when a source snapshot meaningfully owns the failure, otherwise `null`;
- zero or more labeled secondary spans;
- causal notes ordered from immediate to underlying reason;
- zero or more help messages;
- zero or more non-overlapping machine-applicable edits;
- related symbol/package/task identifiers where applicable;
- origin component and language/toolchain version.

A span identifies a source snapshot, file identity, byte start/end, and lazily computed line/Unicode column. Diagnostics never point into a newer unsaved snapshot without marking the version mismatch. A diagnostic with no honest source owner uses `primary: null` and may include a typed `context` object such as a command-line argument index, package identity, bytecode section, or runtime resource. It must not invent a pseudo-source span.

Error ordering is deterministic by module/package, file path, byte position, severity, and code. Parallel checking cannot reorder output between runs.

## 3. Human rendering

Default rendering shows the shortest useful source window with Unicode-safe carets, labels, and related locations:

```text
error[M1001]: match does not cover `Lookup.Denied`
  --> src/user.ruda:18:10
   |
18 |   return match result {
   |          ^^^^^^^^^^^^ missing case `Denied { reason: _ }`
   |
help: add an arm before the closing brace
   |
21 |     case Denied { reason } => ...;
   |
```

Rules:

- terminal color is optional and never the only carrier of meaning;
- tab/wide/combining characters align correctly;
- source lines and labels are bounded for malicious/generated input;
- long inferred types are pretty-printed structurally with named elisions and an expansion path;
- repeated causes are summarized rather than producing thousands of notes;
- paths are relative to module/workspace where unambiguous;
- secrets, environment values, HTTP headers, and source outside granted display scope are redacted.

`--diagnostic short` emits one stable line per diagnostic for editor/build logs. `--diagnostic json` emits the structured form, not ANSI-decorated text.

## 4. JSON format

JSON diagnostics are newline-delimited so streaming tools receive progress without buffering an entire build. Required top-level fields are `schema`, `code`, `severity`, `message`, `primary`, `labels`, `notes`, and `fixes`. `primary` is either the source-span object below or `null` under D021:

```json
{
  "schema": "ruda.diagnostic/1",
  "code": "M1001",
  "severity": "error",
  "message": "match does not cover Lookup.Denied",
  "primary": {
    "file": "src/user.ruda",
    "snapshot": "sha256:...",
    "startByte": 412,
    "endByte": 425,
    "start": { "line": 18, "column": 10 },
    "end": { "line": 18, "column": 23 }
  },
  "labels": [],
  "notes": [],
  "fixes": []
}
```

New optional fields may be added within schema 1. Removing/changing required fields or their meaning requires a schema version. Unknown fields must be ignored by consumers.

A command invocation diagnostic has the same required shape without pretending that an argument is a source file:

```json
{
  "schema": "ruda.diagnostic/1",
  "code": "U0001",
  "severity": "error",
  "message": "unknown command 'buid'; did you mean 'build'?",
  "primary": null,
  "labels": [],
  "notes": [],
  "fixes": [],
  "context": { "kind": "command-line", "argument": 1 }
}
```

A machine-applicable fix includes description, applicability, source snapshot hashes, and edits. `ruda fix` rejects stale/overlapping edits rather than guessing.

## 5. Type and interface diagnostics

Type errors display the expected and actual type at the relevant boundary, followed by the first structural mismatch. Interface failures list missing or incompatible dot-call signatures and the eligible first-parameter function location when one nearly matches.

Example shape:

```text
error[T2403]: `User` does not satisfy `Stringer`
  --> src/view.ruda:9:14
note: required method: string(): string
note: candidate `string(user: User, locale: Locale): string`
      has one additional required parameter
help: provide `fn string(user: User): string`
```

The compiler does not dump a full generic solver trace by default. `--verbose` or `ruda explain inference <span>` may show canonical constraints for compiler debugging.

## 6. Match diagnostics

Non-exhaustive diagnostics provide concrete missing witness patterns. For large spaces they show a bounded representative set and count/ellipsis. A guard is explicitly named as non-covering when it causes surprise. Unreachable arms label both the unreachable arm and the earlier covering pattern.

Adding a wildcard is not the first automatic suggestion when the compiler knows the finite missing cases.

## 7. Dot-call diagnostics

Dot-call lookup failures distinguish:

- no visible same-named function exists;
- a same-named function is not dot-call eligible because its first parameter does not exactly match the subject's static nominal type;
- the first parameter's nominal type belongs to another package and cannot gain dot-call behavior here;
- an eligible same-named declaration exists for a different nominal type;
- an interface value lacks the required method;
- visibility prevents access.

Diagnostics state that `value.method(args)` and `method(value,args)` are one symbol; they never suggest creating an `impl` block.

## 8. Suspension and task diagnostics

When a non-suspending boundary rejects a call, print the shortest inferred path to a suspension primitive:

```text
error[E3102]: C callback `onData` may suspend
  --> src/ffi.ruda:14:1
note: `onData` calls `decodeAndStore`
note: `decodeAndStore` calls `store.write`
note: `store.write` may wait for network I/O
help: enqueue data into the isolate and return from the C callback
```

`ruda explain suspension <symbol>` shows the full effect summary, dynamic-call conservative edges, and source spans.

Task-escape diagnostics name the forbidden destination and owner scope. They suggest waiting, returning the task's completed value, or moving the `scope` block outward; they do not suggest unsafe detachment.

## 9. Runtime panic and task traces

A panic has a stable runtime code, message, source-first stack, task ancestry, and suppressed child failures. Backend helper/state-machine/VM-dispatch frames are hidden by default.

An explicit source `panic(message)` uses `G4000`, preserves the exact runtime
string bytes as its message, and names the complete call expression as its
primary source location. Compiler/runtime-generated failures use distinct codes;
they are not collapsed into `G4000`.

```text
panic[G4002]: array index 8 is outside length 4
  at selectUser (src/users.ruda:42:17)
  at handleRequest (src/http.ruda:19:12)
task 17 spawned at src/server.ruda:88:24
  from task 3 (server scope)
```

Under D045, `G4002` formats a negative or non-negative `int` index as
locale-independent signed decimal and the current array length as unsigned
decimal. Its primary span is the complete failing `array[index]` expression,
including on the left of an assignment. Nested indexing reports the first
failing access. The check occurs before the right-hand side of an indexed
assignment, so that expression has no effects on failure. VM and native traces
use the same primary span and source-first caller frames.

The same controlled program yields equivalent source frames under VM/native. Raw native addresses are available only in a separate debug section and cannot replace source information.

Cancellation is normally not printed as an error. Unexpected forced shutdown, cleanup panic, or leaked resource receives a runtime diagnostic with allocation/open and owning task spans.

Under D041 an already-active panic remains primary while cleanup panics are
rendered as suppressed failures in LIFO observation order. If no panic was
active, the first cleanup panic is primary and later cleanup panics are
suppressed. `closeBestEffort` does not create a cleanup panic; applications
that need fallible-close reporting provide an explicit sink.

## 10. Package and security diagnostics

Dependency conflicts show the shortest paths introducing incompatible constraints. Hash/signature/provenance failures show expected/actual identities without printing credentials. Archive/path failures name the rejected entry in escaped bounded form.

Capability denials name operation, requested resource, root package, and the minimal manifest/CLI grant syntax. They distinguish “not granted” from “cannot be safely represented/enforced on this platform”.

Unsafe/FFI diagnostics explain the specific unchecked invariant. A generic “unsafe required” message without naming pointer lifetime/layout/callback/suspension reason is insufficient.

## 11. Test diagnostics

Test failures include full test name, source assertion, bounded actual/expected structural diff, captured output, deterministic seed/schedule, capabilities, and reproduction command.

VM/native divergence reports the first differing semantic event and both backend/MIR locations. It exits with the dedicated divergence status and is never rendered as an ordinary failed expectation.

Fuzz failures include minimized corpus path/hash, seed, engine, sanitizer, and one-command reproduction. Flaky reruns retain all attempts instead of presenting the final pass alone.

## 12. Recovery and limits

The compiler defaults to a bounded number of primary diagnostics per file/package and reports when more were suppressed. Parser/type recovery placeholders carry an “invalid origin” so downstream phases do not emit misleading secondary errors.

Timeout, memory, nesting, and complexity-limit diagnostics identify the limit and configurable safe alternative where one exists. Internal stack overflow, infinite recovery, or uncontrolled exponential diagnostics are defects, not acceptable handling of hostile source.

## 13. Internal failures

An internal compiler/runtime failure uses an `Ixxxx` code, concise invariant, toolchain/build identity, and sanitized crash-report location. It never tells the user that valid source is their fault.

Where safe, `ruda` writes a minimized reproduction bundle containing only explicitly approved source/config. Automatic network upload is opt-in. Secrets and absolute user paths are redacted.

Internal failures exit `70`; source errors remain `1`.

## 14. Verification

Every diagnostic rule has:

- a compile/runtime/package/test fixture asserting code and primary span;
- human rendering snapshots for representative layouts;
- JSON schema validation;
- Unicode/terminal-width tests;
- fix applicability and stale-source tests when a fix exists;
- cascade suppression tests;
- VM/native source-trace equivalence where runtime behavior is involved.

Diagnostic copy can improve within a stable code. Any change that alters the rule, primary location, or machine fields updates the specification and compatibility corpus.
