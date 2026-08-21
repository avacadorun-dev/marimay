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

Batch checker discovery, read, or defensive traversal/source-limit failures use
`K0004`. A syntactically valid manifest that lacks exactly one usable,
nonempty, NUL-free `module` and `language` identity uses `K0005`. Lexical, parser, resolver, type,
match, and effect failures discovered by `ruda check` retain their originating
codes and exact source spans. If accepted syntax reaches an unimplemented
checker path, `ruda check` emits `I0001` and exits 70; it must never turn that
condition into a successful or diagnostic-free check.

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

Managed-array types are displayed with canonical D054 `[]T` spelling in every
expected/actual type, note, and candidate signature. If `Array<T>` does not
resolve to a visible user/package nominal declaration, the complete type span
receives `T0018` with exact message `prelude managed arrays use '[]T'; replace
'Array<T>' with '[]T'`. This targeted migration error is not emitted for an
ordinary resolved nominal type named `Array`.

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

For a name shared by package-owned nominal behavior and a compiler-known
collection operation, diagnostics use the exact static receiver candidate
set. They do not report an `[]T` mismatch merely because `push` or
`length` is also a compiler-known name. If no exact candidate exists, the
primary error distinguishes a hidden exact declaration, a visible declaration
for another nominal receiver, and an unknown name; an approximate spelling
does not silently select a different collection operation. Named type-only,
named function, and namespace imports retain the same D043 dot-call
diagnostics, while only their available free spelling differs.

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

Under D049, checked integer overflow, division or remainder by zero, and an
invalid shift count use `G4001`. Their locale-independent messages are
respectively `integer overflow`, `integer division by zero`, and
`integer shift count is outside width`. A root `Result.Err(error)` is the
expected-program diagnostic `G4003`, whose message is the exact counted string
returned by the selected structural `message(error)` function. Both classes
exit `1` and write only stderr. An invalid executable entry uses `T0017` and
exit `1`.

Within an `@test` function, a failed compiler-known expectation uses `G4004`.
For `expect.output`, its counted message contains a bounded deterministic
expected/actual byte diff; the captured bytes do not escape to program stdout.
The test runner classifies this as a failed expectation rather than an engine
divergence. VM and native must preserve the same code, message, assertion span,
and cleanup behavior.

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

Under D053, use of an array iterator after an initialized-sequence mutation
panics with `G4005` and the exact locale-independent message `array iterator
invalidated by mutation`. The primary span is the complete
`for const name of expression` header. `reserve` cannot produce this panic;
empty/no-op/error mutation paths do not change the generation. VM and native
preserve the same code, message, span, and caller frames.

Cancellation is normally not printed as an error. Unexpected forced shutdown, cleanup panic, or leaked resource receives a runtime diagnostic with allocation/open and owning task spans.

Under D041 an already-active panic remains primary while cleanup panics are
rendered as suppressed failures in LIFO observation order. If no panic was
active, the first cleanup panic is primary and later cleanup panics are
suppressed. `closeBestEffort` does not create a cleanup panic; applications
that need fallible-close reporting provide an explicit sink.

## 10. Package and security diagnostics

Dependency conflicts show the shortest paths introducing incompatible constraints. Hash/signature/provenance failures show expected/actual identities without printing credentials. Archive/path failures name the rejected entry in escaped bounded form.

Local source-package import resolution uses distinct resolver identities. An
unknown supplied-local package path is `R0008`; a deterministic dependency
cycle is `R0009`; an absent public item or namespace member is `R0010`; an
existing but package-private item or member is `R0011`; and a duplicate named
or namespace binding, including a collision with a package declaration, is
`R0012`. The primary span is respectively the import string, the
import edge that closes the canonical cycle, the item or member name, the
denied item or member name, or the later conflicting binding. Diagnostics for
`R0009` include the bounded canonical package path cycle. A non-owning package
cannot add a declaration to the D043 set;
using such a declaration as a dot call is rejected by the ordinary D003 call
resolution diagnostic.

Named imports and namespace imports are file-local. Changing between a named
type import, a named type-plus-function import, and a namespace import must not
change the D043 public owned dot-call set or structural interface conformance.
A diagnostic may therefore reject an unavailable free spelling, but it must
not describe the same public owned dot call as absent merely because its
function name was not imported. As in section 7, no import diagnostic suggests
an `impl` block.

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

For D049 execution, failure to lower, encode, decode, or verify the canonical
typed-MIR program is `B0001`. A post-verification VM resource, integrity, or
unsupported-host failure is `I0002`. Both are internal failures and exit `70`;
neither may be rendered as a program panic or written to program stdout.

For D050 native construction, `B0002` means the distribution-selected host C
toolchain could not be started or did not pass its preflight; it is an expected
build failure and exits `1`. `B0003` means verified typed MIR could not emit
the compiler-owned C17 unit, and `B0004` means a supported compiler rejected
that generated unit; both are internal failures and exit `70`. `B0005` is a
bounded cache/path/atomic-publication failure and exits `1`. `B0006` rejects a
host that cannot preserve the required native semantics and exits `1` before
publishing an artifact. Compiler output is bounded diagnostic evidence, never
an unstructured replacement for these primary codes.

For D051 engine-equivalence testing, `B0007` means the VM observation and the
native replay differ in termination, exact D046 stdout, or exact panic/stderr
bytes. It is emitted once for the first source-ordered divergent test and exits
`5`. A matching nonzero test result, including `G4000` or `G4004`, is not
`B0007`; it is an ordinary failed test and exits `1` after deterministic
reporting. Test-pipeline integrity/resource failures remain `I0002`/exit `70`.

For D052 runtime-profile selection, `B0008` means the distribution's selected
native runtime archive or manifest is missing, partial, stale, or incompatible
with the requested profile, runtime version, host, or compiler identity. It is
an expected toolchain-installation failure and exits `1` before C emission or
host linking. The primary diagnostic gives one reinstall action; raw linker
output cannot replace it.

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
