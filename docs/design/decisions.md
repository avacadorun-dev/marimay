# Architectural decisions

Status values: `accepted`, `provisional`, `deferred`, `rejected`.

## D001 — Name, file extension, and command

Status: accepted; external distribution clearance remains gated by N0.

The official language and product name is **Marimay**. English prose, code-adjacent prose, product metadata, and technical identifiers spell it `Marimay`. The canonical short technical stem is `mmay`; the full machine-readable language and schema identifier is `marimay`.

The canonical toolchain executable is `mmay`, source files use `.my`, bytecode bundles use `.myb`, project manifests are `mmay.mod`, `mmay.work`, and `mmay.lock`, and tool-owned local state uses `.mmay/`. C functions, build targets, environment variables, include paths, install paths, and cache/runtime artifacts use the corresponding `mmay`/`MMAY` stems. JSON schemas and Markdown/editor language IDs use `marimay`.

This is an atomic, intentionally incompatible pre-release rename from the historical **Ruda** identity and its `ruda`, `.ruda`, `.rudab`, `ruda.mod`/`ruda.work`/`ruda.lock`, and `.ruda/` artifacts. Those names, and the still earlier **Malang**/`ma` identity, receive no executable alias, source fallback, dual manifest discovery, old-bytecode decoder, warning period, or converter.

The rename changes identity-dependent suffixes, wire magic, schema IDs, and cache namespaces, but it changes no source syntax or semantics, manifest/package data model, MIR opcode IDs, runtime behavior, or observable program behavior. The repository has no published compatibility contract or external users. This accepted text supersedes D001's historical Malang and Ruda identities; no other accepted decision is superseded.

N0 remains a release blocker for trademark/legal review, domains, public handles, organization and registry namespaces, package/module prefixes, signing identity, and independent verification of `mmay` across supported PATH/package-manager environments. D114 separately permits the owner's explicitly authorized experimental macOS preview without claiming N0 or CLI0 completion. Accepting `Marimay` here does not claim that any domain, external account, package, namespace, or trademark is available, reserved, controlled, or legally cleared; `marimay.example` is only a reserved-TLD placeholder.

## D002 — Familiar C-family surface, Marimay-native forms

Status: accepted.

Use `fn`, `const`, `let`, `name: Type`, `<T>`, braces, and named imports because they are readable without a glossary. Give Marimay's core constructs their own regular forms: `type User { ... }` for records, `User { ... }` for construction, `Variant { ... }` for named enum payloads, `$"...{value}..."` for interpolation, `public` for every public declaration, and control-flow conditions without mandatory parentheses. Keep semicolons explicit where statements or signatures require them.

Do not inherit browser-language semantics: no coercion, prototypes, classes, `any`, nullish values, truthiness, automatic semicolon insertion, open record-shape assignment, or dynamic escape tier. Familiar punctuation is not a compatibility promise.

Reason: familiarity should reduce reading cost without weakening runtime guarantees.

## D003 — Ordinary functions with restricted first-parameter dot calls

Status: accepted.

Behavior is declared by an ordinary top-level function with an ordinary, meaningfully named first parameter:

```marimay
fn string(user: User): string {
  return user.name;
}
```

`user.string()` and `string(user)` resolve to the same symbol and evaluate `user` once. A function is dot-call eligible only when it is top-level, visible at the call site, and its first parameter is the exact nominal type owned by the declaring package. A package cannot add dot-call behavior to a foreign nominal type.

There is no general overload facility. The same function name may be declared more than once only when every declaration is dot-call eligible and their first nominal parameter types are distinct. Interfaces are satisfied structurally by the derived dot-call set after removing that first parameter from each eligible signature. There is no special method keyword, method body inside a record, or `impl` declaration.

Reason: one declaration should support free composition, discoverable dot notation, and interface conformance. A second behavior hierarchy would repeat information already present in the function signature. Package ownership supplies a simple coherence boundary without a general trait solver.

## D003a — Compact core declaration and task-scope keywords

Status: accepted.

Functions are declared with `fn`. Structured concurrency uses `scope { ... }`; `spawn` remains the only operation that creates a child task. `scope` is intentionally not named after parallelism: tasks in one isolate may interleave without running simultaneously.

Reason: these constructs are common enough to deserve low typing cost. `fn` is established and unambiguous; `scope` names the lifetime guarantee that matters more than the scheduling mechanism.

## D004 — Nominal records/enums, structural interfaces

Status: accepted.

Record and enum identity follows declarations, not field shape. Interface satisfaction uses exact dot-call signatures. Interfaces contain call signatures only; 1.0 has no fields, constructors, associated types, default bodies, negative conformance, or specialization.

Reason: this keeps domain data distinct while retaining Go-like decoupling at behavior boundaries and a bounded solver.

## D005 — Algebraic enums and exhaustive match

Status: accepted.

Enums are closed nominal sums with unit, positional, or named payloads. `match` is an expression. A match over a closed enum must cover every reachable case unless an unguarded wildcard is present. Guards do not contribute to exhaustiveness.

## D006 — No null; typed expected failure

Status: accepted.

Safe Marimay has no null-like value. Absence is `Option<T>`. Recoverable failure is `Result<T,E>`. Postfix `?` propagates an `Err` from a function with the same error type. Error conversion is explicit; ordinary structural conversion to a declared interface remains available at an explicit construction/return boundary. Panic represents a violated invariant, not routine control flow.

## D007 — Managed reference data with immutable fields by default

Status: provisional, prototype gate required.

Nominal record values have managed reference identity. Fields are immutable after construction unless marked `mutable`. `const`/`let` govern rebinding of a local, not deep object mutability. Strings and enum values are immutable. Arrays and maps are managed mutable containers.

Reason: this avoids source-level ownership and accidental large copies while supporting graphs, services, and FFI handles. The single-threaded isolate rule prevents data races within one heap. The prototype must measure allocation behavior and confirm that escape analysis can stack-allocate or scalar-replace common short-lived records. The required workloads include decoded/encoded JSON DTO graphs, immutable UI-style trees, request/response models, and collection-heavy transformations; microbenchmarks alone cannot accept D007.

Rejected alternative: implicit copy-on-write value semantics for every record. It makes alias behavior attractive but imposes substantial hidden runtime and FFI complexity. Marimay may later add an explicitly declared compact `value type`; it will not change `type` semantics.

## D008 — One frontend and typed MIR, two required engines

Status: accepted.

Stage 0 implements a typed bytecode VM and a C17 native backend from one canonical MIR. VM is the default for `run` and `test`; native is the default for `build`. Engine choice cannot change accepted programs or observable language behavior.

Reason: fast interactive work and standalone deployment have different performance needs but do not require different languages.

## D009 — C17 bootstrap then self-hosting

Status: accepted.

The first compiler, VM, garbage collector, task runtime, bytecode verifier, formatter, and test harness are implemented in portable C17. After the language reaches the bootstrap gate, the compiler is rewritten in Marimay. Reproducible C17 Stage 0 remains the root of trust and must be able to build Stage 1.

## D010 — No source-level async color

Status: accepted, high-risk implementation gate.

Ordinary functions may call suspending standard-library operations. `spawn call()` explicitly creates concurrency, but functions are not declared `async`, calls do not return `Future<T>`, and users do not select an executor. The compiler infers a transitive suspension effect and lowers suspending frames in both engines. The effect is visible in compiler diagnostics and metadata but is not part of source-level function compatibility.

Reason: direct-style code preserves Go's usability while avoiding Rust's sync/async ecosystem split. This moves complexity into the compiler/runtime and therefore requires a dedicated ABI, debugger, cancellation, and FFI prototype before the syntax freezes.

## D011 — Lexically scoped tasks

Status: accepted.

`spawn` is legal only inside `scope { ... }`. A `Task<T>` handle cannot escape that block, be stored in heap data, or cross an isolate. Leaving the block for any reason cancels unfinished children and joins them before control continues. Child tasks form an observable tree.

Reason: unstructured goroutine lifetime is a documented leak and reliability hazard. Structure makes ownership of work explicit without ownership types.

## D012 — Concurrency inside isolates; parallelism across isolates

Status: provisional through 1.0 performance validation.

Each isolate has one heap and one cooperative scheduler bound to one OS thread at a time. Safe code cannot share mutable heap objects between isolates. Inter-isolate channels copy immutable/sendable values or transfer supported buffers/resources, invalidating the sender's handle through a compiler-known operation.

Reason: data-race freedom without a borrow checker or pervasive locks. The cost is explicit isolation for CPU parallelism and serialization/transfer overhead, which must be benchmarked against target service workloads.

## D013 — Shape-shared generics

Status: provisional, benchmark gate required.

Generic code is generated once per ABI representation shape by default (`i64`, `f64`, managed reference, and aggregate shapes as defined by MIR). Runtime type dictionaries provide operations required by constraints. The optimizer may specialize hot concrete instantiations without changing observable semantics.

Reason: avoid Rust-style mandatory per-type monomorphization costs while keeping primitives unboxed on common paths.

## D014 — No arbitrary compile-time code in 1.0

Status: accepted.

There are no user-defined syntax macros, procedural macros, import hooks, package install scripts, or implicit build scripts. A small closed set of compiler derives may generate predictable functions. External code generation is an explicit `mmay generate` action and generated files are ordinary reviewed inputs.

## D015 — Canonical formatting is non-configurable in 1.x

Status: accepted.

`mmay fmt` has one style and is idempotent. Projects cannot configure brace, indentation, semicolon, or line-width policy. Compatibility exceptions require a language-versioned formatter change.

## D016 — Deterministic safe semantics

Status: accepted.

Evaluation order is left to right. Signed and unsigned integer overflow traps in every build mode; explicit wrapping operations live in the numeric API. Map iteration is insertion-ordered. Safe code has no undefined behavior. Clock, randomness, scheduling, and external I/O are modeled as nondeterministic inputs and can be controlled by the test runtime.

## D017 — Package builds are hermetic by default

Status: accepted.

The lock file pins immutable artifacts and cryptographic hashes. Build and test do not access the network after dependency fetch. Dependencies cannot execute installation code. The root application declares ambient runtime capabilities; libraries cannot silently broaden them.

## D018 — Exceptions and inheritance

Status: rejected for 1.x.

Marimay has no catchable exceptions for expected failure and no class inheritance. `Result`, exhaustive enums, interfaces, composition, and `defer` cover the intended use cases with visible control flow.

## D019 — JIT

Status: deferred.

The mandatory VM is initially an interpreter over typed bytecode. A tracing or method JIT may be added only after VM/AOT conformance and profiler evidence identify a need. JIT support cannot introduce a third semantic tier or become required for acceptable startup.

## D020 — User-defined value types and deterministic destruction

Status: deferred.

Compact stack-oriented values and non-GC resource types may be valuable for games, embedded systems, and zero-copy FFI. They interact with aliasing, suspension, cancellation, and self-hosting. 1.0 uses managed records, opaque standard-library resources, and `defer`; a later proposal must preserve the no-lifetime-annotation goal.

## D021 — Diagnostics without source locations

Status: accepted.

Every diagnostic keeps the same schema, code, severity, message, labels, notes, and fixes fields. The required `primary` field contains a source span when a meaningful source snapshot owns the failure and is `null` otherwise. A non-source diagnostic may carry a typed `context` object, such as a command-line argument index, package identity, bytecode section, or runtime resource.

Reason: command invocation, dependency transport, corrupt bundle, capability, and process-level failures often have no honest source byte range. Inventing a pseudo-file or unrelated source span would make machine output look more precise while weakening it. Requiring the nullable field preserves streaming schema shape and lets consumers distinguish “no source owner” from an accidentally omitted field.

## D022 — Exact source bytes and logical newline positions

Status: accepted.

Source snapshots retain physical bytes unchanged. Diagnostic byte offsets address those bytes, including an optional UTF-8 BOM at offsets 0–2. A BOM is ignored for tokenization and Unicode-scalar columns, so the first byte after it is line 1, column 1. LF, CRLF, and a lone CR each form one logical newline. An offset on any byte of a newline sequence maps to the preceding line's end position; the offset immediately after the complete sequence maps to column 1 of the next line.

Reason: editor overlays, fixes, and hashes need exact bytes, while diagnostics and formatting need stable positions across common newline encodings. Treating CRLF as two lines or counting an accepted BOM as a visible column would make the same text report different logical locations after canonical formatting.

## D023 — Versioned Unicode identifier profile

Status: accepted.

Language version `0.1-design` uses the Unicode 17.0.0 `XID_Start` and `XID_Continue` derived properties and the default identifier form from UAX #31 requirement R1-1: `XID_Start XID_Continue*`. Source code points are compared exactly and are not normalized by the compiler. A standalone ASCII `_` is a dedicated wildcard/discard token; because `_` is not `XID_Start`, `_name` is not an identifier, while an underscore may occur after a valid start character.

The bootstrap consumes generated, committed range tables derived from the versioned Unicode Character Database. Their source URL, version, and SHA-256 are committed beside the generator. Release configure/build/test does not download Unicode data. Changing the Unicode version is a language-versioned compatibility change with identifier-boundary fixtures; it must not follow the host C library or generator runtime implicitly.

Reason: XID membership grows between Unicode releases. Pinning the data makes lexical acceptance reproducible on every host, while UAX #31's recommended XID profile gives normalization-closed identifier classes without silently rewriting user spelling.

## D024 — Closed literal and escape spelling for 0.1

Status: accepted.

The 0.1 lexer uses one closed, locale-independent literal grammar. Decimal digits are ASCII. `_` may separate digits only when a valid digit for that base appears on both sides. Binary, octal, and hexadecimal integers use lowercase or uppercase `0b`, `0o`, and `0x` prefixes. Decimal floats require digits on both sides of `.` when a decimal point is present; an exponent uses `e` or `E`, an optional sign, and at least one decimal digit. Hexadecimal floats and numeric suffixes are not in 0.1.

Normal strings and runes accept simple escapes `\\`, `\"`, `\'`, `\n`, `\r`, `\t`, and `\0`, plus `\u{H...}` with one to six hexadecimal digits naming a Unicode scalar. Byte strings accept the simple escapes and `\xHH` with exactly two hexadecimal digits; their unescaped content is ASCII. Raw strings perform no escape processing. No 0.1 quoted literal contains an unescaped physical newline. A rune contains exactly one Unicode scalar or one escape.

Interpolated string text follows normal-string escapes. `{{` and `}}` are literal braces, a single `{` starts an expression, and the balancing single `}` ends it. The lexer retains interpolation boundary tokens and enforces explicit nesting limits.

Reason: the baseline already promises targeted malformed-escape and numeric diagnostics but did not define their accepted spellings. A closed grammar prevents dependence on C locale/library parsing and leaves multiline/hash-delimited/raw variants and numeric suffixes available for a later language version.

## D025 — Named enum payload declaration separators

Status: accepted.

Fields inside a named enum payload declaration use the same `name: Type;` signature form as record fields. The closing brace is followed by the enum variant's optional comma: `InvalidDigit { offset: int; },`. Commas continue to separate enum variants, construction fields, and pattern fields; they do not replace the semicolon that terminates a payload field signature.

Reason: the language grammar and record/enum declaration examples already use semicolon-terminated field signatures, while several standard-library and tour examples accidentally omitted those semicolons. One declaration-field rule is easier to parse, format, and teach than a separator change based only on which nominal declaration owns the fields.

## D026 — Deterministic formatter column metric

Status: accepted.

The canonical formatter uses Unicode 17.0.0 scalar properties rather than host locale, terminal, font, or `wcwidth`. A scalar in `Grapheme_Extend` or `Default_Ignorable_Code_Point` contributes zero formatter columns. A remaining scalar with East Asian Width `W` or `F` contributes two columns. Every other valid scalar, including East Asian Width `A`, contributes one column. A hard line resets the column to zero; the target width is 100 columns.

The metric sums scalar contributions and deliberately does not perform font shaping or claim the rendered width of an extended grapheme cluster. Source spelling remains exact under D023, so canonically equivalent but differently spelled unbreakable tokens may have different measured widths. Unicode inputs and generated tables are committed and hash-pinned; changing the Unicode version or metric is a language-versioned formatter compatibility change.

Reason: “Unicode display columns” is otherwise host- and terminal-dependent, especially for ambiguous-width characters, combining marks, variation selectors, and emoji sequences. Canonical formatting must produce identical bytes on every Tier 1 platform. Unicode UAX #11 explicitly requires contextual treatment of ambiguous width and notes that combining-mark East Asian Width is not advance width, so Marimay defines the non-East-Asian deterministic tailoring above rather than inheriting an OS policy.

## D027 — Manifest syntax and formatter gate ownership

Status: accepted.

I1 owns the bounded syntactic parsers and canonical formatters for `.my`, `mmay.mod`, and `mmay.work`, including their use by no-argument `mmay fmt`. I7 reuses those parsers and adds manifest semantic validation, dependency resolution, canonical `mmay.lock`, and package commands; it does not introduce a second manifest grammar or formatter. A syntactically valid manifest may therefore be formatted before its module names, version constraints, capabilities, or workspace membership can be semantically resolved.

Reason: the toolchain contract assigns no-argument canonical formatting to I1, while the original I7 roadmap wording also assigned all manifest parsing/formatting to I7. Leaving both statements in place would either make the I1 gate unverifiable or encourage a temporary formatter grammar that later drifts. One early hostile-input syntax implementation and one later semantic consumer preserve the public command contract and eliminate duplicate parsers.

## D028 — Deterministic formatter target discovery

Status: accepted.

No-argument `mmay fmt` finds the nearest regular `mmay.mod` on the physical working-directory ancestor chain and formats that module tree. It also formats the nearest regular `mmay.work`, if one exists from the module root upward. A module-tree walk visits regular `.my` files and the root manifests in unsigned UTF-8 path order, does not follow symbolic links, ignores tool-owned `.git` and `.mmay` directories, and does not enter a nested directory that owns another `mmay.mod`. An explicit regular file formats only that recognized `.my`, `mmay.mod`, or `mmay.work` input; an explicit directory uses the same bounded physical-tree rules rooted at that directory.

Directory-entry count, nesting, path bytes, and total selected files are bounded before unbounded allocation or recursion. Each file retains the existing per-file transactional rule; tree formatting does not claim an all-filesystem atomic commit. Discovery/traversal failures are formatter input failures, and a symbolic link explicitly selected as the target is rejected rather than followed.

Reason: root discovery and tree selection affect which user files a write-capable command mutates. Leaving symlinks, nested modules, hidden tool state, ordering, and bounds to host directory APIs would make `mmay fmt` nondeterministic and could rewrite sources outside the selected module. Physical no-follow traversal is portable, auditable, and independent of the later package resolver.

## D029 — Structured race and readiness selection before 1.0

Status: accepted, prototype gate required.

Direct-style concurrency must provide both structured first-completion racing and readiness selection before I6 is verified. A race owns its competing operations in a nested scope, cancels and joins losers, and cannot return while losing work remains detached. A selection waits on a finite set of task completions, bounded-channel operations, timers, or cancellation-aware I/O registrations, removes losing registrations, and does not implicitly cancel independently owned tasks. Deterministic test mode resolves simultaneously ready alternatives by declared source order and records the choice for VM/native replay.

These are compiler-aware first-party library/runtime operations, not new source syntax in 0.1. Exact API spelling is frozen only after the I6 prototype covers fan-out requests, hedged calls, timeout-plus-channel loops, closed channels, simultaneous readiness, cancellation during registration, and cleanup/panic paths.

Reason: `spawn`, `wait`, and a single-operation timeout do not express common server coordination without hand-built channels and cancellation plumbing. Requiring structured race/select semantics preserves D011 lifetimes while avoiding detached winner-takes-all work.

## D030 — Native toolchain delivery is a product responsibility

Status: accepted.

The required C17 AOT backend remains the auditable Stage 0 native path, but ordinary Tier 1 users must not configure compiler, linker, SDK paths, or cache keys manually. Each official distribution provides one tested zero-manual-configuration host-build path: a version-matched managed toolchain where redistribution permits it, or verified platform-toolchain discovery plus an installer/preflight flow that performs or precisely requests the one unavoidable platform installation step. `mmay build` itself remains offline and deterministic after installation.

Missing or incompatible native tools produce one actionable diagnostic and a machine-readable `mmay doctor` result; they never fail later as raw C compiler/linker output. Cross-compilation may require an explicitly installed target pack. A future direct object-code backend may remove the external C toolchain from day-to-day builds only after it reaches the same safety, portability, and VM/native differential gates; it does not weaken the required C17 bootstrap root.

Reason: exposing generated C is valuable for audit and bootstrap, but making users assemble a C development environment would break the single-command `mmay build` product promise. Packaging and discovery complexity belongs to the toolchain, not each Marimay project.

## D031 — Width-sensitive braced list layout

Status: accepted.

Import name lists, record constructions, record/named-variant patterns, and named enum payload declarations are formatter groups rather than unconditionally multiline blocks. If the complete group fits within the D026 100-column metric and contains no hard-line comment, canonical output keeps it on one line with one space inside each brace. Otherwise it breaks after `{` and before `}`, indents one item per line, and emits a trailing comma for comma-separated lists. Named enum payload declaration fields retain D025 semicolons and do not gain an internal comma.

Fitting uses canonical text and spacing under D026; original source byte
length and whitespace do not impose a separate pattern-layout threshold.

Declaration bodies, control-flow bodies, `scope`, `unsafe`, `match`, test/fuzz bodies, and ordinary statement blocks remain structurally multiline even when short. An empty grouped brace pair remains `{}`.

Reason: treating record-like lists as statement blocks expands common DTO construction and match patterns into unnecessary vertical space, while always keeping them inline makes large data shapes unreadable. The existing deterministic document model already has an exact fit decision, so one width-sensitive rule preserves both scanability and cross-platform canonical bytes.

## D032 — Canonical ordinary and raw string spelling

Status: accepted.

The formatter decodes every valid ordinary or raw string before choosing its
canonical spelling. Ordinary strings emit valid non-control Unicode scalars
literally, use `\\`, `\"`, `\n`, `\r`, `\t`, and `\0` for those exact values,
and use the shortest lowercase `\u{h...}` spelling for remaining C0/C1 control
scalars. An escaped apostrophe becomes a literal apostrophe because it needs no
escape inside a double-quoted string.

Raw syntax is canonical in expression-literal positions only when the decoded
value can be represented by the 0.1 raw-string grammar and contains at least
two backslashes that an ordinary string would have to double. With the one-byte
`r` prefix this is exactly the case where raw syntax is strictly shorter; one
or zero doubled backslashes remain an ordinary string. Grammar positions that
specifically require an ordinary string token—such as import paths and the
extern ABI—remain ordinary. A quote, physical newline, or C0/C1 control
scalar makes raw spelling ineligible. Byte strings, runes, and
interpolated-string text retain their distinct literal forms.

Reason: the previous phrase “unless raw syntax materially reduces escaping”
did not define an interoperable byte result. A decoded-value rule makes
formatting idempotent across hosts, removes non-canonical escape choices, and
keeps raw strings for the path/regular-expression cases where they actually
reduce source noise without changing the runtime string value.

String literal patterns use that same decoded-value identity. Two spellings
that decode to the same UTF-8 bytes are the same pattern, including embedded
zero bytes; matching applies no locale collation, case folding, or Unicode
normalization. Because the string domain is not finite, literal arms alone are
never exhaustive: an unguarded wildcard or binding must cover the remainder.

## D033 — Exact loop-header forms

Status: accepted.

Marimay has exactly two 0.1 `for` headers. Iteration uses
`for const name of expression`; its binding is immutable and must be an
identifier. The C-style form uses
`for let name = initializer; condition; step`; all three expressions are
required, the initializer declares exactly one `let` binding, and the binding
must be an identifier. The forms do not mix: `let name of value`, a `const`
C-style initializer, an arbitrary expression initializer, omitted clauses, and
`_` loop bindings are not accepted syntax.

Reason: the two explicit forms cover collection iteration and counter/state
loops while preserving definite initialization and avoiding the punctuation-
heavy family of optional C clauses. Requiring `const` for iteration makes the
per-iteration binding contract visible; requiring `let` for the stepped form
makes its intended mutation visible. This decision freezes the exact syntax
already used by the normative control-flow examples and supersedes the
provisional state of `simple_for_header`; it does not define iterator or
counter runtime semantics beyond the existing language contract.

## D034 — Semantic facts do not encode tooling trust policy

Status: accepted.

The Marimay language owns program meaning and static validity: types, memory and
error behavior, arithmetic, control flow, task lifetimes, and any effect that
is proven to be part of a function's semantic contract. The compiler proves
those rules and exposes stable factual outputs. First-party tools may use the
same typed AST, symbol graph, effect metadata, MIR, and diagnostics for
formatting, semantic diff, dependency analysis, testing, fuzzing, vetting, and
editor features.

Repository and CI policy owns authorship, generated-code provenance,
attestation, required coverage, review authority, and release admission.
Runtime and operating-system layers own sandbox enforcement and resource
limits. Marimay source has no author/agent marker, trust level, coverage promise,
or attestation construct. Co-locating compiler, formatter, tests, audit, and
package commands under the `mmay` executable does not make every command a
language feature.

A future source-level capability annotation is justified only if the
capability set changes the statically checked semantic contract of calls and
function values. A package/network/filesystem admission policy remains in the
manifest, runtime grant, sandbox, or CI layer and must not be added to syntax
merely to make an audit policy look language-enforced.

Reason: Marimay should be a small language with precise semantics from which
tools can reliably extract facts. It cannot determine an author's intent or
replace engineering governance; mixing those concerns would expand the type
system while weakening the responsibility boundary.

## D035 — Exact checked shift semantics

Status: accepted.

The binary shift operators require integer operands of the same exact type;
an integer literal may receive that type from the other operand under the
ordinary contextual-literal rule. The result has the left operand's type. A
shift count is valid only in the half-open range from zero to the bit width of
that type. A negative signed count or a count greater than or equal to the
width panics in every build mode.

`left << count` is checked arithmetic. It produces the mathematical value
`left * 2^count` only when that value is representable in the operand type;
otherwise it panics. It does not silently discard high bits. Explicit
wrapping shift operations belong to the numeric API alongside the other
wrapping operations.

`left >> count` never overflows. For an unsigned type it shifts in zero bits.
For a signed two's-complement type it is an arithmetic right shift that copies
the sign bit; this is a language rule and does not inherit C's
implementation-defined behavior. Compound `<<=` and `>>=` have exactly the
same operation and panic behavior before assigning the result.

Reason: “out-of-range shift” previously fixed only part of the contract and
left signed right shift plus left-shift overflow vulnerable to backend drift.
Checked left shift follows D016's default arithmetic rule, while explicit sign
extension and count validation give VM and portable C17 backends one
testable result independent of host operators.

## D036 — Stable explicit-panic identity

Status: accepted.

The source expression `panic(message)` requires exactly one value of type
`string`, has type `never`, and uses stable runtime code `G4000`. Its primary
source location is the complete call expression, while its message is the exact
runtime string value, including embedded zero bytes. It is distinct from
compiler/runtime-generated panics such as bounds, arithmetic, assertion,
capability, and resource failures, which have their own stable codes.

Reason: VM/native equivalence and `expect.panics` compare panic identity, not
only process failure. Fixing the explicit-source code and source span before
backend work prevents each engine or test harness from inventing a different
classification, while leaving the richer frame/task rendering to the runtime
and diagnostics layers.

## D037 — Declared `never` is a non-returning call contract

Status: accepted.

A named function may explicitly declare result type `never`. Every reachable
path in its body must terminate without producing a normal return value. After
its arguments have been evaluated left to right, a call resolved to that
signature has type `never` and has no normal continuation in typed MIR. It is
not a call returning a fabricated sentinel, zero-sized value, `void`, or
backend-specific unreachable value.

Reason: `never` was already a primitive bottom type and was compatible with
every return position, but leaving its call boundary implicit permits one
backend to synthesize a value or fallthrough that another rejects. A dedicated
terminating-call edge lets the checker and every engine agree without requiring
the compiler to prove whether abnormal completion is an explicit panic,
unbounded recursion, cancellation, or another defined non-returning operation.

## D038 — Minimal hard-keyword boundary and ordinary test/fuzz/panic names

Status: accepted.

Marimay reserves a spelling globally only when the parser needs it to determine
program structure without surrounding context. The audited 0.1 hard-keyword
set is `defer false if match return spawn true while`. The parser treats
`as break case const continue else enum extern fn for from import interface let
mutable of public scope type unsafe yield` contextually: each retains its
grammatical role where that role is unambiguous and otherwise becomes an
ordinary identifier token in the syntax tree. Hard keywords remain unavailable
as declaration, field, member, or local names in 0.1; Marimay does not add a
separate escaped-identifier syntax at this stage.

The disambiguating forms are part of the grammar contract. `break;` and
`continue;` are loop control; a call/member/postfix form is an identifier.
`const`/`let` begin a binding only with a following binding name or discard.
`for` begins a loop only with a valid or recoverably intended loop header.
`scope { ... }` and `unsafe { ... }` are statements at a statement boundary;
the same spelling remains a name in expression context (for example
`const value = scope {};`).
Top-level `import`, `type`, `enum`, `interface`, `extern`, `fn`, and `const`
retain their declaration roles at a declaration boundary. `else` attaches only
when followed by `if` or a block. These rules keep malformed constructs
recoverable without reserving the same words across every API namespace.

`test`, `fuzz`, and `panic` are ordinary identifiers. Tests and fuzz targets
are ordinary package-private functions marked with the closed compiler
attributes `@test` and `@fuzz`; the former requires `fn(): void`, and the latter
exactly one typed parameter plus `void`. Both attributes are legal only in a
source file ending `_test.my`; normal builds exclude those files while test,
fuzz, and explicit all-targets checks include them. The unshadowed prelude declaration
`fn panic(message: string): never` is compiler intrinsic, but normal lexical
and package name resolution may shadow it. Its stable D036 runtime identity
applies only after resolution selects that prelude symbol.

`yield expression;` is recognized contextually as the match-arm transfer form
and remains invalid outside a block match arm. `yield()`, `task.yield()`, and
declarations named `yield` are ordinary identifier uses. `in`, `any`, `async`,
`await`, `class`, `extends`, `impl`, and `throws` are also ordinary identifiers:
Marimay omits the corresponding type or construct by grammar/semantic rules, not
by preemptively occupying API vocabulary. Only `null` and `undefined` remain
reserved sentinel spellings so ordinary resolution cannot introduce a
null-like value tier.

Reason: every globally reserved word creates permanent collisions for domain
models, generated bindings, foreign APIs, and standard-library functions.
Keeping tool discovery in attributes, invariant failure in the prelude, and
ambiguous control words contextual makes the semantic core smaller without
weakening null safety, structured matching, diagnostics, or toolchain
behavior. This supersedes D003a only where that decision called all compact
surface spellings “keywords”; its accepted `fn`/`scope`/`spawn` forms and task
semantics are unchanged. It also supersedes the keyword/test/fuzz/panic portions
of the original baseline consistency audit and D036's implicit assumption that
the intrinsic spelling needed special lexical status. The complete follow-up
audit supersedes D038's initial 20-hard/9-contextual partition; it does not
change the syntax or meaning of any construct.

## D039 — C17 generated identifiers are compiler-owned

Status: accepted.

No user-controlled Marimay identifier is copied verbatim into a generated C
identifier. The C17 backend assigns functions, types, field accessors, locals,
temporaries, descriptors, and helpers injective compiler-owned spellings from
stable symbol identity plus bounded ordinals. A generated spelling is valid
C17, avoids every C keyword and implementation-reserved namespace, cannot
collide with runtime helpers or C-library external identifiers, and remains
distinct under target filesystem case folding and backend length limits.
Unicode source spelling and normalization-distinct Marimay names remain distinct.

Explicit `extern "c"` link names are not internal generated identifiers. They
are validated foreign ABI contracts and are never silently mangled; I9 owns
their platform-specific linkage and library validation.

Reason: reserving C vocabulary in Marimay would leak the temporary Stage 0 backend
into source semantics, break generated bindings, and still fail to cover C's
underscore-reserved, library, runtime-helper, case-folding, and truncation
collision spaces. A compiler-owned namespace closes the entire boundary and
keeps VM/native source acceptance identical. This decision does not freeze
human-readable emitted-C symbol spelling or make generated C a public ABI.

## D040 — Construction source order is semantic and formatter-preserved

Status: accepted.

In record construction and named enum-payload construction, a field name
selects its destination while the field's position in source selects when its
initializer is evaluated. Initializers execute exactly once from left to right
in written order, including when that order differs from declaration order.
The formatter preserves field order and changes only layout, whitespace,
comments, trailing separators, and other spelling choices that do not reorder
expressions. It does not use name or type resolution to format a construction.

Reason: reordering `Pair { second: next(), first: next() }` changes which
effectful result is stored in each field even though both constructions name
the same destinations. Canonical formatting must preserve program meaning and
remain available from the lossless syntax tree before semantic resolution.
This supersedes the former language §5.4 sentence that made construction order
irrelevant and allowed declaration-order canonicalization; it narrows D031's
braced-group rule without changing its width or line-breaking policy.

## D041 — Deferred cleanup keeps failure policy explicit

Status: accepted.

`defer` schedules only a call whose static result type is `void`; it does not
implicitly discard or unwrap `Result`, and no cleanup result can implicitly
replace a function's declared result. Fallible resources expose a fallible
operation such as `File.close(): Result<void, fs.Error>` for failures that the
program must observe, plus a separately named infallible fallback such as
`File.closeBestEffort(): void` when the program intentionally chooses release
attempt over error propagation. `closeBestEffort` is idempotent, does not
panic, and never replaces an in-flight fallthrough, `return`, `Err`, panic, or
cancellation outcome. Applications that must observe cleanup failure on every
exit defer an ordinary `void` helper that matches the `Result` and reports it
to an explicit sink.

Deferred calls run exactly once in LIFO order on every outward edge. Cleanup
runs with an already-requested task cancellation masked until the registered
calls finish. If an arbitrary deferred `void` call panics, remaining deferred
calls are still attempted. The first cleanup panic becomes the primary outcome
when the prior outcome was fallthrough, return, expected `Err`, loop transfer,
or cancellation; an already-active panic remains primary, and cleanup panics
are attached to it as suppressed failures in LIFO observation order.

Reason: treating `Result` specially inside `defer` would hide an error mapping
and create exit-dependent return semantics. An ordinary fallible operation,
an explicitly named best-effort fallback, and deterministic panic precedence
keep resource policy visible without adding block-defer syntax or exceptions.

## D042 — Exact signed division and remainder

Status: accepted.

For every signed integer width, `/` computes the mathematical quotient rounded
toward zero. `%` returns `left - (left / right) * right`; a nonzero remainder
therefore has the sign of the left operand and magnitude less than the
magnitude of the right operand. A zero right operand panics for both operators.
`MIN / -1` and `MIN % -1` both panic as checked overflow before a backend host
operator executes. Unsigned division and remainder use the corresponding
nonnegative quotient and remainder and panic on a zero right operand.

Reason: overflow and zero-divisor rules alone do not determine negative
quotients or remainders and do not protect portable generated C from its
`MIN / -1` undefined-behavior boundary. Fixing quotient rounding, remainder
identity, sign, and the shared overflow case gives the MIR interpreter, VM,
and every C17 backend one executable contract.

## D043 — Owned public dot-call behavior travels with the nominal type

Status: accepted.

When a public nominal type from another package is usable at a call site, all
public dot-call-eligible functions owned by that same package participate in
the type's dot-call set. This is true for a named type import, a named function
import, and a namespace import. Import spelling changes only the available
free-call spelling: a named function import permits `string(user)`, a namespace
import permits `store.string(user)`, and a type-only named import may provide
neither while still permitting `user.string()` and the same structural
interface conformance. Private functions and behavior owned by any other
package never enter that set.

Reason: otherwise the same nominal value could satisfy an interface in one
file and fail in another solely because the owning function name was imported
differently. Package ownership already provides coherence, so importing the
type's public behavior for dot lookup makes conformance stable while keeping
ordinary free names explicit. Because this behavior is intentionally not
spelled at the call site, checker-owned completion and documentation must show
the owning package and equivalent available free-call spelling. This
supersedes D003 only where “visible” could be read as requiring a separately
named function import; it does not admit foreign extensions or `impl` blocks.

## D044 — Deterministic `f64` binary64 execution

Status: accepted.

`f64` and its `float` alias use IEEE 754 binary64. A decimal `f64` literal is
converted from its source decimal value with round-to-nearest, ties-to-even,
independently of the host locale. The resulting raw binary64 bit pattern is
preserved by constants, copies, call arguments, call results, and returns.

`f64` arithmetic uses IEEE 754 binary64 under round-to-nearest, ties-to-even.
Each arithmetic result canonicalizes any NaN to raw bits
`0x7ff8000000000000`; this includes a NaN produced by an operation or supplied
as an operand. Unary negation is not arithmetic for this rule: it toggles the
sign bit exactly and therefore preserves the payload and quiet/signaling state
of a NaN. A binary64 division by positive or negative zero has its IEEE result
and does not use the integer divide-by-zero panic rule.

Comparisons use IEEE ordered semantics: `+0.0 == -0.0`; every comparison with
a NaN is false except `!=`, which is true; and NaN is unordered for `<`, `<=`,
`>`, and `>=`. The canonical NaN rule applies before a value is observed across
an arithmetic boundary, so backend-specific NaN payload propagation is not
observable.

Tier 1 native targets must provide an IEC 60559/IEEE 754 binary64 `double` and
compile generated floating-point C with strict semantics: no fast-math,
reassociation, contraction, excess precision, or locale-dependent literal
conversion. A host that cannot meet those conditions is unsupported for native
`f64` execution rather than a conforming approximation.

This decision specifies `f64` only. `f32` remains an IEEE-width primitive in
the language table, but its literal conversion, NaN canonicalization, and
backend conformance rules require a separate decision before implementation.
Textual formatting and parsing APIs for floating-point values remain
separate library contracts; D046 is the 0.1-dev preview contract for
`formatF32`/`formatF64` used by user stdout.

Reason: merely requiring "IEEE" and disabling fast-math leaves locale parsing,
rounding, NaN payloads, division by zero, and target precision to each host.
Fixing the binary64 value boundary permits MIR, bytecode, and generated C to
compare exact result bits without making `printf`, host parsers, or a future
`f32` implementation accidental language authorities.

## D045 — Exact managed-array construction, access, and growth

Status: accepted.

`[]T` is a managed mutable container with stable reference identity.
`arrayOf(...)` is a compiler-known variadic prelude function whose arguments
are evaluated once from left to right and must have one exact element type.
An empty `arrayOf<T>()` supplies that type explicitly; an empty array literal
requires an expected `[]T` type. Non-empty literals infer one exact common
element type without implicit conversion.

The ordinary prelude function `length(array): int` is dot-call eligible as
`array.length()`. Array indexing accepts exact `int`/`i64` only and is valid
exactly when `0 <= index < length`. A failed read or write panics with `G4002`,
the locale-independent message `array index <signed-index> is outside length
<length>`, and the complete failing `array[index]` expression as its primary
span. Nested indexing reports the first failing access.

For `array[index] = value`, the receiver and index are evaluated, the bounds
check is performed, and only then is the right-hand side evaluated and stored.
A compound indexed assignment additionally reads the old element before
evaluating its right-hand side. Thus an invalid destination cannot trigger
right-hand-side effects, and no failing operation mutates the array.

The ordinary prelude function `push(array, value)` is dot-call eligible and
returns `Result<void, ArrayGrowthError>`. `ArrayGrowthError.LengthLimit` reports
the documented semantic maximum of `int::MAX` elements. Capacity is not
observable. Allocator exhaustion and a configured VM/isolate heap ceiling keep
their execution-specification fatal/runtime-limit behavior; they are not
silently converted into a recoverable collection error.

The normative iteration spelling for arrays is the existing D033 form
`for const item of array`. It obtains one iterator for the expression, exposes
an immutable `item` of the exact element type, and closes the iterator on normal
completion, `break`, `continue`, panic, or return. Mutation of the iterated
array is rejected by the standard iterator generation check unless an iterator
operation explicitly owns that mutation. Indexed `for let` loops remain a
separate, valid way to traverse an array when mutation is intentional.

When a bytecode bundle contains a generic array, its portable representation
uses a canonical array-type table. Each row contains the element bytecode type
tag and, for an array, record, enum, or interface element, the referenced
layout-table index. The built-in canonical string descriptor uses `NO_LAYOUT`,
as do scalar elements; array
instructions refer to the row rather than serializing a source `TypeId` or a
host pointer. The fixed `[]f64` preview tags remain a decoder-compatible
legacy form only for old f64-only bundles.

Across package calls, results, public aliases, and forwarding wrappers, the
array row denotes the same fully-qualified instantiated element type and exact
recursive managed layout. No package-boundary conversion or local nominal
substitute is permitted.

Reason: array construction, indexing, mutation, panic identity, and resource
failure must agree in the MIR oracle, VM, and native engine. Leaving the index
type, bounds span, assignment order, or growth failure implicit would make a
matrix program observably backend-dependent. This decision does not define
array value equality or hashing, iteration invalidation, slices, or a general
variadic-function declaration feature.

## D046 — Preview user stdout is an explicit `std/io` write

Status: accepted.

The 0.1-dev user stdout surface is exactly:

```marimay
import { println, Error as IoError } from "std/io";

public enum Error {
  CapabilityDenied { capability: string; },
  WriteFailed,
}

public fn message(error: Error): string;
public fn println(text: string): Result<void, Error>;
```

`println` is an ordinary public function in `std/io`, not a prelude name and
not a special call form. Its only accepted argument type is `string`. There is
no implicit conversion from `bool`, any integer width, `f32`, `f64`, records,
enums, arrays, or interpolation results that are not already `string`. D003
forbids same-name overloads over those primitives, so this preview does not
invent a compiler-known multi-type `println`.

A program that needs decimal text for a binary32 or binary64 value uses the
total `std/convert` formatters and then writes the resulting string:

```marimay
import { formatF32, formatF64 } from "std/convert";

public fn formatF32(value: f32): string;
public fn formatF64(value: f64): string;
```

Those formatters are not user stdout. They do not write, flush, grant
capability, or append a newline. Interpolation's "standard string
representation" is not this contract and remains a separate decision.

`println` evaluates its argument once, then attempts to publish the exact UTF-8
bytes of that `string` followed by one LF (`U+000A`). It never emits a BOM, a
CRLF, a trailing extra newline at process exit, or a locale/code-page
transcoding. Embedded NUL bytes and payload newlines are written unchanged;
`println("")` publishes a single LF; `println("a\nb")` publishes `a\nb\n`.
Host console display is outside program semantics; captured and file-redirected
bytes are the UTF-8 program stream.

Each successful call is line-atomic for that payload-plus-LF sequence: either
the complete sequence becomes part of the isolate's program-ordered stdout
stream and is observable to the host or test capture before `Ok()` returns, or
the call returns `Err` and publishes no additional bytes from this attempt.
Implementations serialize `println` through one stdout owner per isolate so two
successful lines cannot interleave inside each other. Order across independent
tasks is not an additional guarantee beyond ordinary happens-before; sequential
calls in one task follow source evaluation order. Deterministic tests use the
execution-spec scheduler.

`println` requires the named capability `"io.stdout"`. `mmay run` of a local
module or script, and a native executable produced by `mmay build`, grant it by
default against the process stdout stream. `mmay test` grants a captured sink
by default; `expect.output` compares those exact bytes. `mmay exec` of an
untrusted `.myb` denies it unless the invocation grants it. Libraries cannot
broaden the root set. Denial returns `Err(Error.CapabilityDenied { capability:
"io.stdout" })` and writes no program stdout.

Write failure after a grant, including a closed or broken pipe, returns
`Err(Error.WriteFailed)` rather than panicking. `message` is locale-independent
and engine-identical:

- `CapabilityDenied` → `capability io.stdout is not granted`
- `WriteFailed` → `stdout write failed`

Required messages do not embed host `errno` text. A residual host prefix after
an already-started failed write is not a VM/native byte-match obligation;
successful programs and capability denials are.

`println` is standard I/O and therefore may suspend. Until I6 implements
parking, a Stage 0 lowering may use a blocking host write if and only if the
observable bytes, `Result` cases, capability checks, and publish-before-`Ok`
rule remain identical; that blocking lowering is removed when I6 lands.

`formatF64` and `formatF32` map raw IEEE bits to a unique ASCII spelling
without consulting the host locale or `printf`:

1. Sign bit set emits a leading `-`, including for `-0.0` and a negative NaN.
2. A NaN is `nan` or `-nan`; payload and signaling bits are not printed.
3. Infinities are `inf` and `-inf`.
4. A finite value uses the shortest decimal that, interpreted with
   round-to-nearest, ties-to-even at that IEEE width, recovers the original
   bits. If several same-length decimals qualify, choose the closest to the
   exact mathematical value; a remaining tie chooses the even last digit.
5. Let `k` be the decimal exponent of that significand written in `[1, 10)`.
   Use plain form when `-4 <= k <= 15`, otherwise scientific form.
6. Plain form is `digits . digits` with no leading zeros other than a single
   `0` before the point, and at least one fractional digit (`415.0`, `0.0125`).
7. Scientific form is `d . digits e [optional -] exponent`, one digit before
   the point, no `+` on the exponent, and no leading zeros in the exponent
   (`1.0e10`, `1.25e-8`).

`formatF32` applies that algorithm to binary32 bits. It does not specify `f32`
arithmetic, literals, or NaN canonicalization; those remain owned by a future
decision as stated in D044.

The internal engine-test traces of the form `ok:f64:` plus hexadecimal bits are
not a public API, not a formatter, and not an accepted user stdout spelling.
Public VM, native, and `mmay test` capture comparison for this surface uses the
D046 byte stream and `Result` classification.

This decision does not add `print`, `eprintln`, a public `flush`, formatted
arguments, or a general `Display`/`Debug` system.

Reason: acceptance programs must print identical VM and native bytes without
turning a hex test oracle, host `printf`, or implicit conversion into language
law. A single `string` write plus explicit float formatters keeps D003, D006,
D016, and D044 intact while giving 0.1-dev a closed, testable stdout contract.

## D047 — Integer policy operations and conversions are explicit and closed

Status: accepted.

The prelude defines a compiler-known generic integer family over exactly
`i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, and `u64`. Binary operations
require two values of the same exact integer type `T`; negate takes one `T`:

```marimay
checkedAdd<T>(left: T, right: T): Option<T>
checkedSubtract<T>(left: T, right: T): Option<T>
checkedMultiply<T>(left: T, right: T): Option<T>
checkedDivide<T>(left: T, right: T): Option<T>
checkedRemainder<T>(left: T, right: T): Option<T>
checkedNegate<T>(value: T): Option<T>
checkedShiftLeft<T>(value: T, count: T): Option<T>
checkedShiftRight<T>(value: T, count: T): Option<T>
```

The `wrapping` and `saturating` families have the same operation suffixes and
arguments, are named `wrappingAdd` through `wrappingShiftRight` and
`saturatingAdd` through `saturatingShiftRight`, and return `T` directly.
These are unqualified prelude symbols subject to ordinary name resolution and
shadowing; they are not dot-call eligible because primitive integers are not
package-owned nominal types.

For the checked family, every arithmetic overflow, zero divisor, or invalid
shift count returns `None`; success returns `Some(value)`. The wrapping family
reduces overflow modulo `2^bitWidth`; in particular signed `MIN / -1` produces
`MIN` and `MIN % -1` produces zero. The saturating family clamps an overflow
to the nearest representable endpoint; signed `MIN / -1` produces `MAX` and
its remainder produces zero. A zero divisor and an invalid shift count still
panic for wrapping and saturating operations because neither policy supplies
a numeric result for those invalid operands. Right shift follows D035 and has
no overflow to wrap or saturate.

Integer conversion uses the contextual infix form `value as Target`. Source
and target must both be one of the eight integer primitives. If every source
value is representable in the target, the expression has type `Target` and is
total. Otherwise it has type `Option<Target>`, producing `Some(converted)` only
when the runtime value is representable and `None` otherwise. Conversion never
wraps, saturates, or panics. There are no implicit integer conversions and no
integer/float conversion is introduced by this decision.

`as` binds more tightly than multiplicative operators and less tightly than
prefix unary operators. Repeated conversions associate left to right.

Reason: the previous specification named policy operations and required
loss-detecting explicit conversion without fixing callable signatures or
conversion syntax. A closed primitive family keeps inference decidable without
inventing a public numeric constraint, makes every information-losing result
visible in the type, and gives MIR, bytecode, VM, and native C17 one portable
contract at signed overflow boundaries.

## D048 — Batch checking owns minimal module identity before package resolution

Status: accepted.

The manifest parser continues to retain repeated syntactically valid
`module` and `language` directives for formatting. A command that constructs a
local module graph, beginning with `mmay check`, additionally requires exactly
one of each directive and requires both decoded values to be nonempty and
NUL-free. Failure is `K0005`. This is the minimum identity boundary needed to
name local packages; versions, dependencies, capabilities, metadata,
workspaces, and all other package-resolution semantics remain owned by I7.

Within one module, the root directory has the exact decoded module import path.
A source-bearing descendant directory is a package whose import path appends
its `/`-separated relative directory path. Whole-module checking visits every
discovered package in canonical import-path order, including packages not
reachable from the root package. Source files within each package are ordered
by exact normalized path before the authoritative compiler service runs.

Default batch checking excludes files ending `_test.my` and the module-root
`tests/` tree. `mmay check --all-targets` includes both. Traversal does not
follow symbolic links, enter `.git` or `.mmay`, or cross a nested `mmay.mod`,
and it is bounded by documented path, depth, entry, file-count, and source-byte
limits. Batch requests never consume editor overlays or persistent semantic
caches: each invocation loads immutable disk snapshots into a fresh compiler
service and checks the complete selected graph.

An accepted source construct that has not reached the authoritative checker
cannot produce a successful `mmay check`. The command reports `I0001` and exits
70 rather than treating an incomplete compiler status as a clean program.

Reason: checkpoint 8 cannot truthfully expose complete, deterministic module
checking without a stable local package identity, while moving dependency and
supply-chain policy ahead of I7 would couple an otherwise local compiler
command to unfinished resolution semantics. This narrow exception resolves
that boundary without changing the lossless manifest tree used by I1.

## D049 — Local `mmay run` has one verified VM boundary and closed outcomes

Status: accepted.

`mmay run` uses the D048 local target and root-package discovery rules. A
standalone `.my` target is the root `script` package. A module target selects
the canonical package represented by that target; no-target and an explicit
module-root target select the module root package. Only a function named
exactly `main` in that root package can be the entry symbol. It must be a
non-external, non-test, non-fuzz function with no parameters and one of these
exact return types:

```marimay
fn main(): void;
fn main(): Result<void, E>;
```

For the second form, the root-visible D003 dot-call set for `E` must contain
one non-external exact `message(error: E): string` function. A private
function is visible for this contract only when it is owned by the root
package; another package must publish it. This is the preview structural
`Error` contract. Entry selection does not accept an unrelated nominal named
`Result`, a non-`void` `Ok` payload, parameters, or a same-named `main` in
another package.

After the authoritative checker succeeds, `run` consumes only its verified
typed MIR. It lowers every checked non-external function to the canonical
bytecode program, encodes that program, decodes the produced bytes into a new
program, verifies the complete decoded program, and only then enters the VM.
VM is the default engine. Failure of this internal integrity chain is `B0001`
and exit `70`; a VM resource, integrity, or unsupported-host failure is
`I0002` and exit `70`. An invalid entry is `T0017` and exit `1`.

A `void` return or `Result.Ok()` exits `0`. `Result.Err(error)` invokes the
selected `message(error)` function, renders `G4003` with those exact counted
string bytes on stderr, and exits `1`. D046 `io.stdout` denial and write
failure remain recoverable `std/io.Error` values; if `?` propagates either to
root `main`, the resulting `G4003` is an expected program failure, not the
tool-level sandbox exit `4`.

Runtime panics write no tool envelope to program stdout and exit `1`.
Explicit source panic remains `G4000`; D045 bounds panic remains `G4002` with
its exact bounds message. Checked integer overflow, division/remainder by
zero, and invalid shift use `G4001` with respectively `integer overflow`,
`integer division by zero`, and `integer shift count is outside width`.
Human rendering is `panic[code]: message`; short and JSON styles retain their
ordinary stable diagnostic forms. All compiler, expected-error, panic, and
internal diagnostics go only to stderr. Successful program bytes go only to
the D046 stdout owner; the internal `ok:f64:` trace is never emitted by
`mmay run`.

The D046 capability `io.stdout` is granted by default for this local command.
The argument boundary is exact: arguments after the first `--` are program
arguments and are not parsed as tool options. The preview has no source API
for reading them yet, so retaining the bounded slice is intentionally
unobservable. Checkpoint 9's Stage 0 command accepts only `--engine vm` and
rejects other values with `U0005`/exit `2`; checkpoint 10 supplies the native
artifact path before the final toolchain contract exposes native run.

Reason: exposing source execution before fixing entry ownership, the
encode/decode verification boundary, and every process outcome would make the
CLI itself a second semantics engine. One closed VM path gives acceptance
programs real stdout and stable failures while preserving D046, D045, and the
later native-build sequencing.

## D050 — Checkpoint-10 native builds publish one content-addressed C17 artifact

Status: accepted.

The Stage 0 checkpoint-10 `mmay build` path reuses D048 target discovery,
D049 entry selection, and the authoritative verified typed MIR. Native is the
only public build engine in this checkpoint. `--release`, `--output`, and
`--emit c` are accepted; `--engine vm` and `--emit mir` remain `U0005` staging
errors, while cross-target packs remain outside this checkpoint.

The native owner emits one portable C17 translation unit plus a compiler-owned
entry adapter, links the version-matched runtime archive, and invokes the
distribution-selected supported host compiler directly with an argv vector.
The default is the CMake-selected pin; D114 adds an explicit portable installed
macOS policy. Neither policy uses a command shell, consults `CC`, or accesses
the network. The compiler
identity and runtime locations are distribution metadata rather than project
configuration. A missing compiler is detected by a bounded preflight before C
generation and is `B0002`/exit `1`, with one platform installation action.

The default artifact lives below
`.mmay/build/native/{debug|release}/{sha256}/app` (`app.exe` on Windows), and
`mmay build` writes that path followed by LF on success unless `--quiet` is
set. The SHA-256 key covers generated C, compiler/runtime/language versions,
host compiler identity, target host identity, profile, and the versioned
backend flag contract; absolute workspace paths do not enter it. A regular
artifact at that exact path is a cache hit. `--output` additionally compiles
and atomically replaces the exact requested sibling output. `--emit c`
atomically publishes the public-entry translation unit instead of invoking the
host compiler. Failed checking or code generation never executes or publishes
a stale temporary artifact.

Debug uses strict C17 at `-O0` (or `/Od`), release uses `-O2` (or `/O2`), and
both retain checked Marimay overflow, division, shift, bounds, panic, and root
cleanup semantics. The public native entry emits no internal `ok:*`/`trap:*`
harness envelope. `void` and `Ok()` exit `0`; `Err`, `G4000`, `G4001`, and
`G4002` retain D049 messages and exit `1`; runtime integrity/resource failures
are `I0002` and exit `70`. Every initialized managed heap is disposed on all
normal, expected-error, panic, and internal exits.

Reason: generated-C execution was already a semantic oracle, but a user-facing
build needs a single owner for toolchain discovery, flags, runtime linkage,
cache identity, atomic publication, and process outcomes. Fixing those details
together prevents the CLI, backend, and host compiler wrapper from becoming
three competing native semantics paths.

## D051 — Checkpoint-11 tests compare one captured execution record

Status: accepted.

The Stage 0 checkpoint-11 `mmay test` path discovers only ordinary
`@test fn(): void` declarations in `*_test.my`. Package tests retain their
package-private visibility; module-root `tests/` packages are checked against
public imports. Discovery and reporting use the stable checked symbol and are
ordered by package, canonical source path, declaration span, and name.

Each test owns a fresh execution state. VM is the default engine; native uses a
fresh process. Both grant a captured D046 sink, keep stdout and stderr in
separate bounded buffers, and publish those bytes only as part of that test's
failure report. `expect.output` is compiler-known in test context and brackets
one zero-parameter `void` block closure. It compares the closure's exact D046
bytes, including embedded zero and LF bytes, and reports a deterministic
bounded `G4004` mismatch after lexical D041 cleanup.

`--engine both` is a record/replay comparison, not two independent assertions.
The VM supplies termination, exact stdout, and exact panic/stderr bytes; native
replays the same verified typed-MIR test symbol with the same fixed capture
grant. Exit, signal, stdout, or panic/stderr disagreement is `B0007`/exit `5`.
A matching panic or expectation failure is an ordinary failed test/exit `1`.
Filters, watch mode, and test-result caching remain I8 work and their named
checkpoint-11 options are rejected with `U0005` rather than ignored.

Reason: a built-in loop is only trustworthy if discovery boundaries, isolation,
captured output, and backend equivalence share one compiler-owned observation
contract. Staging the deterministic core now avoids creating separate VM and
native notions of a passing test while leaving scheduler/cache policy to their
ordered owners.

## D052 — Native output profile selects a validated runtime artifact

Status: accepted.

`mmay build` treats the requested output profile as authoritative for every
first-party native object. Each Stage 0 toolchain build and official host
installation contains separate debug and release runtime archives. The debug
archive is compiled with the debug optimization contract (`-O0` or `/Od`); the
release archive is compiled with the release optimization contract (`-O2` or
`/O2`). Which configuration was used to compile the invoking `mmay` executable
does not affect this selection.

Before generated-C emission, the native artifact owner selects the requested
archive and validates a versioned manifest plus embedded runtime-version and
profile markers. A missing, stale, partial, or wrong-profile artifact is
`B0008`/exit `1` and never falls through to an opaque host linker failure. An
installed tool first resolves `../lib/mmay/{runtime-version}/native/{profile}`
and `../include` relative to the invoked `mmay` path. A partially present
installed candidate is an error; only the absence of the complete installed
candidate permits the development build-tree metadata fallback. Relocating a
complete installed prefix therefore preserves native builds without retaining
a dependency on the original build tree. Every native-build consumer supplies
the invoked tool identity. An absent identity is a missing runtime error, not
permission to select build-tree metadata; this keeps an omitted caller field
from passing only on a machine that happens to contain the development tree.

The content-addressed build key hashes the selected runtime archive bytes,
profile, version, host/compiler identity, generated C, and backend flag
contract. It does not hash the archive's absolute path. Changing runtime bytes
invalidates the cache; moving the same complete installation does not. `--emit
c` links no runtime and remains available without validating an unused archive.
Neither artifact lookup nor validation consults project configuration, `CC`, or
the network.

Reason: release flags applied only to generated C cannot make a release
executable when a debug runtime archive was inherited from the compiler's own
build directory. Profile-specific shipped artifacts make the output request
deterministic, while manifests, embedded markers, relative installed lookup,
and content hashing prevent silent mismatch and stale cache reuse.

## D053 — Array removal, truncation, reservation, and iterator generations

Status: accepted.

The compiler-known prelude collection surface additionally contains these
ordinary generic functions, with free and dot forms resolving to the same
operation after exact receiver-directed D003/D043 candidate selection:

```marimay
public fn pop<T>(values: []T): Option<T>;
public fn truncate<T>(
  values: []T,
  newLength: int,
): Result<void, ArrayTruncateError>;
public fn clear<T>(values: []T): void;
public fn reserve<T>(
  values: []T,
  additional: int,
): Result<void, ArrayGrowthError>;

public enum ArrayGrowthError {
  LengthLimit { limit: int; },
  NegativeAdditional { additional: int; },
}

public enum ArrayTruncateError {
  InvalidLength { requested: int; length: int; },
}
```

`pop` on an empty array returns `None` and changes nothing. Otherwise it
removes the last initialized element and returns that exact value in `Some`.
The removed slot is no longer part of the array's initialized or traced
storage; a managed reference returned by `Some` remains rooted by the result.

`truncate` succeeds only when `0 <= newLength <= length(values)`. An equal
length is a successful no-op. A negative or expanding request returns
`Err(ArrayTruncateError.InvalidLength { requested, length })` without changing
the array. A successful shrink removes the complete suffix beginning at
`newLength`; `clear` removes the same suffix beginning at zero and is a no-op
for an already empty array. Neither operation implicitly shrinks capacity.

`reserve(values, additional)` requires a nonnegative additional element count.
A negative count returns `Err(ArrayGrowthError.NegativeAdditional {
additional })`. When `length(values) + additional` is greater than
`int::MAX`, it returns `Err(ArrayGrowthError.LengthLimit { limit: int::MAX })`.
Otherwise success guarantees storage for that many further `push` operations
without another array-storage growth, unless an intervening operation changes
the initialized sequence. The implementation may reserve more storage, and
neither capacity nor whether a request reused existing storage is observable.
Configured heap ceilings and process allocation failure retain their D045
runtime-limit/fatal classification rather than becoming an enum case.

Receiver and explicit arguments are evaluated exactly once from left to right.
Every resource, checked-size, validation, and allocation failure is atomic:
the initialized elements, logical length, reserved storage, and mutation
generation remain as they were before the operation. A collection attempt may
occur before a runtime-limit result; collection timing is unobservable.

Each array has an internal unsigned mutation generation. Construction starts
at zero. A successful `push`, nonempty `pop`, shrinking `truncate`, nonempty
`clear`, or indexed assignment increments it exactly once after the operation
commits. Empty/no-op/error paths and `reserve` do not increment it. Generation
never wraps: exhaustion is a runtime resource-limit outcome and the mutation
does not occur.

An array iterator captures the generation once. Before every iteration test or
element yield it compares that snapshot with the array. A mismatch panics with
`G4005`, message `array iterator invalidated by mutation`, and the complete
`for const ... of ...` header as primary span. Breaking, returning, or
panicking closes the iterator without another generation check. `reserve` is
permitted during iteration because it changes no initialized element and does
not change the generation; implementations must therefore not retain a raw
cell-storage pointer in an iterator.

Reason: real stack, queue, and heap implementations need removal and bounded
capacity planning without shadow logical sizes or stale traced elements.
`Option` represents empty removal, typed errors keep invalid sizing visible,
and a generation snapshot detects structural mutation without exposing
capacity or requiring iterator lifetime syntax. These operations extend D045;
they do not add slices, a deque, a capacity getter, implicit shrinking, or a
general collection-method declaration mechanism.

## D054 — Managed arrays use prefix `[]T` source spelling

Status: accepted.

The canonical source spelling of the existing D045 managed array type is
`[]T`. Prefixes associate to the right, so `[][]int` is an array whose elements
are `[]int`. The element may be a qualified, generic, tuple, or function type.
An empty expression literal remains `[]`; type position is determined by the
grammar context. This decision changes source spelling only: the canonical
type identity, managed-reference identity, typed MIR, bytecode descriptors,
runtime layout, tracing, VM behavior, native behavior, and ABI remain those of
D045 and D053.

This is a pre-1.0 replacement, not a compatibility alias. The prelude no
longer exposes a nominal `Array` type name. An otherwise unresolved
`Array<T>` type reports `T0018` over the complete type with the exact message
`prelude managed arrays use '[]T'; replace 'Array<T>' with '[]T'`. A local or
imported user declaration named `Array` remains an ordinary nominal type and
is never rewritten by the formatter. The distinct `Slice<T>` nominal type
retains its non-owning, scope-bounded semantics and is not denoted by `[]T`.

The formatter emits adjacent prefixes (`[]T`, `[][]T`) while preserving
comments. Diagnostics, hover/signature text, completion detail, generated
documentation, and other compiler-owned type displays use the same canonical
spelling. Existing D045/D053 references to a semantic managed-array identity
are read under this replacement; their operation and failure contracts are
unchanged.

Reason: managed arrays are pervasive in signatures and algorithms, and a
compact recursive spelling makes nested element structure visible without
introducing a second collection. Removing the old prelude spelling before 1.0
avoids a permanent alias and leaves package-owned names under ordinary lexical
resolution.

## D055 — Configurable managed-heap byte budgets and classified `I0002`

Status: accepted.

`mmay run`, `mmay test`, and `mmay build` accept one
`--max-heap-bytes <positive-decimal>` option; the equivalent
`--max-heap-bytes=<positive-decimal>` spelling is also accepted. The value is
an integer number of bytes with no sign, whitespace, separator, or unit suffix.
The minimum is one. The maximum is the lesser of `int::MAX` and the host's
`SIZE_MAX`; every Tier 1 host accepts `1..9223372036854775807`. Zero, a sign,
an empty or non-decimal value, overflow, and a repeated occurrence are `U0005`
and exit `2`. A separated missing value is `U0004`. A repeated occurrence is
diagnosed at the second option; an invalid separated value is diagnosed at its
value argument. The default remains exactly `67108864` bytes. No environment
variable or undeclared project state overrides either value.

The budget counts live bytes owned by the managed heap: published managed
object payloads, reserved array cell storage, and heap slot-table storage.
Compiler/VM metadata, bytecode, native code, call stacks, test-capture buffers,
and OS resources are not charged to this particular budget and retain their
own bounded owners. Runtime string values, including immutable interpolation
and concatenation results, are managed object payloads. Unreachable intermediate
strings are collectible; an execution-lifetime interning table must not impose
a second cumulative string-byte budget on them. Serialized string constants
retain their separate artifact bounds. This clarifies the existing managed
string contract; it changes neither string contents nor value semantics.
An allocation whose checked `live + requested` equals the
limit is permitted. If it would exceed the limit, the runtime first performs
the collection attempt required by D045/D053; the post-collection `live` value
and the net additional accounted `requested` value determine the outcome.
Budget rejection is failure-atomic and does not attempt the rejected host
allocation.

The shared string constructor enforces this order before obtaining either
header or payload storage. Contiguous header/payload storage is permitted with
the same logical byte charges. An allocation-free trace may forecast exact
post-sweep bytes while retaining unreachable storage until borrowed inputs
have been copied; physical sweep must finish before return, including budget,
host-allocation and publication failures. No source execution or unrelated
heap mutation may occur in that private interval. This implements the existing
budget precedence and byte/identity semantics; it does not increase the heap
limit or require a live root for bytes already valid at constructor entry.

The precedence is source semantics, configured budget, then host allocation:
negative `reserve` and growth past `int::MAX` retain their typed
`ArrayGrowthError` cases; a valid operation beyond the configured managed-heap
budget is a budget-class `I0002`; and an allocator failure after successful
budget preflight is a host-allocation-class `I0002`. Neither execution failure
is catchable source control flow. Both write no program stdout and exit `70`
after bounded cleanup. Human/short diagnostic messages are exactly:

```text
managed heap budget exceeded: live=L bytes, requested=R bytes, limit=N bytes
runtime host allocation failed
```

Budget JSON context is `kind: runtime-resource`, `resource: managed-heap`,
`classification: budget-exceeded`, with numeric `liveBytes`, `requestedBytes`,
and `limitBytes`. Host-allocation JSON context uses `resource: host-memory` and
`classification: allocation-failed`; neither form exposes an address.

`run` applies the selected budget to its fresh VM execution. Every discovered
test receives a fresh VM execution or native process with the same selected
budget, so one test never inherits another test's live usage or failure state.
`build` embeds the selected value, including the default when the option is
absent, into generated C and the native executable; the generated source and
therefore the content-addressed cache identity differ when the value differs.
Future child isolates copy their parent's configured value unless an explicit
isolate API supplies another value; they inherit neither live usage nor mutable
failure state, and this decision does not add scheduler/isolate syntax.

Reason: a single broad resource failure hides whether a program hit its
documented sandbox, a recoverable source-level collection bound, or actual
host memory pressure. A byte-exact, engine-independent policy makes local VM
execution, isolated tests, and published native artifacts reproducible without
changing the existing 64 MiB default or redesigning collection.

## D056 — Performance baselines are raw, platform-specific regression contracts

Status: accepted.

An optimization checkpoint establishes its measurement contract before it
changes the implementation being measured. A Tier 1 array/native baseline uses
a dedicated runner, a monotonic nanosecond clock, at least three warmup rounds,
at least eleven odd-count measured rounds, a fixed per-process timeout, and a
deterministic balanced rotation of workload and participant order. Compilation
is completed before timing. Every valid sample is retained; result selection,
silent outlier deletion, and best-of-N reporting are forbidden.

Each run records the exact source/corpus revisions, workload-manifest and
protocol hashes, platform/CPU/load/power metadata, command, managed-heap
budget, Marimay compiler path/hash/version, selected release-runtime
path/hash/manifest/profile, and generated-C compiler/linker paths, versions,
and flags. A timed process must have the exact expected stdout/checksum, empty
stderr, successful exit, and no timeout. Debug or unclassified runtime
artifacts, incomplete metadata, non-finite timing values, insufficient samples,
and incompatible schemas/baselines are rejected rather than downgraded to
exploratory evidence.

Reports publish every raw sample and execution-order event plus median, median
absolute deviation, Tukey-hinge interquartile range, min, max, and the workload
noise fraction. The noise fraction is the maximum of 2%, three MAD divided by
the median, and IQR divided by the median. A baseline with more than 30% noise
is not eligible for a budget. The reviewed per-platform, per-workload Marimay
regression limit is the baseline median multiplied by one plus the greater of
10% and that noise fraction. Cross-language ratios remain exploratory and do
not become Marimay release budgets.

A median above budget triggers exactly one complete run after cool-down using
the same schema and artifacts. A regression is confirmed only when the
original and retry medians both exceed the same reviewed workload budget; both
raw reports remain evidence. The retry cannot erase a flaky failure or replace
the original distribution. Budget changes require a new reviewed baseline or
an accepted decision naming the user benefit; an optimization may not widen a
threshold merely to make its own result pass.

Reason: small sample counts, profile-incoherent runtime archives, partial
metadata, and cross-language ratios cannot distinguish compiler regressions
from host noise or workload drift. Raw compatible distributions and narrow
platform-local budgets create an auditable gate without promising universal
language speed.

## D057 — Checkpoint 21 uses a Linux reference baseline before public-alpha Tier 1 closure

Status: accepted.

Checkpoint 21 is complete when one dedicated Linux x86_64 runner has produced
and passed the D056 protocol for both the nine focused array/native probes and
the faithful eight-workload algorithm corpus with the full Marimay, Go, and Rust
participant matrix. The result must use release Marimay artifacts, retain at
least three warmups and eleven measured samples, pass exact schema, checksum,
artifact, manifest, metadata, and noise validation, and have reviewed
platform/workload Marimay budgets. Other platform reports may be retained as
supplemental evidence but do not widen this checkpoint's completion gate.

Preparation for the public alpha owns the separate full Tier 1 performance
gate. It requires compatible reviewed baselines for Linux x86_64, Linux arm64,
macOS arm64, macOS x86_64, and Windows x86_64 and must revalidate or replace
earlier reports after intervening optimization work. A machine-readable
registry exposes distinct `checkpoint-21` and `public-alpha` gates; passing the
former must not make the latter pass or permit a complete Tier 1 claim.

This decision resolves the sequencing conflict with the original
post-preview checkpoint-21 completion sentence, which required all five Tier 1
runners before checkpoint 22. It supersedes only that sequencing requirement.
D056's measurement, review, retry, and regression rules and the Tier 1 target
set remain unchanged.

Reason: the Linux algorithm baseline is sufficient to choose and evaluate the
ordered pre-alpha optimization work, while obtaining every release-platform
reference runner is deployment preparation. Keeping a separate failing
public-alpha gate preserves the cross-platform obligation without blocking the
measurement-driven implementation sequence on unavailable runner hardware.

## D058 — Bounds proofs are verified MIR capabilities

Status: accepted.

`ARRAY_REQUIRE_INDEX token, array, index` remains the observable bounds
barrier: it performs the D045/G4002 check and copies the exact array handle to
`token`. `ARRAY_GET_IN_BOUNDS value, token, index` and
`ARRAY_SET_IN_BOUNDS token, index, value` are internal MIR/bytecode operations
that consume that capability and do not repeat the signed bounds comparison.
They are never source operations and are accepted only when the MIR and the
hostile-bytecode verifier independently reconstruct a preceding matching
capability.

A proof is a must-fact over its exact carrier register and index register. The
MIR-only `ARRAY_ASSUME_INDEX` may replace a barrier only for a literal array
length or on the true edge of an exact signed `index < array.length()` test.
For the latter, a forward must-analysis reaches a CFG fixed point for the
separate `index >= 0` fact: nonnegative constants, copies, array lengths, and
checked additions of nonnegative values transfer it; every other definition
loses it. The verifier repeats both facts and rejects changed registers,
calls, stores, or mutations before the consumer. Every incoming edge at a
join must provide the same proof. An affine index is eligible only when the
checked arithmetic result itself is the exact compared index; uncertain,
negative, or overflow paths retain `ARRAY_REQUIRE_INDEX`.

`ARRAY_ASSUME_INDEX` has no bytecode wire encoding. Lowering serializes it as
the ordinary checked `ARRAY_REQUIRE_INDEX`, so a hostile bundle cannot claim a
static fact the bytecode verifier cannot reconstruct. Native and MIR execution
consume the verified typed-MIR assumption directly. Rewriting is transactional:
analysis and rewrite storage are allocated before MIR changes, a post-rewrite
verifier failure restores the checked operations, and an optimizer resource
failure leaves the original checked MIR unchanged. Verifier resource failure
rejects the artifact rather than accepting an in-bounds operation.

The native/VM helpers may validate managed references and element values but
must not independently grant a proof or use a cached raw array cell. They rely
on the verified capability and retain no pointer across a call, allocation, or
mutation. This is an internal implementation decision only; D040/D045 source
evaluation order, G4002 spans, and the language rule that unproven indexing is
checked remain unchanged.

Reason: a proof carrier makes the authority boundary explicit in both in-memory
MIR and serialized bytecode. Invalidating conservatively prevents an aliased
array shrink or call from turning a previously checked index into an unchecked
memory access, while transactional fallback keeps resource pressure from
changing source semantics.

## D059 — Native exact array access uses a versioned runtime ABI, not LTO

Status: accepted.

Generated C and the separately shipped native runtime use one narrow exact
array-access ABI. `length`, `require`, `get`, and `set` receive the expected
immutable array descriptor. Each call resolves the managed handle once,
compares the exact descriptor at that resolve, observes the current length,
and either performs the signed bounds check or consumes a D058 verified
in-bounds capability. Get/set transport one `MarimayManagedCell`; type-directed
generated C converts that cell exactly once at the call boundary. Set validates
reference-valued elements and advances the D053 mutation generation only after
the access is known to succeed.

The native emitter may fuse an `ARRAY_REQUIRE_INDEX` only with its immediately
following matching `ARRAY_GET_IN_BOUNDS` or `ARRAY_SET_IN_BOUNDS`. The fused
access uses checked mode and retains the barrier's G4002 source span. A
non-adjacent or otherwise observable barrier remains an explicit `require`
call; subsequent verified consumers use in-bounds mode. An
`ARRAY_ASSUME_INDEX` emits no runtime bounds check. On this call-ABI path the
array reference remains in the native shadow-root frame for the call, and the
ABI retains no resolved slot, array, cell pointer, capacity, or length after
returning. D063 later adds a separately constrained activation-scoped view
path; it does not weaken D058 proof invalidation.

Whole-program or archive LTO is not the selected boundary. The released
runtime is a relocatable, version-matched C17 archive that must work with the
distribution-selected compiler on every Tier 1 platform. LTO objects and
linker-plugin contracts are compiler-family and version coupled, are not
uniform across Clang/GCC/MSVC archives, and would make the access path depend
on the host/profile combination. The measured D059 prototype comparison is
recorded in checkpoint-23 evidence; the exact ABI remains the stable runtime
fallback and VM boundary, and the former generated helper plus
describe/length/get-set chain is removed. D063 may bypass a repeated native
call only through the same version-matched C17 headers and runtime archive.

Reason: one stable C17 ABI call removes repeated handle, descriptor, length,
bounds, status, and cell-boundary work without exposing representation or
letting a linker-dependent optimization mode choose source-observable safety.

## D060 — Raw scalar arrays use descriptor-selected dense native storage

Status: accepted.

The immutable runtime array descriptor selects one of five internal storage
layouts: `RAW8`, `RAW16`, `RAW32`, `RAW64`, or `CELL`. `bool`, `i8`, and `u8`
use `RAW8`; `i16` and `u16` use `RAW16`; `i32`, `u32`, and the raw binary32
formatter input use `RAW32`; `i64`, `u64`, and `f64` use `RAW64`. Managed
reference elements use `CELL`. Each descriptor carries the exact byte stride,
which must be respectively 1, 2, 4, 8, or `sizeof(MarimayManagedCell)` and must
agree with its existing raw/reference cell class. An invalid tag, stride, or
raw/reference combination is rejected before storage is published or accessed.

Raw payload storage is byte-addressed. Loads and stores copy through an aligned
fixed-width local integer with `memcpy`; generated C and the VM continue to
transport the canonical low payload bits in one `MarimayManagedCell` at the D059
boundary. This gives raw storage an alignment requirement of one without
creating an unaligned typed pointer or violating C17 effective-type/aliasing
rules. `CELL` storage retains the allocator alignment required by
`MarimayManagedCell`. Newly reserved bytes are zero-initialized, growth copies the
complete initialized prefix, and truncation clears the removed byte range.
`f64` preserves every binary64 pattern, including signed zero and NaN payloads;
the narrower layouts preserve exactly their low 8, 16, or 32 payload bits.

Only the initialized prefix of `CELL` arrays is precisely traced. Every raw
layout is skipped without interpreting payload bits as references. Managed
live-byte accounting charges `capacity * descriptor.stride`, using checked size
arithmetic before budget preflight, allocation, resize, or address arithmetic.
Growth remains failure-atomic and preserves the array reference, descriptor,
length, capacity, mutation generation, initialized bits, and accounting on
every rejected budget or host-allocation path.

This is a version-matched internal runtime-layout change, not D013 source-level
specialization. Array source identity, stable non-moving references, D045/D053
semantics, D058 proofs, D059 calls, and bytecode array-table rows are unchanged.
Portable bundles serialize the same element type and layout index as before;
the VM/native runtime descriptor is derived only after verification. There is
no packed-bool layout, public stride/capacity observation, moving storage, or
raw-scalar union-cell compatibility fallback.

Reason: a reference-width transport union is an appropriate ABI value but an
unnecessarily sparse backing representation for homogeneous raw scalars. A
closed descriptor/stride contract improves cache density and the number of
elements admitted by the same managed-heap budget while keeping all observable
semantics and portable artifacts unchanged.

## D061 — Managed-array capacity planning preserves the established resource boundary

Status: accepted.

Construction with a statically or dynamically known element count allocates
exactly that many descriptor-stride storage bytes and publishes capacity equal
to logical length, including zero. It does not pass through the unknown-size
growth planner. This applies equally to literals, `arrayOf`, decoded VM
construction, and generated native calls because all converge on the same
managed-array constructor.

Unknown-size growth uses one focused capacity planner. A zero-capacity array
starts at four slots; otherwise the current capacity is doubled until it can
hold the requested minimum. If doubling overflows or crosses the configured
maximum array length, the planner tries that maximum once. `push` asks for the
next logical length. `reserve(values, additional)` computes the D053 minimum
`length + additional` first and invokes the same planner once, so a successful
reservation performs at most one storage resize and guarantees the subsequent
pushes promised by D053. A request already covered by current capacity is an
allocation-free no-op. There is no hidden shrink, capacity getter, or separate
VM/native rounding rule.

The planner uses checked capacity and D060 stride multiplication before budget
preflight or allocator entry. Collection may invalidate resolved addresses, so
growth roots the array and temporary reference element, re-resolves the handle,
and recomputes the deterministic plan before commit. The resize is
failure-atomic and zeroes only the newly reserved byte suffix. Live accounting
charges the same net added bytes as D055.

This decision makes the already established capacity policy explicit; it does
not change its rounding, managed-heap success threshold, `requestedBytes`,
host-allocation request, failure precedence, mutation generation, evaluation
order, or bytecode identity. In particular, exact construction and geometric
unknown-size growth are different internal policies by design. A future policy
change that affects the D055 resource boundary requires a separate decision.

Reason: known-size construction should not pay unused geometric slack, while
unknown producers need amortized growth and `reserve` needs one deterministic
plan. A single owner prevents VM/native drift and preserves the existing
observable resource and failure contracts while making allocation-count and
capacity-boundary testing precise.

## D062 — Checked integer elimination requires reconstructed interval proof

Status: accepted.

The MIR optimizer may replace checked `i64` addition, subtraction, or
multiplication with the corresponding proven operation only when a bounded
interval analysis proves that every mathematical result fits in
`int::MIN..int::MAX`. The lattice tracks signed ranges and the representable
nonnegative ranges of `u8`, `u16`, and `u32`; constants, copies,
range-preserving conversions, array lengths, checked arithmetic, and remainder
by a positive singleton have explicit transfer rules. True/false integer
comparison edges refine intervals. Joins take the convex hull, and a loop bound
that expands after two updates widens to its declared type range before branch
refinement. After the widening fixed point, one monotone narrowing phase
recomputes branch-constrained loop inputs without widening so finite guards can
recover safe body ranges. At a loop header, narrowing replaces an endpoint
still at its declared type extremum with the recomputed endpoint; an already
finite endpoint is retained when recomputation would tighten it further.
Ordinary successor blocks propagate the resulting constraints. This finite-
height narrowing operator avoids counting down a 64-bit interval one value per
iteration. Recomputed growth must never be discarded, and the existing work
budget still bounds the complete analysis. Unsupported `u64` values, unknown
writes, ambiguous
conversions, or an exhausted analysis budget conservatively retain checked
operations.

`ADD_PROVEN_I64`, `SUBTRACT_PROVEN_I64`, and `MULTIPLY_PROVEN_I64` are distinct
typed MIR instructions. Portable bytecode appends the matching wire opcodes
`i64_add_proven`, `i64_subtract_proven`, and `i64_multiply_proven`; they carry
the same destination/left/right registers and no forgeable numeric bounds.
Both the MIR verifier and hostile-bytecode verifier independently reconstruct
the interval analysis at every proven operation. A missing, widened, corrupt,
wrong-type, or over-budget proof rejects the optimized artifact. An optimizer
allocation/work failure leaves those operations checked; it is not a reason to
accept an unproved instruction.

Execution of a verified proven operation converts both operands to `uint64_t`,
performs the corresponding unsigned add/subtract/multiply, and converts the
two's-complement result with the portable D035 helper. The proof guarantees the
mathematical result is representable, while unsigned evaluation ensures the C17
implementation itself never evaluates overflowing signed arithmetic. MIR,
VM, and generated native C use the same helpers. The proven opcodes are not
panic sites; all operations not rewritten retain the original checked helper,
overflow span, first-failure order, and release/debug behavior.

This decision does not cover division, remainder elimination, shifts,
wrapping/saturating APIs, `u64` proof carriers, width changes, or fast math.
Conversions contribute safe input ranges but are not themselves replaced in
this checkpoint. There is no source syntax or source semantic change.

Reason: source-level checked overflow cannot be weakened by a backend hunch or
benchmark invariant. Reconstructed proofs preserve hostile-artifact safety and
VM/native equivalence, while focused interval and verifier owners keep the
arithmetic rule out of execution coordinators.

## D063 — Native managed views are activation-scoped and safepoint-aware

Status: accepted.

Generated native C may bind an exact managed array or record reference once
and retain a non-escaping view for the current function activation. Binding
resolves the handle and validates the immutable descriptor before publishing
the view. Managed objects are non-moving. An array view retains addresses of
the object's `length`, mutation-generation, and storage-pointer fields, not a
copy of the current element-storage pointer, so successful growth updates what
later accesses observe. A record view retains its fixed field-array address.
No view is serialized, returned, stored in managed memory, or exposed to Marimay.

Every managed register has a companion-view invariant. Copying the exact
reference copies its proven companion; any instruction that can define a new
reference without that identity proof clears the destination companion.
Parameter records may bind exact array-valued fields eagerly after the record
descriptor check. An activation containing a call or array-valued record write
checks the current field identity before reuse; call-free/read-only activations
cannot replace that field and omit the guard. A view is usable only while the referenced object is
precisely reachable: normally its register is in the activation's shadow-root
frame, while an array obtained through a rooted record field is retained by
that record's trace edge. Calls and mutations still invalidate D058 bounds
capabilities; a cached view grants identity and layout only, never bounds.

For verified `RAW64` arrays, generated C may load or store with `memcpy`
through the bound storage field. A checked access may omit its comparison only
when the native relation analysis proves on every reaching path that the index
is nonnegative and strictly below a length of the same array, and separately
proves that the array size cannot change in the function. Recomputed
checked/proven add, subtract, or multiply expressions may reuse an equality
fact only while every operand value remains current; CFG joins intersect those
facts, and a branch-selected index requires every assignment source to be
proved at its own assignment point. Operand redefinition, calls, size mutation,
unsupported instructions, ambiguous predecessors, or analysis resource
failure conservatively retain the comparison. Every retained check performs
the same signed G4002 comparison at the same span; MIR-proven accesses still
require the same D058 capability. Stores check the non-wrapping D053 generation
before writing and increment it only after the write is known to commit. Other
layouts and reference-valued elements use the validated view ABI, including
reference type checks and rooted growth. Iterator generation reads use the
same bound field and preserve G4005 behavior.

A native function may omit shadow-root linkage only when a conservative
transitive analysis proves that neither it nor any direct callee can allocate,
collect, suspend, call an interface, publish output, or otherwise reach a
managed safepoint. Unknown callees, recursion, strings, and unsupported MIR
forms require roots. Normal generated builds keep up to 64 root-location
pointers in fixed activation storage; larger frames allocate them. Builds with
deterministic allocation injection retain the former allocation and root-link
path exactly so every established failure index remains testable. Thus root
elision never permits collection while an unlinked view is live.

Generated C also uses portable static-inline `i64` helpers at the emission
boundary. Checked helpers test overflow before signed evaluation, and proven
helpers evaluate in the unsigned domain before the D035 two's-complement
conversion. This changes neither D062 proof authority nor overflow,
division-by-zero, span, or first-failure behavior. It does not require LTO,
compiler intrinsics, signed-overflow assumptions, or fast math.

Reason: the checkpoint-26 nested and checkpoint-28 algorithm profiles showed
that repeated stable-handle resolution, record projection, root-location heap
allocation, and separately linked arithmetic calls dominated the already
dense storage path. Activation-scoped views remove that repeated work without
creating a moving/raw-pointer contract or allowing a backend to invent safety
facts.

## D064 — Pre-beta releases carry an explicit bounded claim

Status: accepted.

The first version line is `0.1.0-alpha.N`, where `N` is a positive decimal
release sequence without leading zeroes. Compiler, runtime, and the shipped
standard-library slice use the same version. A version identifies a candidate
contract; it does not assert that an artifact was published. Machine metadata
therefore also carries one release channel and one claim classification.
The manifest selector remains `language "0.1"`, while version metadata reports
the normative baseline classification `0.1-design` until the language gate
accepts a different classification.

The closed pre-1.0 channels are `private-preview`, `experimental-alpha`, `cli-alpha`,
`concurrency-pilot`, `ecosystem-alpha`, `beta`, and `rc`. The current Stage 0
binary uses D114's `experimental-alpha` channel and may claim only the named
checker-backed local slice. The historical CP29 channel was `private-preview`.
`cli-alpha` is unavailable until CLI0, N0, the claimed-host matrices,
and explicit publication authorization close. No channel silently implies a
controlled name, stable package namespace, portable public bytecode, network
runtime, FFI, cross compilation, desktop support, or self-hosting.

Before beta, source compatibility is release-scoped rather than 1.x-stable.
An alpha may remove or change an accepted source/API surface only through an
accepted decision, simultaneous normative text and conformance coverage, an
explicit release-note migration, and a new `alpha.N` sequence. Building never
rewrites source. Once `mmay fix --language` is implemented, a mechanical
migration required by a release must be available there; before that owner
lands, a change requiring such a migration blocks release. D054's `Array<T>`
to `[]T` replacement predates this contract, has no compatibility window or
alias, and is excluded from every alpha source-compatibility claim.

Stable diagnostic codes, severities, exit classifications, and the documented
fields of a versioned JSON schema do not change within an `alpha.N` candidate.
Human wording, notes, and fixes may improve without changing classification.
A machine-schema breaking change increments the schema identifier and the
alpha sequence and includes a migration note. Beta may tighten this policy;
1.x compatibility remains governed by the language version.

Portable public `.myb` compatibility is `none` while the version reports
bytecode `unavailable`. Internal encode/decode round trips remain
version-matched implementation evidence and are not a public bundle contract.
When a public bytecode owner lands, it must introduce a separate format
version and compatibility decision rather than inheriting the compiler
version implicitly.

The standard library is never labelled wholly unavailable once an executable
slice ships. It reports the common `0.1.0-alpha.N` version plus a
`bounded-preview` scope until the owning closure matrix is complete. Its
source/API compatibility follows the same pre-beta rule as the language; its
runtime ABI remains version-matched with the compiler and runtime.

Every user-facing command is classified as `available` or `staged`. Everyday
help lists only available commands as workflow actions and visibly separates
staged names with their owning checkpoint or gate. A staged invocation fails
with `U0003`; it is never accepted and ignored. `mmay explain` remains staged
under I8 and is absent from everyday inspection help until its offline
reference owner lands. Removing a staged name needs no compatibility window;
removing an available command or standard API follows the alpha removal rule
above.

Known limitations use version, channel, implemented surface, exclusions,
publication blockers, and removal gates as explicit fields. Release metadata,
human/JSON `mmay version`, help, installed documentation, README claims, and
the roadmap must agree. A mismatch blocks the candidate. Prepared, versioned,
or publication-ready artifacts are not tagged, signed, uploaded, reserved, or
announced without a separate user instruction.

Reason: pre-release users need enough stability to distinguish a real bounded
tool from target-design prose, while the implementation still needs honest
room to correct alpha APIs. Separating version, channel, and claim prevents a
locally versioned binary from pretending that naming, platform, compatibility,
or publication gates have closed.

## D065 — Primitive text conversion is closed and locale-independent

Status: accepted.

An interpolated string has type `string`. Its expressions are evaluated once
each, from left to right, and each completed fragment is appended before the
next expression is evaluated. The closed 0.1 set is `string`, `bool`, `i8`,
`i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, and `f64`. Strings are
unchanged; booleans are `true` or `false`; integers use canonical base-ten
text; and `f64` uses D046. Unsupported types are type errors. There is no
implicit conversion into `println`, open `Display` contract, reflection,
overload set, or user-defined formatter in this release.

`std/convert` adds total `formatBool` and one exact-width function named
`formatI8` through `formatU64`. They return the same spelling used by
interpolation. Integer formatting emits one or more lower-case digits, a
leading `-` only for a negative signed value, and no prefix, plus sign,
grouping, whitespace, or padding. `formatF32` and `formatF64` retain D046's
raw-bit contracts; interpolation does not widen `f32` implicitly.

`parseInt(text)` is `parseIntRadix(text, 10)`. Both return
`Result<int, ParseError>`, where `int` is `i64` and:

```marimay
public enum ParseError {
  Empty,
  InvalidDigit { offset: int; },
  OutOfRange,
  InvalidRadix,
}
```

A radix from 2 through 36 is accepted. Digits are ASCII `0`–`9`, `a`–`z`, or
`A`–`Z`; the case variants have the same value. One leading `-` is accepted,
but `+`, whitespace, prefixes, separators, and trailing text are not. Leading
zeroes are accepted and ignored, including negative zero; formatting never
preserves them. Empty means exactly zero input bytes. A sign without a digit
is `InvalidDigit` at offset 1. `InvalidDigit.offset` is the zero-based byte
offset of the first unacceptable byte. Magnitudes outside `i64`, including
one beyond either endpoint, are `OutOfRange`. A radix outside the closed range
is `InvalidRadix` before text inspection.

All conversion operates on counted UTF-8 bytes and never calls host locale,
`printf`, `strto*`, or NUL-terminated text authority. Dynamic construction is
bounded by the existing MIR string-byte and managed-heap budgets. Overflow,
limit, or allocation failure follows the existing deterministic resource
outcome; partial strings are not observable, and every temporary allocation
is released on all paths. `println` still accepts only the resulting `string`
and publishes it under D046.

Reason: CLI diagnostics need useful deterministic text before general builders
or streams exist. A closed primitive family provides that vertical slice
without committing the language to an open formatting protocol or hidden
conversion graph.

## D066 — Program arguments and exit status are bounded root inputs

Status: accepted.

The executable entry remains the root package's exact `main`. It accepts zero
parameters or one exact `[]string` parameter and returns `void`, `int`,
`Result<void, E>`, or `Result<int, E>`. Existing D049 forms remain valid. For
either `Result` form, `E` retains D049's root-visible structural
`message(error: E): string` contract. An argument-taking `main` receives only
the program arguments: the executable name is excluded. No environment,
process, filesystem, or shell capability is implied.

`mmay run [target] -- ...` transports every value after the first separator in
order. `mmay test [target] -- ...` injects the same immutable array into each
`@test fn(arguments: []string): void`; the existing zero-parameter test form
remains valid and receives no hidden parameter. A native executable imports
the host arguments after `argv[0]`. POSIX bytes must be valid UTF-8. Windows
uses the wide CRT entry and strict UTF-16-to-UTF-8 conversion after the CRT's
standard quoting/backslash decoding; shell expansion is never repeated by
Marimay. Empty strings, spaces, leading dashes, and a literal `--` after the
tool separator are ordinary values.

Before command parsing, the `mmay` tool itself imports the complete host
invocation, including its executable name, with a separate defensive ceiling
of 8192 values, 262144 bytes per value, and 8388608 bytes total. It uses the
same POSIX/Windows strict conversion. Invalid host encoding or those outer
limits are `U0007`/status `2`; allocation failure is `I0002`/status `70`.
The selected source-program suffix is then independently checked against the
tighter limits below, so tool arguments cannot consume the program's budget.

Every value is counted UTF-8 without an interior NUL. The boundary accepts at
most 4096 arguments, at most 262144 bytes in one argument, and at most 1048576
argument bytes in total. Validation completes before source execution. Invalid
encoding, NUL, or a limit violation is `G4006`, writes no program stdout, and
exits `1`. Allocation or managed-heap exhaustion while importing a valid list
retains the exact D055 `I0002` classification and exits `70`. Tests can inject
counted hostile values through the execution adapter, including NUL, without
depending on what a host argv representation can express.

`void` and `Result.Ok()` select status `0`. A direct `int` return and the
payload of `Result.Ok(status)` select an exact portable process status from 0
through 255 inclusive. Another value is `G4007` with exact message
`program exit status is outside 0..255` and exits `1`; it is not truncated by
the host. `Result.Err` remains `G4003`/status `1`. An active panic, cleanup
panic, internal failure, or invalid status takes precedence over a pending
successful status according to D041/D049. Because the status is observed only
after the root call returns, all lexical deferred calls have completed exactly
once before it is published. There is no safe immediate-termination primitive.

VM execution arguments, encoded bytecode parameter/result layouts, native
entry construction, and test replay all use the same typed `[]string` and
`int` metadata. `mmay run` returns the selected program status verbatim, even
when its numeric value overlaps a tool-owned status; stderr diagnostic identity
distinguishes tool/runtime failure. Native and VM must agree on arguments,
stdout, stderr, cleanup, and the selected status.

Reason: a root parameter and return value make both inputs and termination
visible in ordinary typed control flow. They preserve lexical cleanup and
avoid a hidden global accessor, shell semantics, an immediate `exit`, or a
second test-only entry path.

## D067 — Match arms are comma-separated expressions with explicit remainder capture

Status: accepted.

`match` is expression-oriented throughout. An arm is `pattern [if guard] =>
body`, arms are separated by commas, and a trailing comma is accepted. The
`case` introducer and semicolon-terminated expression arms are removed. An arm
body is either one expression or a block whose final non-terminated expression
is the arm value. Every reachable block-arm path must reach that tail
expression, return from the enclosing function, panic, or otherwise have type
`never`. The contextual `yield expression;` transfer form is removed; `yield`
is again an ordinary identifier in every context.

An unguarded wildcard `_` remains a discard catch-all. A catch-all that retains
the matched value is written `_ as name`. A bare identifier at an arm's root is
always interpreted as a unit constructor and an unknown spelling is an unknown
variant, never a new binding. Payload and named-field positions retain ordinary
binding shorthand, so `Failed(error)` and `Point { x, y }` remain compact.

This decision supersedes D005 only for arm spelling and remainder capture,
D038 for `case`/`yield`, and D031 where match layout assumed semicolon arms. It
does not change exhaustiveness, guard, OR-pattern, evaluation-order, or nominal
pattern semantics.

Reason: a match is already an expression, so statement punctuation and a
generator-associated transfer word add a second local value model. Commas make
the arms visibly a list, a tail expression matches other expression blocks,
and explicit remainder capture prevents a misspelled unit variant from silently
becoming an exhaustive binding.

## D068 — One `for name in iterable` model with closed integer ranges

Status: accepted.

Marimay has one `for` header: `for name in expression`. The iteration binding
is an immutable identifier and is freshly created for each iteration. `_` is
not a loop binding. Tuple and array patterns are rejected in this position;
destructuring belongs in a `match` inside the loop body. Recovery of an
invalid compound binding synchronizes at the header separator or body
boundary instead of treating its tokens as the iterable and consuming later
declarations. This clarifies error recovery without adding a binding form.
Arrays and standard iterators use the existing single-
evaluation, cleanup, and invalidation contracts.

The language adds integer range forms `start..end` and `start..=end` in the
iterable position of a `for` header. Both operands must have exact type
`int`/`i64` and are evaluated exactly once from left to right before iteration.
`..` excludes `end`; `..=` includes it. A range with `start > end` is empty.
Range iteration cannot overflow when advancing the final `int::MAX` element.
In 0.1 a range is a compiler-known loop iterable, not a first-class value: it
cannot be stored, returned, passed, or constructed outside that header.
Stateful loops use `while`; the C-style `for let ...; ...; ...` header and
`for const name of expression` are removed without compatibility aliases.

This decision supersedes D033 and the loop-header portion of D038. D053 array
iterator generations, lexical loop control, and closure capture remain intact.

Reason: iteration is the common operation, while a second declaration and
punctuation-heavy C header duplicates `while`. A closed range type covers
ordinary counters without implicit conversion or backend-specific arithmetic.

## D069 — Enum constructors participate in expected-type and contextual mapper inference

Status: accepted.

A unit enum constructor is a value. When the expected type uniquely selects a
concrete enum instantiation, `None` is sufficient for `Option<T>.None`; explicit
`None<T>()` remains unnecessary and is removed from canonical examples. A
positional enum constructor may occupy an exact contextual unary-mapper
position whose input is its payload and whose output is its enum. In 0.1,
compiler-known `Result.mapError` provides such a position, so it may select
`AppError.Network` as the mapping from `http.Error` to `AppError`. This is a
contextual constructor application, not a materialized first-class function:
the constructor cannot be stored, returned, or passed through an ordinary
function-typed value. Unit and named-payload constructors are not implicitly
callable.

Generic arguments are inferred from the expected result/function type and from
payload arguments. Inference must still produce one complete concrete nominal
type; ambiguity is a type error. Ordinary shadowing and visibility apply before
prelude or enum-constructor interpretation. Closure parameter and result types
may be omitted for an expression-bodied closure only when a compiler-known
higher-order operation supplies one exact input and result type. In 0.1 this
contextual closure rule is implemented for `Result.mapError`; expanding it to
ordinary function-valued parameters requires a separate decision and runtime
representation.

Reason: constructors already have deterministic typed call behavior. The
contextual mapper form removes mechanical mapping lambdas at the supported
higher-order boundary without claiming a general closure representation,
implicit error conversion, or an overload facility.

## D070 — `as` is total; fallible integer conversion is named

Status: accepted.

`value as Target` is valid only when every value of the source integer type is
representable in `Target`; its type is always `Target`. Potentially narrowing
or signedness-changing conversion is `tryConvert<Target>(value)` and has type
`Option<Target>`. Both forms cover exactly the eight D047 integer primitives,
perform no implicit integer/float conversion, and never wrap, saturate, or
panic. `tryConvert` evaluates its argument once and returns `None` when the
runtime value is not representable.

This decision supersedes the conversion-expression result rule in D047 while
retaining its arithmetic policy families and runtime representability rules.

Reason: the spelling of an operation should reveal whether failure is part of
its type. A single `as` whose result alternates between `T` and `Option<T>`
requires readers to reproduce the integer range table at every use site.

## D071 — Tuples use parentheses and data declarations use commas

Status: accepted.

Tuple types, values, and patterns use parentheses: `(string, int)`,
`("port", 8080)`, and `(name, count)`. A one-element tuple requires a trailing
comma; ordinary `(expression)` remains grouping and `(T) => U` remains a
function type. Brackets are reserved for `[]T`, array literals, and indexing.

Record fields and named enum-payload fields are comma-separated data items with
an optional trailing comma. Enum variants, construction fields, and pattern
fields already follow the same rule. Interface/function signatures, imports,
aliases, and executable statements retain semicolons.

This decision supersedes D025, the tuple/data examples in D031 and D054, and
the corresponding sections of the baseline grammar.

Clarification, 2026-11-08: the grammar rows for `tuple_type` and
`positional_payload` omitted optional final commas already accepted by the
parser, while the formatter contract requires trailing commas in broken
lists. A formatter bullet also retained D025/D031's named-payload semicolons
despite this decision and the language specification requiring commas. Those
projections are inconsistent and must follow D071.

Parenthesized tuple-element, function-parameter-type, and positional
enum-payload-type lists permit an optional final comma without changing their
types, arity, order, or evaluation. A singleton tuple still requires its first
comma: `(T,)` is a tuple type, `(value,)` is a tuple value, and `(expression)`
remains grouping. `(T)` alone is not a type; `(T) => U` is a function type.
Named payload fields use commas, including an optional final comma. Correcting
the grammar, formatter comparison oracles, and stale prose records this
existing contract; it does not introduce a new runtime feature.

Reason: tuple/array identity should be visible without contextual typing, and
data-member lists should not alternate between statement punctuation inside an
item and list punctuation immediately outside it.

## D072 — Every active 0.1 grammar word is a hard keyword

Status: accepted.

The hard-keyword set is `as break const continue defer else enum extern false
fn for from if import in interface let match mutable public return scope spawn
true type unsafe while`. These spellings are unavailable as identifiers in all
source contexts. `case`, `of`, and `yield` no longer have grammar roles and are
ordinary identifiers. `test`, `fuzz`, `panic`, `any`, `async`, `await`,
`class`, `extends`, `impl`, and `throws` remain ordinary identifiers. `null`
and `undefined` remain reserved sentinels.

This decision supersedes D038's contextual-keyword boundary. It does not
reserve words for constructs absent from the language.

Reason: active grammar spellings used as API names complicate recovery,
highlighting, documentation, and human scanning. Reserving only words that
actually participate in 0.1 keeps the boundary finite without occupying
hypothetical future syntax.

## D073 — Canonical formatting preserves statement grouping and breaks boolean chains

Status: accepted.

Within a statement block, the formatter preserves at most one user-authored
empty line between adjacent statements. Additional blank lines collapse to
one; a source gap without an empty line is not expanded except by existing
structural rules. At a required line break, a chain of `&&` or `||` breaks
before each operator with the operator aligned at the continuation indent.
The continuation belongs to that logical expression, not to an enclosing
statement: it ends before a following match arm, argument, field, or closing
delimiter outside the expression. Short chains stay flat when they fit even
inside an otherwise multiline match or statement. Parenthesized inner chains
have their own group; interpolation remains an unbreakable group under D065.
Index brackets stay attached to a single scalar or otherwise-fitting index
expression; the formatter does not produce a three-line `values[\n  0\n]`
shape merely because the surrounding expression exceeds the target width.

This refines D015, D026, and D031 without adding configuration or changing the
100-column metric.

Reason: blank lines communicate local statement groups, while leading boolean
operators make long conditions and returns scan predictably. Isolating a short
index on its own lines spends vertical space without exposing structure.

## D074 — Discarding `Result` requires explicit source intent

Status: accepted.

An expression statement whose value has type `Result<T,E>` is a type-check
error. The programmer must propagate with `?`, inspect with `match` or a
consuming API, return it, or explicitly discard it through `const _ =
expression;`. Calls returning `void` remain valid expression statements.
This rule applies equally to free calls, dot calls, compiler-known operations,
and both execution engines; it introduces no implicit unwrap or cleanup policy.

Reason: expected failure is part of the type. Making accidental discard look
identical to a `void` call defeats that visibility, while the existing discard
binding gives deliberate fire-and-forget code an explicit spelling.

## D075 — Bounded typed standard streams

Status: accepted.

`std/io` provides `readLine(limit)`, `print`, `println`, `eprint`, `eprintln`,
`flushStdout`, and `flushStderr`, all returning typed `Result` values. Its
`Error` variants are `CapabilityDenied`, `ReadFailed`, `WriteFailed`,
`FlushFailed`, `InvalidUtf8`, and `LineTooLong { limit }`.

`readLine` requires `io.stdin`, accepts limits in `0..1048576`, strips LF and
an immediately preceding CR, and returns `Ok(None)` only for EOF before bytes.
It retains NUL, rejects invalid UTF-8, and never publishes partial text. Output
operations require their exact stdout or stderr capability; each stream stays
ordered and flush affects only its own stream. Failures expose no host error
text or partial count. The Stage 0 adapter may block and retry bounded host
operations; I6 must replace it without changing observable behavior.

The limit counts bytes in the returned line after LF/CRLF removal, so a CRLF
terminator does not consume the limit; a CR at EOF is data. An invalid limit
returns `LineTooLong { limit }` before a capability check and preserves the
requested signed value. EOF after one or more bytes returns `Ok(Some(text))`.
`print` and `eprint` append nothing, while `println` and `eprintln` append one
LF; none flushes implicitly. An empty output request still checks its exact
capability but performs no host write. Only `flushStdout` or `flushStderr`
requests a flush.

A zero-progress interrupted host operation may be retried at most 1024 times
after its initial attempt. A positive short write commits that prefix, resets
the consecutive-retry budget, and continues in order. A final write or flush
failure may therefore leave a prefix externally visible, but its source error
contains neither a byte count nor host text. A read failure, invalid UTF-8, or
line-limit failure publishes no `string`. Internal allocation, integrity, or
resource-budget failures remain runtime `I0002` outcomes rather than invented
`std/io.Error` variants. The stable `message` spellings are the declarations
in `std/io`; they contain no `errno`, locale, or backend name.

This supersedes D046's stdout-only preview while retaining its string-only,
UTF-8 output contract.

Reason: synchronous typed streams provide the bounded CLI boundary required
before Bytes, files, parsing, and JSON without promising task-aware I/O early.

## D076 — Owned UTF-8 views, nominal Bytes, and bounded builders

Status: accepted.

The CLI alpha keeps `string` immutable and valid UTF-8. `std/text` owns the
dot-call set for `string` and exposes counted byte length, Unicode-scalar
count/copy, an alpha grapheme copy, byte-offset substring, exact-byte
contains/prefix/suffix, split, join, and ASCII trim. Integer indexing remains
invalid. Every operation is locale-independent and compares exact counted
bytes without normalization.

Substring offsets are zero-based byte offsets. Both endpoints must be in
`0..byteLength(text)`, `start <= end`, and Unicode-scalar boundaries; failure
is a typed `BoundaryError`, and success returns an owned `string`. Rune and
grapheme APIs also return owned arrays. The alpha grapheme rule starts a new
cluster at each scalar except CRLF stays together and a scalar whose pinned
Unicode 17 formatter width is zero stays with the preceding cluster. This is
an explicit bounded preview subset, not a claim of complete UAX #29
segmentation. `trimAscii` removes only ASCII space, tab, LF, CR, vertical tab,
and form feed from both ends.

`split(text, separator, limit)` rejects an empty separator and accepts a part
limit in `0..65536`; exact non-overlapping byte matches are selected from left
to right, trailing empty parts are retained, and exceeding the limit is typed.
`join(parts, separator, limit)` and rune/grapheme copies accept byte/item limits
in `0..1048576` and publish nothing when the required count exceeds the limit.
Limits are checked before result construction. Internal allocation or managed-
heap failure remains D055 `I0002`, never a fabricated text error.

`std/bytes.Bytes` is a nominal managed record whose private storage is an exact
`[]byte`. Copy/from-array/to-array/slice create independent storage; assigning
or passing `Bytes` aliases the same identity, so mutation through either alias
is observable. Index access and mutation retain D045 bounds behavior. Push,
append, reserve, clear, and iteration use the underlying D053 generation and
growth contracts. A `Bytes.Iterator` captures that generation and `next`
panics with `G4005` after any initialized-sequence mutation. No private array
or borrowed view escapes the owner in the alpha API.

UTF-8 conversion is explicit. `encodeUtf8(string)` copies exact bytes into a
new `Bytes`; `decodeUtf8(Bytes)` returns `Result<string, Utf8Error>` and reports
the first malformed byte offset. Neither conversion accepts replacement,
normalization, host locale, NUL termination, or implicit coercion. Embedded
NUL is ordinary data in both directions.

`std/text.Builder` and `std/bytes.Builder` are nominal mutable builders created
with a byte limit in `0..1048576`. Each append is failure-atomic with respect
to the declared limit, `finish` returns an owned snapshot, later appends do not
change an earlier result, and no input or output can bypass the limit. Text
builders retain string chunks and perform one measured join at `finish`;
byte builders reserve once before copying an append. Allocation/resource
failures retain D055 classification.

D075 is extended with `readBytes(limit)`, `writeBytes`, and `ewriteBytes`.
`readBytes` requires `io.stdin`, accepts `0..1048576`, reads through EOF, and
returns an empty `Bytes` at immediate EOF. More than `limit` bytes is
`InputTooLong { limit }`; no prefix escapes. Binary writes require the exact
stdout/stderr capability and use D075 partial-write/retry ordering. Text and
binary operations share the same stream, capture, flush, and diagnostic
separation contracts.

Reason: files, CLI parsing, and JSON need a safe counted data foundation now,
while suspension-safe borrowed lifetimes do not exist until I6. Owned copies
and explicit limits make the alpha semantics complete without freezing a
borrowed-view ABI or introducing implicit text/byte conversions.

## D077 — Typed paths and capability-confined synchronous files

Status: accepted.

The CLI alpha `std/path.Path` is a nominal owned path whose counted UTF-8
spelling is lexically normalized for the target host. POSIX accepts `/` as a
separator. Windows accepts `/` and `\\`, emits `/`, upper-cases an ASCII drive
letter, accepts absolute drive/UNC/root forms, and rejects drive-relative,
device, extended-length, reserved-device, trailing-dot/space, and other
non-portable component spellings. Empty and dot-only input normalize to `.`;
repeated separators and `.` are removed, and `..` removes one preceding
ordinary component. An absolute path may not ascend above its root. Every
normalized path is at most 4096 bytes including neither a terminator nor hidden
host state. Normalization is lexical and never claims symlink resolution.

`mmay run`, `mmay test`, and `mmay build` accept repeatable
`--allow-read PATH` and `--allow-write PATH`. A root is resolved against the
launch working directory, lexically normalized into an absolute path, and
stored as a component-bounded capability. A requested relative `Path` is
resolved against that same directory. Matching uses whole components (and
ASCII case folding for Windows drive and ordinary ASCII path bytes); write
does not imply read, while write is sufficient for the tool-created sibling
temporary used by atomic publication. Invalid or oversized resolved roots and
more than 64 roots of either class are `U0005` command errors; a missing option
value is `U0004`, and use on an unsupported command is the existing `U0002`
unknown-option error.

Capability matching happens on the normalized absolute spelling before any
target open, read, write, metadata lookup, or directory traversal. After a
match, every existing component from the capability root through the target is
inspected without following links. A POSIX symbolic link or Windows reparse
point is `Symlink`, including the final target. Thus no capability-authorized
operation can escape through a host alias. Missing final write targets are
allowed only when their existing parent chain is symlink-free; all other
missing components are `NotFound`. This deliberately conservative alpha
policy avoids freezing a canonicalization/alias ABI before broader sandbox
work.

`std/fs` exposes read-only `open`, bounded `read`, idempotent fallible `close`,
and idempotent non-panicking `closeBestEffort`. Handles are execution-local,
non-forgeable integers wrapped by a nominal `File`; a closed/foreign handle is
`Closed`. Every surviving handle is best-effort closed by VM/oracle/native
execution teardown on normal return, `Err`, panic, resource failure, or host
adapter failure. An explicit close failure is returned when no earlier
operation failed; convenience reads preserve their read/open failure and
attempt best-effort cleanup, following D041.

Whole-file byte and text reads accept limits in `0..1048576`, never publish a
prefix on failure, and distinguish invalid limits, over-limit input, invalid
UTF-8, missing, wrong-kind, denied, link, open/read/close, and internal resource
failure. Text decoding reports the first invalid byte. Atomic byte/text writes
create an exclusive same-directory tool-owned sibling, consume short writes in
order, sync, close, then replace; every failure removes the temporary when
possible and leaves the prior destination unchanged until replace. Stable
source errors expose no `errno`, locale text, host path outside the requested
bounded spelling, or partial byte count.

Metadata reports `File`/`Directory` plus a non-negative byte size for regular
files and zero for directories. Directory enumeration accepts an entry limit
in `0..65536`, excludes `.` and `..`, retains an explicit
file/directory/symlink/other kind, owns every UTF-8 name, and publishes only
after sorting by unsigned UTF-8 bytes. Exceeding the limit, invalid host text,
iteration failure, allocation failure, or close failure publishes no array.
The Stage 0 adapters may block; I6 must replace blocking waits without changing
these observable results.

Reason: local CLI workloads need files now, but ambient access, link-following
policy, unbounded whole-input helpers, and host-error leakage would create a
security and portability contract that cannot safely be repaired later.

## D078 — Named environment reads and explicit process context

Status: accepted.

The CLI-alpha environment surface is lookup-only. `std/env.get(name)` returns
`Result<Option<string>, env.Error>`: an absent variable is `Ok(None)`, an
empty value is `Ok(Some(""))`, and a present value is published only after
strict UTF-8 validation and the one-mebibyte value limit. Names are portable
ASCII identifiers matching `[A-Za-z_][A-Za-z0-9_]*` and are at most 255 bytes.
Invalid source names, denied names, malformed host text, over-limit values,
and host lookup failures remain distinct. Environment enumeration is not part
of this decision.

`mmay run`, `mmay test`, and `mmay build` accept repeatable
`--allow-env NAME`. A grant authorizes exactly one name. POSIX matching is
byte-exact; Windows matching and duplicate detection use ASCII case folding,
matching the platform environment namespace. Exact duplicates, Windows-folded
duplicates on Windows, invalid names, and more than 64 grants are `U0005`.
The grant list is the complete environment capability input to VM and native
execution. A denied lookup performs no host environment query.

`std/process.currentDirectory()` and `std/process.executablePath()` return
`Result<path.Path, process.ContextError>`. The current directory is captured
for each fresh execution before source code starts. The executable path is the
absolute identity of the process hosting that execution: the `mmay` process
for VM execution and the generated artifact for native execution. Controlled
tests may replace both values with the same injected context; VM/native
equivalence is required for the same injected context, not for two different
host executable identities. Both paths use D077 normalization and reject
invalid host encoding, an unavailable host value, or a non-portable path
without exposing raw host error text.

Environment values are controlled nondeterministic inputs. Tests receive no
host environment access by default; a real `--allow-env` grant opts the test
out of deterministic result caching. Build cache keys include the ordered,
deduplicated grant names because generated native policy changes, but never an
ambient value. No environment variable overrides compiler selection, heap
limits, capabilities, paths, runtime policy, or another semantic option.

Stable errors and diagnostics may contain the requested environment name, the
fixed capability name `env.read`, a byte offset, or a fixed limit. They never
contain an environment value, a value prefix, host error text/code, or an
ungranted name discovered from the host. Allocation and internal descriptor
failure remain `I0002` rather than a fabricated source `ReadFailed`.

Reason: local CLI programs need configuration and process location before the
first-party option parser, while ambient enumeration, hidden policy overrides,
and secret-bearing diagnostics would make builds non-hermetic and capability
auditing unreliable.

## D079 — Ordered Map Set and checked hash-key coherence

Status: accepted.

The CLI-alpha collection surface adds compiler-known managed `Map<K,V>` and
`Set<T>` types. They have stable reference identity, never expose capacity or
hash-table position, and use these ordinary free/dot-call-equivalent
operations:

```marimay
public fn mapOf<K, V>(): Map<K, V>;
public fn length<K, V>(values: Map<K, V>): int;
public fn containsKey<K, V>(values: Map<K, V>, key: K): bool;
public fn get<K, V>(values: Map<K, V>, key: K): Option<V>;
public fn put<K, V>(
  values: Map<K, V>,
  key: K,
  value: V,
): Result<bool, CollectionGrowthError>;
public fn remove<K, V>(values: Map<K, V>, key: K): Option<V>;
public fn clear<K, V>(values: Map<K, V>): void;

public fn setOf<T>(): Set<T>;
public fn length<T>(values: Set<T>): int;
public fn contains<T>(values: Set<T>, value: T): bool;
public fn add<T>(
  values: Set<T>,
  value: T,
): Result<bool, CollectionGrowthError>;
public fn remove<T>(values: Set<T>, value: T): bool;
public fn clear<T>(values: Set<T>): void;

public enum CollectionGrowthError {
  LengthLimit { limit: int },
}
```

`put` returns `Ok(true)` for a newly inserted key and `Ok(false)` for an
update. An update replaces only the value and retains the key's position.
`add` returns `Ok(true)` for a newly inserted value and `Ok(false)` for an
already-present value. A new entry at `int::MAX` returns
`Err(CollectionGrowthError.LengthLimit { limit: int::MAX })` without
mutation; updating an existing map key or adding an existing set value remains
valid at that length. Managed-budget and host-allocation failures retain D055
`I0002` classification and never become a source growth error. `get` and map
`remove` use `None` for absence. Set `remove` returns whether it removed a
value. An empty `clear` is a no-op.

Iteration uses the existing `for item in values` spelling. A map item has exact
tuple type `(K, V)` and a set item has exact type `T`. Iteration is insertion
ordered. Updating a map retains position; removing and later reinserting a key
or value appends it. Resizing, hash-table layout, tombstones, and hash seeds
are unobservable. A runtime may randomize hashing, but a controlled execution
can inject both 64-bit seed words; changing them cannot change visible order
or any result.

Supported hash keys are `bool`, every integer width, `rune`, `string`, and an
immutable nominal record or enum with the closed `@derive(Eq, Hash)` pair whose
complete stored payload recursively has supported key types. Floating-point,
arrays, maps, sets, functions, interfaces, mutable records, incomplete derive
pairs, and unconstrained generic parameters are rejected as keys. Integer
keys compare at their exact declared width, strings compare complete counted
UTF-8 bytes without normalization, and derived nominals compare their exact
nominal type followed by declaration-order payload values. Equality must be
reflexive, symmetric, and transitive, and equal values must hash equally.

Each collection has one non-wrapping mutation generation. A new map insertion,
map value update, successful removal, or nonempty clear increments it once
after commit. A new set insertion, successful removal, or nonempty clear does
the same; a duplicate `add` does not. Absence, growth errors, and allocation or
resource failures do not increment it and are failure-atomic. Iteration
captures the generation and checks it before every subsequent test or item
yield. A mismatch panics with `G4005`, respectively
`map iterator invalidated by mutation` or
`set iterator invalidated by mutation`, at the complete loop header. Every
normal, break, continue, return, and panic exit closes the compiler-owned
iterator state without a further check.

Reason: CLI parsing and typed JSON need deterministic keyed collections, but
iteration cannot depend on hash bucket order and user hashing cannot be
accepted without equality coherence. A closed derive/key set and
failure-atomic managed layout provide a complete alpha contract without
freezing custom hashing, concurrent collections, allocators, weak keys, or
observable capacity.

## D080 — Preview structural equality, expectations, and actionable incompleteness

Status: accepted.

The checker-backed preview executes `==` and `!=` for identical primitive
types, counted UTF-8 `string`, closed enums including `Option` and `Result`,
and immutable records marked `@derive(Eq)`. Strings compare their complete
counted bytes without normalization. Enums compare the selected variant and
then its payload in declaration order. Records compare fields in declaration
order. A compound type is equatable only when every reachable compared field
or payload is equatable; recursively self-referential nominal equality remains
staged until compiler-generated recursive functions exist. Both operands are
evaluated exactly once, left to right. Record identity remains exclusively
`identity.same`.

The primitive set includes `rune`: equality compares the complete Unicode
scalar value, without integer coercion or normalization. The C17 bootstrap
uses the existing typed integer-equality MIR/wire operation for two exact
`rune` registers, with a `bool` destination. This does not enable rune
arithmetic or ordered integer operations; their verifier rules stay separate.

The closed derive inventory is `Debug`, `Eq`, and `Hash`. They may be listed
independently except that `Hash` requires `Eq` in the same attribute. `Eq` and
`Hash` remain invalid on a record with any mutable field. In this preview,
`Debug` publishes compiler metadata for diagnostics and test tooling but does
not create a user-callable formatter; D065 still defers user-defined
formatting. The C17 bootstrap may lower derived equality directly into typed
MIR at its use site instead of materializing the 1.0 ordinary-HIR function,
provided name resolution, evaluation order, result, panic behavior, and both
engines are observationally identical.

Inside `@test` and `@fuzz`, the compiler-known expectations `equal`,
`notEqual`, `true`, `false`, `some`, `none`, `ok`, and `err` are executable.
They evaluate arguments once from left to right and emit a typed
`TEST_ASSERT(bool, string)` MIR operation. Failure uses `G4004` at the complete
expectation call with one of these exact bounded messages:

```text
expect.equal failed: values differ at root
expect.notEqual failed: values are equal at root
expect.true failed: value is false
expect.false failed: value is true
expect.some failed: value is None
expect.none failed: value is Some
expect.ok failed: value is Err
expect.err failed: value is Ok
```

The root mismatch is the bounded preview structural diff; richer nested paths
and rendered values may be added without changing equality. `expect.matches`
and `expect.panics` remain staged. VM and native consume the same assertion and
structural-equality MIR, and differential tests compare their exact status,
output, panic code, message, and span.

Every accepted construct that reaches an incomplete checker path produces an
owned `I0001` diagnostic naming the construct and pointing at its exact source
node. Checker and compiler-service results retain that diagnostic while
returning the incomplete status; CLI commands render it and exit `70`.

This does not classify an ill-typed call as an incomplete feature: a local
binding with a non-function type produces `T0002` at its callee name, including
when it shadows an import. Namespace qualification likewise respects local
shadowing before falling back to the imported package.

Reason: the language tour, ordinary equality, and basic test helpers are one
user-facing slice. Leaving any one of them as a spanless internal failure makes
the others impossible to validate through the documented VM/native workflow.

## D081 — Bounded declarative first-party command-line parsing

Status: accepted.

The CLI-alpha application parser is the ordinary `std/cli` package. A parser
declares one program name, a single-line summary, an ordered option list, an
ordered positional list, and explicit `Limits`. Options have an exact UTF-8
long name, an optional one-byte ASCII alphanumeric short name, a flag,
required-value, or optional-value mode, and independent required/repeatable
bits. Positionals have an exact UTF-8 name plus required/repeatable bits. A
repeatable positional must be last, and a required positional cannot follow an
optional positional. Duplicate long, short, or positional names and every
malformed or over-limit definition are typed `DefinitionError` values.

Long options accept `--name` and `--name=value`. A required value may instead
be the next argument, including a spelling beginning with `-`; an attached
empty value is still a present value. An optional value is consumed only when
attached with `=`. Short flags may cluster. The first short value-taking option
consumes the rest of its token when nonempty; otherwise a required value
consumes the next argument and an optional value is absent. The exact `--`
token disables option recognition and is not itself positional. A lone `-` is
positional. No shell quoting, expansion, environment lookup, abbreviation,
negated flag, or implicit conversion is performed.

Parsing preserves occurrence and positional order. A nonrepeatable duplicate,
unknown option, unexpected or missing value, unexpected positional, missing
required declaration, or argument-count overflow is a typed `UsageError`.
Argument indices are zero-based indices into the caller-supplied `[]string`.
The recommended usage exit status is exactly 2. Runtime managed-budget or host
allocation failure remains the existing `I0002` execution outcome rather than
a fabricated usage error.

Help is deterministic declaration-order UTF-8 text. Width is a byte width in
`40..120`; synopsis tokens and words are never split, continuation text uses a
four-space indent, and an unrepresentable line or help-byte budget produces a
typed `HelpError`. Default limits are 4096 arguments, 128 options, 64
positionals, and 65536 help bytes. Accepted custom maxima are respectively
4096, 256, 256, and 1048576, with a 256-byte minimum help budget. The complete
API and validation grammar are normative in the standard-library
specification.

Subcommands, aliases beyond one long/short pair, automatic completion,
environment/config defaults, and localized or terminal-display-width help are
absent rather than stubbed. A later subcommand model requires a separate
decision because it changes error routing, help shape, and positional scope.

`std/cli` is implemented only with shipped public string/text, array,
`Option`/`Result`, `Map`, and `Set` contracts. The C17 toolchain embeds its
ordinary source documents but does not recognize parser symbols, add MIR or
bytecode operations, or delegate parsing to host C.

Reason: real CLI applications need a reusable deterministic boundary, while a
host parser or unbounded reflection-based schema would bypass the language
surface that CLI0 is intended to prove.

## D082 — Explicit bounded typed JSON codecs

Status: accepted.

The CLI-alpha JSON surface is the ordinary first-party `std/json` package.
It exposes stateful `Decoder` and `Encoder` cursor values, typed errors with
byte spans, and explicit `Limits`. Application packages define ordinary
nominal codec values and top-level `decode(codec, decoder)` and
`encode(codec, encoder, value)` functions. The first parameter makes those
functions dot-call eligible under D003. There is no compiler-known
`decode<T>` in the original CP38 surface. D086 extends this boundary with
closed, compiler-checked convenience calls; it does not add derive syntax,
a dynamic value tree, a schema registry, or runtime reflection. A codec can
read only fields visible to its package; private
fields cannot be discovered or modified by `std/json`.

The decoder accepts only valid UTF-8 and the JSON lexical grammar. Strings
decode the defined escapes and paired UTF-16 surrogate escapes; unescaped
control bytes, malformed escapes, and unpaired surrogates are errors. JSON
numbers are grammar-validated and available as their exact source spelling;
the typed integer reader additionally requires the signed 64-bit `int` range.
Object keys are strings and duplicates are rejected before a codec receives a
second value. A codec chooses `UnknownFields.Reject` or
`UnknownFields.Skip`; skipping uses the same nesting, item, string, number,
and byte budgets as ordinary decoding. Missing fields and unknown enum tags
remain explicit typed codec errors. `Bytes` uses a JSON array of integers in
`0..255`.

Encoding is driven entirely by codec call order. Object fields therefore use
the order in which the codec calls `writeKey`; map-backed fields use D079
insertion order. The encoder emits no insignificant whitespace, uses the
short escapes for quote, backslash, and the defined control characters,
lowercase `\u00xx` for other controls, and literal UTF-8 for other scalar
values. It does not escape `/`. `writeNumber` accepts only one complete JSON
number spelling. A failed encoder is poisoned and cannot publish partial
bytes through `finishEncoder`.

Default limits are 1 MiB input, 1 MiB output, 256 KiB per decoded or encoded
string, 128 number bytes, 128 nested containers, and 65,536 members per
container. Custom limits remain within the exact maxima in the
standard-library specification. Input is copied when a decoder is created,
every limit is checked before publishing the affected result, and allocator
or managed-heap exhaustion remains the existing `I0002` runtime outcome
rather than a fabricated JSON error. Error spans are half-open byte offsets
into the decoder's owned input. Messages contain fixed resource names,
limits, field/tag text supplied by the input or codec, and offsets; they do
not expose host errors, paths, capabilities, or unrelated data.

The C17 toolchain embeds the three ordinary `std/json` source documents.
D086 recognizes only its closed convenience entry points and expands them
to ordinary typed MIR calls to these documents. It does not lower a
JSON-specific MIR/bytecode opcode or call a host JSON library. VM and native execution consume the same typed
MIR and shared managed collection contracts. File and stream integration is
composition with `std/fs`, `std/io`, and `std/bytes`; JSON itself gains no
ambient file, standard-stream, network, or environment access.

Reason: typed local data transformation is required for CLI0, while generic
reflection or an unbounded DOM would violate package privacy and resource
contracts. Explicit owner-package codecs prove records, enums, arrays, maps,
text, bytes, files, and both engines without freezing new derive syntax or a
dynamic `any` escape hatch.

## D083 — CLI-alpha installations are relocatable, inspectable, and local-only

Status: accepted.

An installable CLI-alpha candidate is one version-locked prefix. It contains
exactly one user command `bin/mmay`; version-matched debug and release native
runtime archives and manifests under `lib/mmay/V/native/{debug,release}`;
public compiler/runtime headers under `include/mmay`; the shipped `std` source,
release metadata, provenance statement, SPDX SBOM, and checksum manifest under
`share/mmay/V`; and offline documentation plus the corresponding source kit
under `share/doc/mmay/V`. `V` is the common compiler/runtime/standard-library
version. Complete-prefix relocation does not change command behavior or cache
keys. The old `ma` and `ruda` binaries and paths are never installed as aliases.

Per-host archives are named `marimay-V-HOST.{tar.gz|zip}` and the portable
source archive is `marimay-V-source.tar.gz`. Every archive has a sibling
`.sha256`; the checksum manifest covers every shipped regular file by
prefix-relative UTF-8 path. `provenance.json` records its versioned schema,
source revision, source-date epoch, builder identity, host tuple, compiler
identity, build profile, and artifact hashes. `sbom.spdx.json` is SPDX 2.3 and
lists the Marimay compiler, both runtime profiles, shipped standard packages,
offline docs, and source kit. Generation is deterministic from explicit CMake
inputs and performs no signing, upload, or publication. Source archives include
only tracked physical regular files, recursively omit `.git` and `.mmay`
directories, and neither source nor host archives contain symbolic links.

The host C compiler and platform SDK remain platform-owned under D030. An
installation pins one reviewed compiler identity at build time and uses that
path; neither a project nor `CC`, `PATH`, nor another environment variable may
select a replacement. D114 adds an explicit portable installed macOS policy
using the fixed system Apple toolchain shim; build-tree and other default
policies remain pinned. Runtime-producer provenance remains exact under both
policies. `mmay doctor` preflights the selected compiler and both
runtime profiles without generating C, linking, reading a project, or using a
network. Human output is a fixed ordered checklist. `mmay doctor --json` emits
one `marimay.doctor/1` object with toolchain version/host, selected compiler
identity and availability, per-profile runtime compatibility, installed-layout
classification, and an overall `ok` boolean. An unhealthy installation exits
1 after reporting every independent check; invocation or internal failures
retain the ordinary CLI exit classifications.

`mmay init [path]` is local-only and accepts exactly zero or one UTF-8 path;
the default is a missing `marimay-app` child of the current directory. The final
target must not exist. The command creates a complete sibling temporary
directory, writes canonical `mmay.mod`, `main.my`, and `main_test.my`, syncs
every file, and atomically renames the directory into place. Failure removes
the temporary tree and never modifies an existing destination. The module
identity is `local/NAME`, where `NAME` is the final nonempty path component
after replacing bytes outside ASCII letters, digits, `_`, `-`, and `.` with
`-`; an empty result is `app`. Templates have no network or third-party
dependency and must pass the installed formatter, checker, both test engines,
VM/native run, and native build unchanged.

`mmay run --engine native` performs the same checker, MIR optimization, and
entry selection as VM run, constructs the ordinary D050/D052 debug native
build request, reuses its content-addressed cache, and executes only the
returned artifact. It forwards validated arguments after `--` in order,
applies the exact heap and environment/filesystem grants embedded in the
build, supplies stdin bytes, and publishes the child stdout/stderr bytes on
their corresponding streams. Source or build failure cannot execute an older
artifact. A signal maps to the platform-derived nonzero child status; process
launch, capture, or bounded-stream failure is `I0002`/70.

Install, upgrade, and uninstall operate only on the selected prefix. A clean
install is staged then atomically published by the packaging driver. Upgrade
replaces only tool-owned versioned files and preserves unrelated prefix
content and every project outside the prefix. Uninstall consumes the installed
checksum manifest, removes only matching tool-owned files, refuses changed
files, and prunes only empty tool-owned directories. Read-only destinations,
interruption, collision with a non-Marimay `bin/mmay`, partial/stale runtime
profiles, and paths containing spaces or Unicode have explicit tests. PATH
editing is never automatic.

Reason: CLI0 needs a reproducible artifact that can be inspected, moved, and
removed without hidden host or project state. Keeping compiler selection,
templates, native execution, and ownership metadata inside one versioned
distribution contract prevents an apparently simple installer from becoming
a second package manager or an ambient trust boundary.

## D084 — Enum pattern qualifiers identify the matched nominal type

Status: accepted.

Enum patterns accept the existing qualified-identifier grammar in three
explicit forms: `Variant`, `Enum.Variant`, and `namespace.Enum.Variant`.
`Enum` may be a local enum, a named enum import (including a renamed import),
or a type alias. The explicit enum qualifier must denote the scrutinee's
nominal enum; a concrete alias must match its complete instantiated type.
An uninstantiated generic enum qualifier takes its type arguments from the
matched type, without changing inference elsewhere.

The existing `namespace.Variant` pattern remains a contextual shorthand: the
namespace must expose the matched enum or a public alias of its exact type.
Namespace lookup is file-local and does not fall back to another package or
a same-named local type. Private, unknown, wrong-type and excessively long
qualifiers are source errors, never internal checker failures.

These rules apply to unit, positional and named variants, including nested
enum payload patterns. A qualified name inside a payload is a constructor
pattern, not a binding. An ordinary unqualified payload binding remains a
binding. Qualification does not change exhaustiveness, redundancy, guards,
binding types, or execution behavior.

Reason: the grammar already admits qualified constructor names, and expression
construction already uses `Enum.Variant` and `namespace.Enum.Variant`, but the
checker implemented only `namespace.Variant` in patterns and returned an
internal failure for a third component. This decision resolves that missing
name-resolution contract without removing the existing shorthand or accepting
a different nominal type merely because it has a same-named variant.

## D086 — Strict type-directed JSON convenience

Status: accepted.

The notes application demonstrates that repeating a record's fields in hand-
written readers and writers is not an acceptable default experience for a
bounded CLI alpha. This decision supersedes D082's exclusion of compiler-known
JSON calls, not its cursor, resource, error, privacy, or encoding contracts.

`std/json` exposes compiler-checked `decode<T>(contents)` and `encode(value)`,
plus `decodeWithLimits<T>(contents, limits)` and
`encodeWithLimits(value, limits)`. The first pair uses `defaultLimits()`.
The result types are `Result<T, json.Error>` and
`Result<bytes.Bytes, json.Error>`. Decode requires one explicit type argument;
encode infers its type from its first argument and also permits one matching
explicit type argument. Import identity, named/namespace aliases, file-local
visibility, and lexical shadowing apply normally; spelling alone never grants
the intrinsic. The calls are not first-class function values in this slice.

The initial closed type set is `int`, `bool`, `string`, arrays of eligible
elements, and nominal records whose fields are all public and recursively
eligible. Concrete generic records and aliases preserve nominal identity and
substitute their field types. Record names become exact JSON keys in declaration
order. Every field is required, unknown and duplicate keys are rejected, and
`null` is not a value for any of these types. Empty arrays encode as `[]`.
No defaults, coercions, omitted fields, case folding, or private-field discovery
are implicit. Recursive type graphs, other scalar types, enums, `Option`, maps,
sets, interfaces, and functions require explicit codecs in this initial slice;
unsupported use is a source diagnostic, never a runtime fallback or panic.

Successful decoding consumes the entire input, builds fresh record and array
values, and publishes no partial value. Encoding preserves D082 declaration/
write order and all byte, string, number, nesting, item, UTF-8, surrogate, and
poisoned-output rules. Expected errors remain `json.Error`; allocation/heap
exhaustion remains `I0002`. The generated calls inherit the existing diagnostic
byte spans. The CP42 path extension adds `PathSegment.Field { name }` and
`PathSegment.Index { index }` and a `path: []PathSegment` payload to each named
`Error` case. `InvalidLimits` remains a root-only unit case. Paths run from
root to failure; missing, unknown and duplicate fields include the offending
field, value failures include their enclosing fields and zero-based indices,
and container grammar failures identify the container. Cursor errors are
relative to the current operation; automatic codecs prepend the enclosing
context. `atField`/`atIndex` provide the same explicit composition to custom
codecs without changing the error case, byte span or original path array.
This is an intentional pre-alpha payload migration: exhaustive named patterns
must bind or explicitly ignore `path`; callers using `message` need no change.
Error paths are constructed only on failure, not on every successful value.
`path` returns a fresh shallow copy, and `pathText` renders a bounded escaped
rooted display suitable for diagnostics. No runtime side table stores context.

The checker owns eligibility, field selection, and expansion into verified
ordinary typed MIR with ordinary calls to the shipped JSON cursor functions.
Neither backend discovers fields, parses JSON, or repairs source semantics.
No host JSON library, runtime reflection, dynamic schema/DOM, hidden build
script, or new JSON execution opcode is introduced. Generated control flow is
subject to the existing checker/MIR resource budgets. The original explicit
codec API remains available for private state and custom representations.

The compiler service publishes the four convenience signatures as intrinsic
metadata, not fabricated external functions, source declarations, or nominal
dot-call candidates. One descriptor registry supplies exact package/name
identity, generic-argument policy and value arity to import validation, call
selection and metadata publication. File-import completion retains every
visible named/namespace spelling, excludes type-only and absent imports, and
is bounded by the shared completion budget. It describes file import scope,
not lexical availability at an arbitrary cursor offset. Actual calls still
apply lexical shadowing through the checker. Queries borrow immutable facts
and reject stale service generations/snapshots. Generic signatures explicitly
state the closed eligible type set and direct-call-only limitation; no fake
definition location or first-class callable type is exposed.

## D087 — Local commands do not inherit the embedder's VM instruction fuel

Status: accepted.

Ordinary `mmay run` and `mmay test` impose no instruction-count or elapsed-time
cutoff, in either execution engine. A long but finite computation must not
fail merely because one compiled function executes ten million bytecode
instructions. A nonterminating local program or test requires interruption
by its caller; CI uses an explicit process timeout. This is not a claim that
local commands are a CPU sandbox.

This resolves a policy conflict discovered by the external configuration
utility: a valid 1,048,459-byte JSON document, below D082's limits, succeeded
in native execution but failed in the VM at exactly 10,000,000 instructions
in one frame. D049/D051 used the embedding API's default fuel without an
equivalent native restriction, contrary to execution §13.1. Neither changing
the JSON limit nor raising a magic instruction count defines a stable source
contract: instruction counts change with compilation and optimization.

The bytecode embedding API retains its bounded default of 10,000,000 steps
per frame. An explicit `max_steps == SIZE_MAX` selects unmetered per-frame
execution without incrementing or wrapping a counter. Any non-null shared
step budget is still consumed exactly, including through calls and cleanup;
zero shared fuel still fails before the next instruction, while a zero
per-frame maximum remains an invalid API argument. Finite per-frame budgets,
call-depth checks, verifier budgets, memory accounting, capability checks and
allocation-failure behavior are unchanged. Fuzzers and the MIR oracle retain
their existing bounded policies. The two CLI VM hosts select only the
unmetered per-frame policy; they do not disable verification or other limits.

The existing VM call-depth ceiling and native host-stack constraints remain
separate limits, not instruction fuel. This change does not establish their
cross-engine resource parity; that obligation remains in the pre-alpha
resource-boundary acceptance gate. A future user-selectable CPU sandbox must
specify its units, interruption/cleanup and both-engine behavior in a separate
decision, rather than silently changing ordinary local command semantics.

## D088 — Ordered collections use descriptor-selected compact storage

Status: accepted.

The shared D079 Map/Set runtime stores each insertion-ordered entry as an
aligned active/hash prefix followed by its key and, for a Map, its value.
The constructor validates the immutable descriptor before choosing this
layout. A raw scalar cell stores its full 64-bit representation; a reference
cell stores the complete managed reference. A Set stores no unused value.
Stride and cell widths are immutable constructor-derived metadata, not
caller-supplied layout IDs. C17 object-representation copies preserve bits
without unaligned or incompatible typed loads. Only reference cells trace.

The separately compiled storage owner controls checked size planning, exact
accounting, temporary roots, growth, compaction, publication and cleanup.
Growth retains existing entry/bucket capacities and load-factor policy;
compaction preserves insertion order. Replacement buffers publish only after
successful allocation and rebuilding, and every failure preserves logical
contents and mutation generation. The lookup memo is invalidated whenever
storage is rebuilt. Operations and probing keep their complete handle, key,
hash, collision and storage-integrity checks. SipHash-2-4 and both seeds,
public signatures, source key eligibility, MIR and bytecode are unchanged.

D055 budgets count the actual managed object, slot bookkeeping and reserved
compact storage, including constructor-derived layout metadata. A smaller
entry can therefore move a resource boundary; the language does not promise
the former oversized cell allocation. There is no internal-layout binary
compatibility claim. Exact-boundary, mixed-cell tracing, failure-index and
VM/native tests accompany the representation change. No source syntax or
formatter rule changes.

Reason: a raw pair and a Set element need not reserve two full reference-sized
cells. The selected implementation must keep representation decoding out of
expensive generic copy calls; memory savings do not excuse a measured hot-path
regression. This decision does not close the broader collection performance
or Tier 1 acceptance obligations.

## D089 — Independent cursors for linear ordered collection traversal

Status: accepted.

D079 source loops advance an independent scalar cursor per traversal. Zero
starts a traversal; a successful advance returns a positive one-based storage
position, or zero at exhaustion. Advance scans forward from the previous
position, skipping removed entries. Reading a cursor checks that it names an
active entry; Map reads its key/value and Set reads its key. Cursors contain
no address or managed reference, allocate nothing and add no collection state.
They are internal execution values, not source-visible indices or APIs.

The checker emits the generation barrier before every advance, including the
exhaustion test, and retains the existing iterable/header spans. Every nested
or interleaved loop has its own progress. Break exits immediately; continue
returns to the generation barrier and advances exactly once. Mutation, alias,
GC/rooting, insertion order, sparse removal/reinsertion and failure contracts
remain D079/D088. A cursor is not a proof capability: all runtime accesses
validate receiver identity/kind, range and active storage independently, even
for hostile verified bytecode. Reusing a cursor with another receiver cannot
bypass those checks; source lowering preserves its original receiver.

MIR and bytecode add NEXT, CURSOR_KEY and CURSOR_VALUE operations. Wire IDs
158–160 are appended; 151/152 retain their active-ordinal meaning. The oracle
implements the protocol over its independent ordered storage; VM and generated
C17 use the same checked, allocation-free runtime boundary. There is no raw
storage exposure, shared mutable iterator, weakening of hashing or implicit
change to the legacy operations. Source syntax and formatter rules do not change.

Reason: ordinal lookup from the start for each item makes a full traversal
quadratic. Per-loop forward progress visits each occupied or removed storage
slot at most once between mutations. Storage representation and D055 byte
accounting remain unchanged. Performance evidence must separate preparation
from traversal and include several sizes, end-to-end workloads, sparse/nested
correctness and the complete applicable verification tail.

Coordination note: simultaneous draft D089 proposals used incompatible -1 and
zero sentinels for IDs 158–160. The zero/positive cursor contract above is the
accepted resolution; the signed -1 draft is withdrawn before publication.

## D090 — Group metadata for the ordered collection index

Status: accepted.

The shared Map/Set index filters eight adjacent bucket positions using compact
hash metadata before reading insertion-ordered entries. Each occupied position
records seven high bits of the existing SipHash-2-4 result; distinct control
bytes mark empty and deleted positions. Seven mirrored bytes permit a bounded
group read across the wrap point. Portable C17 byte operations define the
byte-to-probe mapping independently of host endianness and SIMD availability.
An eight-position index uses scalar control-byte probing to avoid group-mask
overhead for the smallest tables; larger indices use group filtering.

The bounded linear placement order, first-deleted-position reuse, load factor,
capacities, full hash comparison and nominal/string equality remain unchanged.
Reference-key equality can reject a full-hash candidate and resume the same
walk. Group matching can yield conservative false candidates; it cannot prove
key equality. Every candidate entry access checks the index, storage and active
state before reading its hash or key. Checked public operations still resolve
the complete receiver and validate reference-valued inputs.

This refines D088's storage-integrity wording: index metadata is private,
storage-owner-maintained state, and lookup validates the positions it actually
inspects. Lookup is not a full-table corruption audit, including positions
excluded by metadata. Source and bytecode cannot supply a bucket, tag, pointer
or private progress record. No metadata match bypasses a managed handle or
candidate bounds check.

Control bytes share the existing bucket allocation and add no map fields,
managed roots or allocation calls. For a nonempty bucket reservation of N
positions, D055 additionally counts N + 7 bytes; an unallocated index uses
zero bytes. All size arithmetic is checked. Growth/compaction builds the new
index before publication; insertion, deletion and clear maintain both metadata
and its mirrored tail. Failure cleanup releases the complete allocation.
Allocation failure preserves contents, order and mutation generation.

D079 insertion order and hashing, D088 entry layout, D089 cursors, public APIs,
source syntax, MIR and bytecode retain their contracts. No formatter fixture
or new user-facing operation is needed. Correctness evidence includes an
independent scalar probe model, reference candidate resumption, all six cell
layouts, exact byte boundaries, every reachable allocation failure, GC and
VM/native behavior. Performance evidence includes whole original workloads
and small/large tables with hits, misses, updates and deletion/reinsertion.

Reason: ordered entries and the lookup index have different access patterns.
Compact metadata avoids loading most nonmatching entry payloads without
reordering entries or weakening the hash. This decision changes the index
representation; it does not close the wider CP42 performance or release gates.

## D091 — Amortized rebuilding of insertion-ordered storage

Status: accepted.

When the ordered entry reservation is exhausted, Map/Set compacts at the same
capacity only if at most half that reservation is live; otherwise it grows the
entry reservation geometrically. Bucket load-factor planning is independent.
Rebuilding preserves the order of all live entries and invalidates the lookup
memo before subsequent operations. D090 metadata is rebuilt with the buckets.
Removing the last live entry resets the append position and empty index in
place, retaining the reservation for allocation-free reuse.

This replaces D088's former rule of compacting an exhausted reservation after
any deletion. That rule could copy every entry and rebuild every bucket on
each delete/reinsert pair in a full table. The new policy reserves enough
append positions or reclaims enough holes to distribute rebuilding over many
mutations. At a fixed live cardinality, repeated replacement must not allocate
replacement buffers on every operation. This is an amortized storage policy,
not a worst-case constant-time or collision-complexity promise.

D055 counts actual reserved entry and index bytes. A sparse deletion may now
be followed by a larger reservation, moving an exact resource boundary; public
capacity is not exposed. Checked arithmetic, complete temporary rooting,
allocation-failure cleanup, failure-atomic insertion, generation and ordering
remain unchanged. Failure of a put after a successful remove does not undo
that earlier remove. No emergency in-place compaction or unaccounted fallback
may conceal a resource failure. D079 source operations, D089 cursors, MIR,
bytecode and formatter syntax retain their contracts.

Tests independently bound allocations across repeated replacements, verify
the rotating insertion order and all six raw/reference layouts, force GC,
exercise every reachable allocation-failure index and check the new exact
growth boundary. Public source regression tests execute in both engines.

## D092 — Void block arms and lexical match cleanup

Status: accepted.

A reachable match-arm block with no tail expression has type `void`, including
an empty block. All non-diverging arms still have one identical result type:
implicit void, an explicit void call and a diverging arm can coexist; a
value-producing arm cannot mix with void. A value context does not invent a
default value for a tail-less block. `return`, postfix `?`, break, continue and
panic keep their existing control-flow meaning.

This explicitly replaces D067's requirement that every continuing block arm
end in a tail expression. It allows ordinary side-effect/error-handling matches
such as `Ok(_) => {}` with an error arm returning from the enclosing function,
without a dummy boolean or a no-op helper. Arm punctuation, parsing,
exhaustiveness, bindings, guards and nominal patterns are unchanged.

D074 applies inside every block and to the complete match: omitting a tail
cannot silently discard a Result-valued expression statement. Void has no
stored payload; a named local of type void, inferred or annotated, is a source
type error. An explicit discard binding may consume void without storage.
Frontend checking must reject a named void binding rather than publishing MIR
that fails only during bytecode/native lowering.

Each selected block arm is an existing lexical cleanup scope under D041.
Its tail is evaluated before that arm's LIFO deferred calls; the result stays
live through cleanup, then control joins the surrounding expression. A
continuing tail-less block runs the same cleanup before joining. Return,
propagation and loop transfer perform their existing cleanup on their exit
path, without replay at the match join. Deferred registrations from one arm
must never leak into another arm or the enclosing block. This repairs an
existing D067/D041 lowering defect rather than changing defer timing.

One checker owner implements arm completion for all scrutinee families.
Backends consume the resulting typed MIR and add no source-semantic workaround.
Tests cover positive/negative typing, parser/formatter round trips, every
reachable checker allocation-failure index, branch-local cleanup and root
lifetime, and oracle/VM/native behavior. The external config app replaces its
dummy-bool workaround using the accepted spelling.

## D093 — Expectations in ordinary test-file helpers

Status: accepted.

The implemented `expect` surface is available in every ordinary function
defined in a file ending `_test.my`, including a generic helper specialization.
Eligibility follows the defining source file, not the caller's test annotation
or the current command's mode. A production-file function does not gain
expectations by being called from a test. Existing shadowing, argument typing,
constant-evaluation restrictions and staged expectation members are unchanged.

Helpers need no new annotation, context parameter or test-framework import.
Only `@test`/`@fuzz` declarations are discovered as test/fuzz entry points;
an unannotated helper is an ordinary call in its caller's execution and uses
that execution's managed state, limits and captured output. Helpers do not
create an isolate or weaken the fresh-state boundary between test entries.
The test-source inclusion and package-visibility rules remain unchanged.

Failure retains D080's `G4004`, bounded message and complete expectation-call
span in the helper's defining source. It fails the current test rather than
creating a separate result for the helper. MIR/oracle, VM and native use the
existing assertion/output instructions and panic/source ownership.

This replaces the implementation restriction to bodies directly annotated
`@test` or `@fuzz`, which forced ordinary `_test.my` helpers to reimplement
assertions with `if`/`panic`. The checker function-context owner derives the
permission from the definition AST before checking the body; runners and
backends add no caller-sensitive source rules. Focused tests cover helpers,
generic/cross-file calls, discovery, production rejection, failure spans,
allocation cleanup and public VM/native execution. The config application's
error-path helper adopts the built-in expectation reporting.

## D094 — Exact error context for Result mappers

Status: accepted.

For `Result<T,E>.mapError`, the known input error type E participates in the
existing nominal-first-parameter selection of an overloaded named mapper.
This applies to local, namespace-qualified and explicitly imported/renamed
functions. Exactly one visible unary function must be selected; its parameter
must be E. The result type does not introduce a new overload dimension.
Ordinary local shadowing wins over top-level functions and import namespaces.
Unsupported local function values do not fall back to an obscured named mapper.

When a mapError call is the direct operand of postfix `?`, an enclosing
`Result<U,F>` supplies F as its output-error context. Parentheses are transparent.
The receiver determines T independently of U. This lets a contextual mapper
closure omit its result annotation and lets an eligible generic enum mapper
use its concrete error context. The context applies to that direct call, not
to unrelated expressions or every nested mapper in the function. Existing
explicit lambda/result annotations continue to work; a mapper without enough
context remains a type error. There are no implicit error conversions.

Both mapError and postfix `?` require the exact prelude Result nominal family.
A user enum with two generic parameters and two variants is not Result, even
if it has the same payload shape. This enforces the existing D006 contract at
the source boundary rather than relying on a downstream enum-layout failure.

Receiver evaluation remains once, before branch selection. A mapper runs only
on Err; success payloads are preserved. The mapError owner supplies the narrow
direct-call context query and reuses the existing callable resolver. The try
owner passes that context through normal expression checking and retains its
ordinary propagation/cleanup path. MIR, VM and native add no inference rules.
Tests cover imported overloads/aliases, contextual closures/constructors,
different success types, parentheses, shadowing, nominal rejection, allocation
failure and observable execution; the config app adopts `mapError(cli.message)?`.

## D095 — Defining source ranges in test panic reports

Status: accepted.

A test runtime panic includes its defining source path and complete half-open
source range, rendered with D022 one-based Unicode-scalar line/column positions.
An expectation inside a generic or cross-file helper points to that helper,
not the test declaration. Source metadata comes from the same immutable
checked snapshots as typed MIR; reports never reopen possibly changed files
or infer language semantics in the backend. Existing code/message and test
exit classification remain unchanged. Output written to stderr before panic
is retained before the diagnostic in both engines.

The CLI prepares a canonical, deduplicated symbol/span-to-location table from
checked source facts and MIR spans. Native test compilation embeds that
read-only table; VM reporting consumes the same table. No runtime allocation
or source-file read is required for native lookup. Ordinary native builds and
low-level emitters without source metadata retain their existing output.
The table is optional metadata at the native boundary, not a new MIR opcode,
source-language construct, or permission to change observable execution.

The failure report uses `  --> path:line:column-endLine:endColumn` after the
existing panic line. Exact location bytes participate in the existing `both`
comparison. Low-level panic symbols, byte spans, call frames and suppressed
panics remain available; this change adds the primary source range to the
terse CLI report and does not claim full stack/suppressed-panic presentation.
Checks cover same/cross-file generic helpers, Unicode/space paths, CRLF/BOM,
range ends, earlier stderr, both engines, native O0/O2, and every reachable
allocation-failure index in location preparation and metadata emission.

## D096 — Scoped Option bindings in conditions

Status: accepted.

An `if` or `while` header may contain `const name = expression` or
`let name = expression` in place of its ordinary bool expression. The
initializer must have the exact prelude `Option<T>` type. On `Some`, the
body receives its exact T value; on `None`, execution selects `else` or exits
the loop. This is explicit binding syntax, not truthiness or conversion of
Result or user-defined enums. A named binding cannot store void.

The name is visible only inside the successful body, in that body's lexical
scope. It is absent from its own initializer, `else`, and following statements.
It may shadow an outer name; a declaration with the same name in the immediate
body is a duplicate. `const` is immutable and `let` permits reassignment, with
ordinary reference identity and aliasing. A fresh binding is made on each loop
entry. The initializer runs once per if test and once per while test, including
the test after `continue`. `break`, `return`, panic and defer retain their
existing control and cleanup rules. Definite initialization includes the
None/zero-iteration path; no nonempty or eventual-termination fact is inferred.

The first surface accepts one identifier and an inferred payload type. A type
annotation, discard, compound pattern or binding chain is not part of this
grammar. D072 remains authoritative: `const` and `let` are hard keywords,
including in conditions; they cannot name ordinary calls. General enum destructuring and exhaustive handling
remain with `match`; the existing exhaustiveness rules are unchanged.

Reason: real CLI code consumes optional lookups, destinations and popped
elements. A binding exposes their payload in the branch that can use it,
without a sentinel, repeated lookup or Result error suppression. The checker
lowers the exact Option test and guarded payload access into existing typed
MIR enum and CFG operations; neither backend adds source-language semantics.
The initial CP42 research packet's contextual-name assumption conflicted with
D072 and language section 2.3; that assumption is corrected, not implemented.

## D097 — Shallow array copy and failure-atomic stable removal

Status: accepted.

The compiler-known prelude adds ordinary receiver-selected functions:

```marimay
public fn copy<T>(values: []T): []T;
public fn removeAt<T>(values: []T, index: int): Option<T>;
```

Free and dot calls use the same exact receiver selection as D043/D053. Types
are inferred from the receiver; explicit type arguments on these calls are
rejected, as for the existing array operations. Receiver and index are evaluated
once, left to right. Ordinary same-named functions retain their resolution.

`copy` creates a distinct outer array, including for an empty source, preserving
length, order and exact element values. Managed elements retain their identity;
this is not a deep or immutable snapshot. Mutation of either outer array does
not mutate the other. The source and its iterator generation are unchanged;
the new array starts at generation zero, with D061 exact-length storage.
Allocation and configured resource failures retain D055 execution outcomes.
Construction roots the source through collection and publishes only a complete
copy, with no temporary element-sized staging array.

`removeAt` returns None without mutation for an index outside `[0, length)`.
A valid index returns the exact removed element in Some, shifts the following
elements left preserving their order, and mutates every alias of the receiver.
It does not shrink capacity. The vacated slot ceases to retain references.
The mutation increments generation once; None leaves it unchanged. Generation
exhaustion does not wrap and leaves the array unchanged. All possible result
allocation, validation and resource failures precede the mutation commit.
Normal indexed access keeps its G4002 bounds-panic contract.

Typed MIR has a fresh-array copy and an allocation-free checked stable-removal
primitive. The checker owns Option construction and evaluates its Some payload
before the removal primitive; backends do not reconstruct the source contract.
The primitive rejects an out-of-bounds index with G4002 even for hostile code.
Removal invalidates array bounds/iteration facts through aliases; copy does not
mutate its source. VM and native share the managed-storage operations.

Reason: notes requires one coherent model for producing a new outer collection
and editing it, without handwritten growth/copy/shift loops or partial failure.
Predicate search remains a separate callback contract; this decision introduces
neither general function values nor a search API.

## D098 — Exact function values through closure conversion

Status: accepted.

Function types `(A, B) => R` are exact, invariant value types. Ordinary visible
functions and explicitly annotated closures can produce these values; values
can be passed, returned, rebound and stored in managed containers. Invocation
evaluates the callee once, before arguments evaluated once from left to right.
The argument count, parameter types and result type are checked statically.
There is no function-to-interface source conversion, nullable callable or
user-defined implicit conversion. An expected exact function type can select
an existing nominal-first-parameter overload; result type alone cannot create
new overloads. Uninstantiated generic function references remain unsupported
until their separate specialization contract is implemented. External and unsafe
function references are excluded from this bounded value ABI, including imports.
A member call retains D003 dot-call resolution; invoking a function-valued record
field uses `(record.field)(arguments)`. This adds no field/method priority rule.

Closures retain the section 6.3 capture contract: const bindings by value
(retaining reference identity), let bindings through a shared managed cell,
and fresh iteration bindings. Capturing a local requires definite initialization
at closure creation. Each body has its own parameters, locals, return target,
loops and defer scope. There is no contextual annotation omission beyond D094.
Escaping a mutable capture remains legal, with the specified warning; warning
delivery is a distinct checker obligation, not a runtime failure or restriction.

Closure conversion represents a function value internally as a one-method
typed MIR interface whose payload is a private managed environment record.
Synthetic bodies receive that environment before source parameters. Captured
mutable bindings use private record cells; uninitialized declarations can use
an Option payload until their checked first assignment. This representation
reuses managed ownership, graph collection, typed calls and both backends.
It does not expose an interface type or synthetic record at the source level.
Synthetic type/body identity derives from the enclosing symbol and source
location, including generic instantiation identity. No runtime closure contains
a pointer into a C stack frame or a checker data structure.

Void callbacks retain a typed void interface call and discard its result through
the existing frame return path. Never-returning callbacks use an internal bool
result slot with no returning body edge and an unreachable-return guard at the
call site; their source type remains never. No new MIR opcode or bytecode wire
format is introduced. A closed-world interface call with no construction of its
receiver type is valid (for example, in an unused higher-order function). Every
actual construction and method signature is still checked, VM tables must be
owned by the program, and native dispatch retains its checked default failure.
Synthetic body source-origin metadata preserves precise test failure locations.

The initial storage plan conservatively boxes mutable names mentioned in nested
closures. Actual capture resolution remains lexical; name shadowing can cause
extra storage but cannot capture the wrong binding. Planning has a per-function
work bound using the configured MIR verification-work ceiling. This conservative
plan stays isolated in the cell owner; a future escape/capture analysis may
remove unnecessary cells without changing source behavior.

The specified nonfatal warning for an escaping mutable capture is not delivered
by this implementation. CP42 must add diagnostic severity, escape classification,
and positive/negative warning tests before claiming section 6.3 conformance or
closing the callback acceptance gate. Legal escaping closures must stay legal;
turning the warning into a compilation error is not an acceptable shortcut.

Exact capture layout and source semantics belong to focused checker owners.
MIR layout completion, verifier checks and the existing engine call boundary
remain authoritative; native/VM code does not infer source captures. Resource
and allocation failures retain D055 outcomes, with roots and teardown checked
at every reachable deterministic allocation failure.

Reason: CP42 predicate search needs reusable callable values with stable capture
lifetime. This establishes the common callable representation instead of a
predicate-specific native helper or repeated AST expansion at each invocation.
It does not add search, suspension or a public callback FFI ABI.

## D099 — Predicate array search with checked callback boundaries

Status: accepted.

The compiler-known prelude exposes `findIndex<T>(values: []T, predicate: (T) => bool): Option<int>`
with the existing exact receiver-directed free/dot selection. It evaluates the
receiver once, then the predicate expression once; the search starts after both
argument expressions complete. The predicate is a D098 function value with the
exact invariant signature `(T) => bool`, including annotated closures and eligible
named overloads. No special contextual annotation omission or explicit generic
argument list is added for this operation.

The result is the first matching zero-based index in Some, or None when no item
matches. Callback invocations follow array order, once per visited element, and
stop at the first true result. An empty array still evaluates both argument
expressions but never invokes the predicate. Reference elements retain identity.
The operation itself leaves the array, its generation and existing iterators unchanged.

The search captures length and mutation generation after evaluating its arguments.
It checks that generation before visiting an element and immediately after every
normal callback return, including true and the last element. A mutation of the
same array through an alias raises G4005 before an index or absence is published.
This deliberately strengthens the post-callback boundary compared with a source
for loop that immediately breaks or returns under D053. Failed/no-op mutations
and reserve retain their existing generation contracts; mutation of a referenced
child does not change the outer array's generation. A callback panic propagates
unchanged, with its own cleanup, rather than being replaced by the generation
check. Completed callback effects are not rolled back by later failure.

A focused checker owner lowers this contract to ordinary typed MIR control flow,
existing array generation checks and D098 interface calls. It neither expands the
callback AST at each visit nor introduces backend-specific predicates or wire
operations. Array bounds proofs remain subject to call invalidation. Managed
roots and D055 resource/allocation outcomes use the common engine mechanisms.

Reason: Notes ID lookup and removal need one shared, identity-preserving search
operation. This slice integrates the callable foundation without changing array
mutation semantics, adding identity equality, or hiding expected failures.

## D100 — Result error mapping consumes ordinary function values

Status: accepted.

`Result.mapError` uses the D098 callable representation and invocation boundary.
The receiver is evaluated once, followed by the mapper expression once, before
choosing the Result variant. A factory or other mapper expression therefore runs
on both Ok and Err; the resulting unary callback runs only on Err, exactly once.
The callable's parameter must be exactly E; its declared result determines F.
Stored, returned, indexed, field and conditional function values are accepted.
A `void` result constructs Err without a payload; a `never` callback has no normal
Err continuation. Normal successful values and referenced error payloads retain
their identity. Callback returns, postfix `?`, defers and panics have their own
function scope, as for every other D098 invocation.

D094's exact input-based named overload selection and direct-postfix-`?` output
context remain. Only a direct expression-bodied mapper closure may omit parameter
and result annotations; parenthesized forms are transparent. Annotated block
closures are ordinary D098 values. A fully annotated or stored callback needs no
contextual output type. Taking generic/unsafe/extern function values remains
outside D098; mapError must not bypass that restriction. Existing unary enum
constructor mappers remain a compiler-known constructor form with their existing
exact generic context; this does not make constructors first-class functions.

The checker mapper owner prepares one callable or constructor packet; the Result
owner handles variant control flow. Closure creation, capture initialization and
managed allocation apply on Ok as well as Err. The former inline contextual body
is removed, so control flow and captured bindings cannot leak into the caller.
No wire operation, backend-specific mapper or runtime allocation API is added.

Reason: config/Notes error adapters should compose with the same callbacks as
array search and ordinary code, instead of maintaining an AST-only exception.

The initial implementation reported I0001 for a callback returning `never`.
D102 removes that guard with common enum metadata, impossible-edge checking,
wire/VM/native and deterministic allocation-failure coverage. This was an
implementation gap, not a source-language restriction.

The differential fixture also requires correction of native root coalescing:
MIR registers are not SSA values. An activation-wide replacement root requires
single assignment outside repeated CFG regions. Mutable container children must
retain their own roots because mutation may occur through an alias or callee.
This follows D063's existing lifetime contract without adding source semantics.

## D101 — Bounded file-line reading shares the stdin line contract

Status: accepted.

`std/fs.readLine(file: File, limit: int): Result<Option<string>, Error>` reads
one UTF-8 line from the current shared handle position. LF and CRLF are removed;
a lone CR and embedded NUL remain data. EOF before any data yields Ok(None);
EOF after data yields Ok(Some(data)). An empty terminated line is Some("").
The payload-byte limit is 0..1048576, independent of total file size. Invalid
limits yield InvalidLimit, overlong lines LimitExceeded, and malformed UTF-8
InvalidUtf8 with the first malformed byte offset relative to this line.
Read/limit/UTF-8 errors publish no partial string; consumed input is not rolled
back, so applications must not resume as if an errored line were complete.

File aliases share one cursor and one bounded read-ahead buffer. Mixing `read`
and `readLine` consumes that same position without dropping prefetched bytes.
Open authorization, symlink policy and close/best-effort teardown remain D077.
The File wrapper checks its closed state before invoking a read intrinsic, as
for existing read. Runtime invalid-limit checks precede raw handle lookup.

A common runtime line parser owns terminator normalization, UTF-8 validation,
line growth and the no-partial-output boundary for stdin and files. It grows
storage with the actual line under the existing limit rather than allocating
that entire limit for each short line. File handle ownership and buffered reads
have a separate owner behind a narrow API; path/capability code does not expose
its private per-handle state. Allocation/resource failures retain D055 classes.
MIR and hostile bytecode validate the exact nested Result/Option/string layout;
both engines consume the same runtime contract.

Reason: logstats, grep and frequency need one bounded line surface on both stdin
and files, including total inputs larger than a whole-file helper's limit.
This adds neither asynchronous I/O nor an unbounded whole-file convenience API.

The streaming consumers also expose a D063 implementation defect: a native
record view must follow the current managed reference when a MIR register is
redefined in a loop. Immutable child fields are immutable per object, not per
register. Unstable parent definitions refresh child-array call hints. These
repairs preserve existing source semantics; JSON expansion remains ordinary
MIR and receives no backend-specific workaround.

## D102 — Enum transport preserves bottom payloads

Status: accepted.

A direct `never` payload makes its concrete enum variant uninhabited. Generic
substitution precedes this check. Other variants remain usable, including
`Result<T, never>.Ok`, `Result<never, E>.Err` and `Option<never>.None`. The
bottom payload is not erased to a unit or void payload. Constructor argument
evaluation keeps its ordinary left-to-right divergence and cleanup behavior;
no expression can supply a normally completing value of type never.

Exhaustiveness excludes variants with a direct never payload; explicit patterns
for them report the existing unreachable-pattern T0014. An empty match over an
enum whose every variant has such a payload has type never. This establishes
the direct-bottom rule; it does not add general recursive inhabitation proofs,
value-dependent reachability, variance, implicit conversions or new syntax.
Postfix `?` still requires the exact same E. For E=never its error edge cannot
continue, while a never success payload makes its success edge diverge.

MIR retains variant ordinals and exact payload types. The existing bytecode
NEVER type tag is admitted only as enum-layout metadata in this transport;
registers, constructor arguments and record fields do not gain never values.
MIR also retains source parameter declarations of type never, which cannot be
supplied a normally completing source argument. Its verifier rejects bottom
payload constructions even if hostile MIR names such a parameter. Existing
function-result bottom handling remains unchanged. The private wire format
needs no new opcode, layout field or version; public .myb remains staged.

The checker owns impossible variant classification and no-continuation edges.
Backend layout descriptors retain a non-reference placeholder for an impossible
payload; verified code can never construct or read its value. Normal variants
use the existing allocation, roots, nominal identity and runtime transport.
No backend reconstructs source capture, type-checking or exhaustiveness rules.

Reason: D100 mapError must accept never-returning callbacks on the same enum
transport as ordinary user code. A mapper-specific approximation would leave
storage, matching, generic calls and propagation broken. Focused checker,
malformed MIR/bytecode, wire round-trip, both-engine, forced-GC and every
reachable deterministic allocation-failure index cover this common boundary.

## D103 — Nonfatal mutable-capture escape diagnostics

Status: accepted.

D098's capture warning is T0020 with severity `warning`. It is emitted once
per closure source span when an inferred retaining scope is shallower than
the declaring block of any captured `let`. Generic instantiations share that
source diagnostic. Warnings do not prevent publication of checked functions,
checking later files/packages, execution, test discovery or native artifacts.
Errors retain their existing exit and publication rules. Diagnostic limits
count both severities; allocation/cancellation/resource failures are not hidden.

The initial analysis is conservative and flow-insensitive within each typed
function body. Copies unify identities; assignments and CFG joins accumulate
possible aliases. Record fields and enum variant payload positions remain
separate. Array elements share a slot; map keys and values have separate slots.
Contained values inherit the shallowest possible retaining scope of their
container. Returning a closure/container, storing it into an outer binding or
storage owned outside the function, or passing it as an ordinary argument may
retain it. Returning an unrelated field alone does not retain sibling fields.
Removal, overwrite and branch conditions do not retract accumulated facts.

Ordinary calls have no interprocedural no-retention summaries in this analysis:
arguments can be retained, and results can alias storage retained by the callee.
Calling a function value does not itself retain that callable, including direct
findIndex/mapError callbacks. Values captured inside its environment may be
exposed by the called body; this is conservative for nested captured callables.
A warning therefore describes a possible escape, not proof of a runtime escape.
No warning for a const-only capture is implied by reference mutability: const
reference capture keeps identity under the existing D098 contract.

A checker owner records resolved lexical facts while locals exist, analyzes
verified typed MIR before publication, and discards its graph on all exits.
Scope propagation is monotone over a finite graph with a max_verify_work
budget, iterative alias merging and cycle-safe worklists. The analysis neither
changes source acceptance nor chooses boxing, GC roots or runtime lifetimes.
Backends do not reconstruct capture semantics. More precise no-retention
summaries and strong updates are future refinements, not reasons to block legal
programs or waive the warning tests. This closes the D098 diagnostic removal
gate locally; CP42 and Tier 1 acceptance still require their own evidence.

## D104 — Nonallocating VM execution regions retain validated array views

Status: accepted.

After complete bytecode verification and argument validation, the program VM
may execute supported scalar, control-flow, raw-array, string-array read and
string-equality instructions in a
separate nonallocating loop. It consumes the original instructions in order;
there are no new wire opcodes, source rewrites or workload-specific kernels.
D035/D062 arithmetic, D053 mutation generations, D058 bounds capabilities,
D055 roots and D087 per-frame/shared instruction fuel retain their contracts.

Within one such region, a bounded activation-local view cache may retain an
exact validated array identity and immutable descriptor. It uses the existing
D063 runtime view ABI: object-field locations remain valid across storage
growth, and no element-storage address is retained. The region neither
allocates nor collects, calls, suspends, performs I/O or invokes host code.
All views expire before the ordinary dispatcher handles any unsupported
instruction or potentially observable failure. Register rebinding cannot
cause collection inside the region; cache lookup still compares the complete
managed identity and expected layout. Lookup may evaluate all initialized
identity fields together; equality requires matching type, layout, heap
provenance, slot and generation, regardless of comparison order.

A separately owned, bounded string-byte cache may reuse complete checked
string references only within that same nonallocating region. String-array
reads require the exact parent descriptor and original checked/proven index,
then validate the selected string before publishing its register. Byte equality
validates both operands before publishing its boolean and preserves embedded
zero bytes and empty strings. All string views expire with the region; calls,
allocation, collection and ordinary dispatch cannot observe or reuse them.

Descriptor preparation remains lazy in the existing owner. The region can
only borrow an already complete descriptor; a miss returns to the ordinary
dispatcher without consuming the instruction. Likewise an arithmetic, bounds,
reference, mutation-generation or fuel failure is left uncommitted and is
handled at the same instruction by the ordinary dispatcher. Successful
instructions consume exactly one unit of applicable frame/shared fuel. Failed
speculation must not allocate, mutate a register/array, advance the instruction
or consume fuel. Panic construction, spans, cleanup and failure classification
therefore stay in their existing owners. Unsupported types/layouts use the
ordinary dispatcher without weakened validation.

Reason: the measured VM repeatedly performs descriptor/type/handle work and
crosses generic instruction-owner boundaries inside allocation-free loops.
A scoped execution region removes that repeated work while retaining one
verified code stream and the checked failure path. This does not promise
native throughput or close the measured parity objective. Required coverage
includes reference rebinding, GC/call boundaries, scalar/layout fallbacks,
exact finite/shared fuel, overflow/bounds/iterator failures and all-index
allocation sweeps, plus the unchanged full benchmark inputs.

## D105 — Program-local reuse of inactive VM activation storage

Status: accepted.

The program VM may retain inactive register, defer and root-location storage
in its explicit frame stack until that program invocation finishes. The pool
is indexed by call depth and stores at most one function layout per slot,
bounded by the existing maximum call depth. Originally a different function at
that depth released the previous storage before creating its own; D110 extends
this policy to reuse capacity while replacing the active layout. There is no process-
global cache, retained program, or reuse across independent invocations.

Returning first publishes the result to the parent's rooted destination, then
unlinks the callee's root frame. Inactive register storage is never a GC root.
Before reactivating the same function, every managed location is reset to the
invalid reference, arguments are installed, and the ordinary checked root
registration validates and links the frame. Definitely-assigned verified
scalar registers may reuse their initialized type metadata; no prior scalar
value can be read before its new definition. Program verification, register
identity, argument/result semantics, call-depth and instruction limits,
closure/interface dispatch, panic spans and defer ordering are unchanged.

The activation owner retains stable separately allocated root-frame addresses
across frame-stack relocation. Setup failure releases every partial allocation;
normal and failing teardown release both active and inactive slots. D055's
managed heap still counts the same managed graph; host activation buffers have
the bounded invocation lifetime above and are not relabeled as managed objects.
All deterministic allocation-failure indices, both zero allocator counters,
forced collection, same/different-layout reuse and recursive stack growth must
be tested. Reduced allocation counts are an implementation consequence, not a
promise of a stable host allocation sequence.

Reason: after D104 removes repeated array work, the measured DSU profile spends
about 40% of sampled stacks constructing/registering and destroying activation
storage. Reusing a verified inactive layout avoids repeated metadata walks and
host allocations without retaining its old managed values.

## D106 — Transactional scalar and control-flow simplification of typed MIR

Status: accepted.

A function-scoped preparation pass may simplify verified typed MIR before
the shared VM/native lowering boundary. Its initial scope is nonallocating
scalar literals, local scalar copies and known scalar results, dead nontrapping
scalar definitions, and redundant CFG edges/blocks. It is not a new source
evaluation rule or a general SSA, alias, inlining or speculative-load pass.

Analysis stays separate from transformation. The pass uses bounded backward
liveness over the actual CFG and includes registers read by call/construction
metadata and implicit deferred cleanup. Scalar constants with exactly one
definition, excluding parameters, may be initialized at function entry; their
identity, type and bits are unchanged. Local copy/value facts are invalidated
on redefinition. Managed copies/allocations, host effects, checks and any
potentially trapping operation remain, even when their result is dead. A
checked scalar operation folds only when the existing exact arithmetic owner
proves its concrete operands succeed. No floating-point arithmetic is folded.

Rewrites preserve source spans for retained effects and checks. Proof-carrying
array/index/generation operands are not independently substituted from local
copy facts. The pass does not hoist memory reads, checks, allocation or calls,
change evaluation order of effects, or assume that two nominal values alias.
Empty-jump threading and unreachable-block removal preserve reachable paths
and divergent cycles. Subsequent verification reconstructs all required MIR
proofs; the existing range preparation follows the scalar/control pass.

Only block/instruction storage is privately cloned; immutable type, layout,
construction and call metadata remain borrowed during preparation. New code
is published only after successful transformation and verification. Allocation
or work-budget failure disposes all temporary state and leaves the input MIR
unchanged under the existing checked-MIR resource fallback. Invalid input or
an invalid transformed result is a compiler failure, not an optimization hint.
Every allocation index must be swept with both allocator counters zero.

D087 fuel counts the instructions of the verified executable bytecode supplied
to the VM. Source compilation does not promise stable instruction counts across
optimization revisions. This pass changes prepared source code; it does not
rewrite a caller-supplied bytecode program to evade its exact fuel contract.

Reason: D104/D105 measurements retain billions of scalar/control instructions
in fixed workloads. Simplifying their common typed representation addresses
generated instruction demand before adding further interpreter specialization.
The method uses non-SSA liveness suited to today's mutable-register MIR,
following Vladimirov printed 71–74 / PDF 72–75; no SSA-only proof is presumed.
Speedup, startup cost and parity remain measured obligations, not predictions.

## D107 — Do not append a general record cache to the scalar/array hot loop

Status: rejected.

A bounded record-view cache was implemented with exact managed identity,
layout, mutability, failure and fuel handling, then measured in alternating
full-size runs. It made `record-reference-copy` 3.4%, `record-create` 2.0%,
`catalog-interface` 1.5% and Dijkstra 1.1% slower at the median. An earlier
combined implementation also changed the compiler's inlining of the established
scalar/array runner and regressed unrelated algorithms.

The general record region and its tests were therefore removed before commit.
Record-heavy shapes may use a separate, completely matched execution-image
runner when that runner wins a paired workload and retains exact side exits.
The established D104 scalar/array translation unit remains independent so a
new representation cannot change its machine-code shape accidentally.

## D108 — Verified execution images own prepared VM dispatch

Status: accepted.

After complete program verification, one VM invocation may build a transient
execution image from the verified bytecode. The image is an internal execution
form, never a wire format or an additional source of language semantics. It
flattens each function into fixed-size operations, resolves block targets and
direct callees once, and retains the original block/instruction coordinates for
every side exit and diagnostic. An unrepresentable program uses the ordinary
verified dispatcher. Failure of the image's single central-allocator arena is
reported as the same deterministic allocation failure and cannot be hidden by
an allocation-dependent fallback.

The image may mark a superinstruction only from opcode, operand, layout and CFG
relations already established by the verifier. Current array-shift and
record-backed array-follow/compression loops are recognized by their complete
generic topology; package names, symbols, source paths and benchmark identity
are unavailable to the matcher. A composed runner commits registers and heap
effects in bytecode order, accounts for the exact number of original D087
steps, and exits at the first uncommitted original instruction when fuel or an
operation fails. The ordinary dispatcher retains panic construction, spans,
classification and cleanup.

Nested call-free functions containing only nonallocating scalar, control,
record and non-growing array operations may borrow the caller's linked managed
roots. Their arguments remain live in that caller for the complete nested
activation, and no reachable instruction may allocate or collect. Entry frames
always establish their own roots. A function containing any call, allocation,
host operation, collection mutation that can grow, string construction or
unknown opcode is ineligible. Cached record/array views are non-owning, compare
the complete heap provenance/slot/generation before reuse, and remain valid
across storage growth only through the runtime view's stable object fields.

This tier remains optional on every host and does not weaken D019: unsupported
shapes use the interpreter, and acceptable startup cannot depend on executable
code generation. Required evidence includes direct binding and eligibility
tests, exact per-frame/shared-fuel comparisons at every composed boundary,
managed alias/rebinding and forced-collection cases, every reachable allocation
failure with zero live allocations/bytes, sanitizers, unchanged workload
checksums and paired before/after measurements. Interpreter specialization alone
is not performance parity.

## D109 — Batch complete verified nonallocating trace iterations

Status: accepted.

An execution-image family runner may retain scalar registers, a stable raw
array-storage view and its mutation generation in C locals across multiple
complete iterations of one verified nonallocating trace. Eligibility requires
the complete opcode, operand, definition/use and CFG relations for the trace;
every loop input retained as an invariant must be distinct from every register
written by the trace and from the induction register, and retained definitions
must be pairwise distinct. Matchers cannot inspect source paths, package or
function names, symbols, constants that identify a
workload, or benchmark metadata.

One batched iteration must first prove that its complete original instruction
sequence can succeed and that exact D087 frame/shared fuel is available. It
then publishes heap writes in the original order, accounts for every original
instruction and updates the retained scalar state. Register values and the
mutation generation may be materialized after the final completed iteration.
Before insufficient fuel, loop exit, comparison exit, bounds failure,
generation saturation or any other side exit, the runner publishes exactly the
last completed iteration and leaves the current iteration at its first
uncommitted original instruction for the ordinary checked path. A failed or
partial speculative iteration cannot mutate heap/register state or consume
fuel.

The retained storage address is permitted only when every instruction in the
batch is proven nonallocating, non-growing, noncollecting and call/host free.
Raw element access uses portable byte copies rather than an alignment
assumption. Unsupported storage, alias relations or trace shapes use D108's
ordinary composed runner. Required coverage compares cold and batched results
at every finite frame/shared-fuel boundary and at condition, bounds and
generation side exits, including final heap contents, mutation generation,
result, trap/panic span and allocator teardown.

Reason: D108 reduced `insertion` from about 6.06 seconds to 0.97 seconds but a
CPU sample still placed almost all steady-state time in per-instruction
materialization inside the proved array-shift loop. Retaining state across
complete iterations removes that representation cost without adding a source
rewrite, workload kernel or executable-code dependency. The measured D109
candidate reduces the paired same-base median from 980.44 to 234.36
milliseconds; VM/Bun parity and other trace families remain open.

## D110 — Reuse inactive VM buffer capacity across verified function layouts

Status: accepted.

D105's invocation-local activation storage may retain register, defer and
root-location buffer capacity when a different verified function enters the
same inactive depth slot. The slot still has exactly one current function
layout. Each buffer grows only when the current function requires more space;
its retained capacity is bounded by the largest corresponding verified layout
encountered at that depth. There is no additional callee cache or executable
code, and no architecture-specific path.

A layout change invalidates the old root-location map before register storage
can move. It initializes every active register's type and invalidates every
active managed reference before installing arguments. An owning activation
rebuilds its complete root-location map and uses checked root registration.
Unused capacity is never a root. D108's proven borrowing eligibility is
unchanged; an inactive retained root object is not linked merely because it
exists. Same-function reuse may retain its already prepared metadata under
D105's definite-assignment contract.

Root-frame addresses remain stable across frame-vector and register-buffer
growth. Growth failure leaves prior buffers owned by the frame; teardown frees
all partial, active and inactive storage using allocation capacities rather
than current layout lengths. Repeated calls within established capacities
require no host allocations. Allocation failure remains observable whenever a
required allocation fails; the host allocation sequence is not a language
contract. Managed-heap accounting, logical call depth, exact per-frame/shared
fuel, argument and return checks, panic locations and defer order are unchanged.

Validation covers alternating scalar/managed layouts with different reference
positions, growth and shrinkage, stale trailing references after collection,
borrowed-to-owning transitions, stack relocation, every deterministic allocation
failure through successful teardown and both zero allocator counters. Existing
VM/native differential and finite-fuel tests remain required.

Reason: D105 removes repeated allocation for one callee, but alternating callees
still rebuild their buffers from fresh allocations even at constant call depth.
Retaining capacity removes that allocator traffic in portable C17 without
retaining the values of a completed activation.

## D111 — Optional shared machine-code tier for verified VM segments

Status: accepted.

The typed bytecode VM may execute verified nonallocating segments through a
shared C machine-code backend. The portable interpreter remains required and
is the reference/fallback engine. Machine code is invocation-local, immutable
and absent from portable bytecode bundles. Unsupported instructions, platforms,
executable-memory policy or bounded code eligibility retain ordinary execution;
actual allocation failures remain observable. This decision does not enable the
experimental tier by default or establish release/platform acceptance.

The first adapter uses pinned SLJIT C sources. Enabling it requires an explicit
build option and locally supplied files matching the committed source manifest.
Preparation verifies a copy of each named input, applies only a recorded patch,
installs the owned allocator configuration and preserves the upstream license.
No build-time network retrieval is implicit. The external backend has its own
compilation target; Marimay-owned compiler/runtime code remains C17 with the
ordinary warning, architecture and sanitizer gates.

Compiler arenas and staging use the existing central allocator. The central
allocation boundary additionally owns OS code pages in `base/code_memory.c`;
only that owner may reserve/release them. Page rounding counts against the
invocation's 8 MiB executable-code budget. Relocation targets the final mapping,
while emission writes non-executable staging. Publication is one-way and uses
platform W^X and instruction-cache synchronization. A denied executable mapping
never triggers a generic RWX fallback. Code-page and ordinary allocator ledgers
must both reach zero on every tested success/failure teardown.

D087 frame/shared instruction fuel remains exact for supplied bytecode. A
compiled segment reserves its full original-instruction cost and refunds the
unexecuted suffix before a side exit. Only an invocation with unlimited frame
steps and no shared-counter pointer may omit that accounting. A SIZE_MAX-valued
shared counter is still observable. Unmetered code cannot execute when later
presented with finite/shared options. D109 composed loops retain their existing
source-prefix and fuel contract inside this tier.

Each side exit restores the original source PC before ordinary execution
handles the uncommitted instruction, trap or call. Calls retain existing
logical frames, call-depth limits, typed roots, argument/return checks and
cleanup order. Borrowed array/record views lose their validity at a return to
the VM, allocation, collection or host/call boundary. A managed register write may
retain its cached view only when the complete reference identity is unchanged.
Representation projections use authoritative verified operands; a compact
image field cannot silently truncate a signed or wide selector.

Cached view metadata may be retained without dereferencing its object pointers.
A subsequent native entry may revalidate it using exact execution identity,
function/layout and value type, the complete managed reference and heap
provenance, and the runtime's unchanged view-invalidation epoch. That epoch
advances before collection can trace or sweep, including failed collection;
UINT64_MAX saturation permanently disables this reuse for the heap lifetime.
Fresh heap provenance distinguishes disposal/reinitialization even at the same
address and epoch. Array views refer to the nonmoving object's live storage,
length and generation fields, not a copied storage buffer. A failed proof uses
the existing full descriptor/reference binding owner. This makes the spec's
revalidation allowance explicit; it does not let an expired borrow remain usable.

Within an uninterrupted native segment, exact-type/layout register copies may
reuse the checked view of a canonical register that still contains the same
complete managed reference. Every original reference value remains materialized
for roots and side exits. A write to the canonical register invalidates dependent
relations; every independently enterable segment and native call/return boundary
resets them. This does not remove index/generation checks, extend view lifetime
or allow a stale descriptor/reference proof to be used.

Repeated reads of the same record field may reuse a still-materialized value
of the exact result type/layout within that segment. This requires unchanged
receiver identity and no intervening record-field mutation. Writing either the
receiver or saved result invalidates the corresponding expression; calls, cold
record stores and independent entries reset the analysis. Reuse still consumes
the original instruction's fuel and preserves its materialized destination.

Reference-array reads copy both words of the managed identity after parent and
index validation; string conversion and reference stores retain their existing
checked owners. String-array reads may remain in a segment when the checked
child conversion precedes destination publication. Immutable byte views and
equality obey the same complete reference, execution/function/layout and GC-epoch
proof as other retained views. Empty and embedded-zero strings keep byte-wise
equality; a failed conversion resumes the unchanged original destination/PC/fuel.
Generated Native may project a string child directly from an exact, validated,
rooted array using D063's existing child-edge proof, with no intervening
safepoint. Host references still require checked string access.
The successfully published result of a managed string constructor proves the
same live string identity until the next safepoint, so generated Native may
project its immutable bytes directly. The constructor retains all allocation,
heap-budget, error and publication behavior.
A reserved scalar push may enter the runtime view-mutation owner
inside a compiled segment after a spare-capacity guard proves that growth and
collection are unreachable. The runtime retains length/generation limits,
resource diagnostics and mutation commit order. A full array or rejected guard
exits before the original push, with exact fuel and no committed mutation.

An immediate enum constructor may also remain in a segment when its complete
nominal descriptor and runtime descriptor ID already exist. The tier must prove
the runtime's nonallocating representation before calling the shared validation
and construction owner. Missing descriptors/IDs and other payload shapes exit
before the original constructor; registration and its allocation failures remain
on the ordinary path. Cached descriptor metadata must match execution, function,
layout and fresh heap provenance. It owns no managed value and cannot preserve a
descriptor across execution teardown. Success publishes the complete result only
after validation, retains resource-failure clearing and consumes the original
instruction's fuel; neither allocation nor collection is permitted in this path.

The tier may project immutable direct-call metadata and prepare register/root
initialization layouts once per selected function. Generated transitions may
refer directly to their original verified instruction while that program is
alive. The shared activation owner still performs depth checks, argument and
return handling, checked root registration and failure publication. Prepared
layouts hold only types, invalid references and register indexes, never live
objects or extra cached activations. They are bounded by selected verified
function layouts, allocated through the central allocator and released with
the invocation on success or any preparation/execution failure.

Function-specific view tables may use a bounded 2 MiB host-cache arena to avoid
cross-function cache collisions. Excess functions retain the shared checked
table. The emitter may inline the existing retained-view predicate, including
all execution/function/layout/type, reference/provenance and heap-epoch checks;
its miss still enters the checked binding owner. Entry-epoch wrap clears every
table. No cached view is a GC root or an additional logical activation.

In the unlimited-frame/no-shared-counter mode, bounded direct callees of selected
functions may also be compiled. A single native trampoline may transfer between
their labels through the shared logical call/return owner. Every Marimay call
still creates its logical activation and enforces the original depth limit,
argument/return rules and root lifetime. Frame-vector relocation cannot leave
stale pointers in the native context. Cold exits publish the actual callee and
its next uncommitted instruction; an already executed callee prefix is never
replayed. Finite/shared runs retain ordinary call/return dispatch and its exact
accounting. The invocation context is prepared once, not reconstructed at each
native entry.

Focused coverage must include operations and source exits, finite/shared fuel,
managed alias/rebinding and collection, code-policy/budget fallback, all
reachable allocation/platform failure indices and teardown. VM/native
conformance, Linux LSan and every claimed Tier 1 platform remain release gates.
The opt-in local build is not equivalent to completing those gates.

Reason: the reused ARM64 prototype with a shared SLJIT adapter reduces the full
32-workload paired VM median sum by 46.1%; invocation-selected fuel additionally
reduces Dijkstra by 15.3% in a separate six-workload series. These results justify
a shared execution tier while leaving ordinary-call, preparation and native
runtime costs open. They do not justify workload rewrites, weaker limits or a
CPU-specific semantic implementation.

Checked JIT array indexing may use one unsigned comparison of the i64 index
against current array length. The runtime publication/mutation invariant bounds
length by `min(SIZE_MAX, INT64_MAX)`, so every negative index converts above the
largest valid length. Reference/view validation precedes the comparison; failure
restores the original PC and unspent fuel before ordinary bounds diagnostics.
No index check is suppressed and no array-storage access precedes success.

## D112 — Publish the learning-project source with privacy-edited history

Status: accepted (repository-owner direction, 2026-11-08).

Marimay is presented publicly as a personal hobby and learning project for
exploring programming-language implementation and writing small programs and
algorithms. It makes no competitive or production-readiness claim. Original
project code and documentation are published under MIT; third-party terms remain.

The owner authorizes publication of the committed main lineage at the public
GitHub repository, after removing private contact metadata, paths, and
infrastructure references across history. Parent relationships, order, dates,
and development changes are retained; object IDs change. The unedited local
repository and unfinished working changes remain local. Historical evidence is
marked as privacy-edited and is not fresh acceptance evidence.

This public source repository is distinct from distributing a supported
toolchain release. N0, P0, and all feature/platform gates stay open. No domain,
trademark, package registry, signing identity, or PATH coexistence clearance is
claimed. See [publication policy](../security/publication.md).

No source-language, runtime, or command semantics change.

## D114 — Publish a bounded experimental macOS alpha

Status: accepted (repository-owner direction, 2026-11-08).

The owner authorizes the first GitHub prerelease `v0.1.0-alpha.1` as an
experimental macOS preview. Compiler, runtime and shipped standard-library
versions remain `0.1.0-alpha.1`; the channel is `experimental-alpha`, the claim
is `local-checker-backed-slice`, and `publicationReady` remains `false`.
This field records completion of the broader CLI0/CP40 readiness audit, not
whether the owner has authorized sharing an experimental artifact. This
pre-publication channel addition preserves the `marimay.version/2` field
shape and available/staged command inventory. `cli-alpha`, N0, CLI0, CP39,
CP40, CP41, CP42 and all other product gates retain their existing requirements
and states. No stable-language, production, signing, notarization, trademark,
registry, namespace or performance-parity claim follows.

The first binary distribution covers macOS 15 or newer on Apple Silicon and
Intel, as distinct ARM64 and x86_64 packages. Apple Command Line Tools are
required for native compilation. An explicitly enabled portable installed
macOS policy uses only
`/usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin LC_ALL=C /usr/bin/clang`
for compiler probes and compilation, with the package architecture and macOS
15.0 deployment target explicit. It accepts only AppleClang 16 or newer after
bounded numeric identity and target validation. Project inputs, `CC`, user
`PATH`, SDK override variables and other inherited environment do not choose
the compiler. The system's configured Apple developer tools remain
platform-owned. Missing, foreign, malformed, too-old, failing or incompatible
tools produce the existing bounded toolchain diagnostics without fallback.

The portable policy applies only to an installed tool built with that explicit
policy. Default/build-tree, Linux and Windows compiler selection remain
pinned. Shipped runtime archives and manifests continue to validate their exact
producer identity, version, profile and embedded markers. The selected local
compiler's actual version/build, target and selection policy are distinct
from this producer provenance and enter native cache identity and doctor
reporting. A changed local toolchain cannot reuse an old compiler identity's
cache entry. Compiler/SDK compatibility is bounded by the tested policy;
version acceptance alone does not establish every SDK ABI property.

Each host ships a complete relocatable `.tar.gz` prefix and a standard macOS
`.pkg`, with SHA-256 siblings, exact source revision, provenance and SBOM.
The unsigned, unnotarized package installs its candidate prefix at
`/usr/local/lib/marimay/V` and activates `/usr/local/bin/mmay` through an
owned POSIX wrapper that executes the absolute real binary path. It creates
no executable symlink and edits no shell profile or PATH. Preinstall rejects
any existing candidate version prefix, including same-version reinstalls or
damaged attempts, and rejects foreign or modified global commands. An upgrade
uses a fresh version slot. An existing Installer receipt is accepted only
with the verified old active wrapper/prefix, complete physical plist and BOM,
and exact package ID, version, install location and target-volume identity.
The receipt location is the exact volume-relative `usr/local/lib/marimay/V`
reported by `pkgutil`, while the package destination remains absolute.
Foreign, partial, corrupt or unqueryable receipts fail without replacement;
receipt absence is checked without enumerating unrelated installations.
Each version has a distinct receipt ID `org.marimay.mmay.V`. A shared ID is
unsafe for this layout: actual macOS Installer testing showed that upgrading
it deleted the previous BOM-owned files before postinstall, breaking the old
active command. The candidate receipt must be absent; the previous active
version's matching receipt is required and retained with its files. The retired
constant `org.marimay.mmay` receipt is a collision, without automatic migration.
Installation must not use `pkgutil --forget` to bypass ownership checks.
After payload/ownership validation, the wrapper is
activated last through atomic rename. Failure preserves the previously active
command. Standard Installer manages payload/receipts; atomic activation does
not promise atomic file-by-file package installation. A failed inactive
payload may require explicit user inspection and recovery, never automatic
deletion of unrelated or modified files.

The release workflow accepts the exact alpha tag or a manual request for that
tag, resolves one immutable source commit on main, and requires successful
complete repository CI/security, both macOS full suites, installed VM/native
debug/release smoke tests, and archive/package inventory and checksum checks
for that exact revision before publication. Only the publishing job receives
repository write permission. It rechecks tag/asset identities, fills a draft,
then publishes a prerelease. Published assets are immutable; a correction
uses a new alpha sequence. Failed or mismatched validation cannot publish a
public release. Full-history privacy and secret checks retain all rules and
do not waive metadata findings for a release.

Reason: an explicitly experimental download lets the owner and other learners
try the current compiler on supported Macs without presenting the unfinished
CLI0 audit or target language as complete. Separate publication authorization,
bounded artifact validation and product-gate readiness make that distinction
inspectable. No source-language or VM/native observable semantics change.

## D115 — Seeded non-cryptographic hash for managed collection keys

Status: accepted.

Managed Map/Set key hashing moves from SipHash-2-4 to the `mmay_hash_seeded`
owner: the XXH64 structure with both injected 64-bit collection seed words
folded into its standard seed slot (`seed0 ^ rotl64(seed1, 32)`), plus an
exact fixed-word entry point. The two-seed injection contract, per-heap seed
options, group-control high-bit derivation, probing, insertion order,
capacity policy, failure atomicity and D055 byte accounting are unchanged.

The ordered-collection decision already makes hashing unobservable: resizing,
layout, tombstones and hash seeds cannot change visible order or any result,
and a runtime may randomize hashing. No source syntax, formatter rule, MIR or
bytecode wire ID, public signature or observable program behavior changes.
Iteration remains insertion-ordered and key equality remains exact counted
UTF-8 comparison, so the algorithm swap has no semantic surface. SipHash-2-4
and its reference vectors remain in the base hash owner for the compiler's
internal tables; only the managed runtime dispatch changes.

Reason: string-keyed collections dominate the current local performance
laggards (word-frequency about 7.4x, set-string-churn about 3.3x, map-string
about 2.8x, rune-frequency about 3.7x from Go medians on the unchanged
corpus). Short-string key hashing is a measured common cost across those
rows. SipHash-2-4's per-block double rounds and six-round finalization are
disproportionate for 4–30 byte keys; the selected structure processes those
keys with one multiply-rotate round per 8 bytes and a short avalanche while
keeping both seeds mandatory. Hash-flooding hardening now rests on seed
injection as it already did: the default seeds remain fixed constants, so
controlled executions keep deterministic behavior and the documented security
review obligation for attacker-controlled map keys is unchanged. Collision
resistance claims for this hobby runtime are not strengthened or weakened by
this record; no DoS-resistance parity with SipHash is asserted.

## D116 — Publish the second experimental macOS alpha

Status: accepted (repository-owner direction, 2026-11-09).

The owner authorizes the next GitHub prerelease `v0.1.0-alpha.2` in the
D114 experimental-alpha sequence. Compiler, runtime and shipped
standard-library versions move from `0.1.0-alpha.1` to `0.1.0-alpha.2`;
the channel remains `experimental-alpha`, the claim remains
`local-checker-backed-slice`, and `publicationReady` remains `false`.
Published `v0.1.0-alpha.1` assets stay immutable per D114; this alpha
publishes the D115 managed-collection hashing change (PR #4) with its
recorded local evidence. No stable-language, production, signing,
notarization, trademark, registry, namespace or performance-parity claim
follows. The release workflow contract, portable installed macOS policy,
package layout, receipt identity rules and security review obligations
are unchanged from D114.
