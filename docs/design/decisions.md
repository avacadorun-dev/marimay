# Architectural decisions

Status values: `accepted`, `provisional`, `deferred`, `rejected`.

## D001 — Name, file extension, and command

Status: accepted; external distribution clearance remains gated by N0.

The official language and product name is **Ruda**. English prose, code-adjacent prose, product metadata, and technical identifiers spell it `Ruda`. Russian prose may additionally show the Cyrillic form **Ру́да**, pronounced «ру́да». `Rudalang` is not an official name; `ruda-lang` is only an allowed fallback for a domain, repository slug, or occupied external namespace.

The canonical toolchain executable is `ruda`, source files use `.ruda`, bytecode bundles use `.rudab`, project manifests are `ruda.mod`, `ruda.work`, and `ruda.lock`, and tool-owned local state uses `.ruda/`. All first-party workflows start with `ruda`; the former executable `ma`, source extension `.ma`, manifest names `ma.mod`/`ma.work`/`ma.lock`, bundle suffix `.mab`, and tool-state directory `.ma/` are not current aliases. Repository-owned user-visible identifiers and internal language/toolchain namespaces use `Ruda`/`ruda` consistently; this rule does not alter ordinary words merely containing the letters `ma`.

This is a pre-release product rename only: it changes no syntax beyond the source filename suffix, source semantics, manifest or package data model, MIR, bytecode/ABI contents, runtime behavior, or observable program behavior. The repository has not published a compatible release with established users, so no undocumented `ma` executable or `.ma` source compatibility alias is retained.

The name uses the ore metaphor: a clear language, strict types, and cohesive toolchain turn source material into reliable native software. It replaces the former working name **Malang** because a separately published project already used “Malang Compiler” for a programming language. This accepted text supersedes both D001's former provisional Malang identity and its short-lived intermediate wording that retained `ma`/`.ma`; no other accepted naming decision is superseded.

N0 remains a release blocker for trademark/legal review, domains, public handles, organization and registry namespaces, package/module prefixes, signing identity, and independent verification of `ruda` across supported PATH/package-manager environments. Accepting `Ruda` here does not claim that any domain, external account, package, namespace, or trademark is available, reserved, controlled, or legally cleared.

## D002 — Familiar C-family surface, Ruda-native forms

Status: accepted.

Use `fn`, `const`, `let`, `name: Type`, `<T>`, braces, and named imports because they are readable without a glossary. Give Ruda's core constructs their own regular forms: `type User { ... }` for records, `User { ... }` for construction, `Variant { ... }` for named enum payloads, `$"...{value}..."` for interpolation, `public` for every public declaration, and control-flow conditions without mandatory parentheses. Keep semicolons explicit where statements or signatures require them.

Do not inherit browser-language semantics: no coercion, prototypes, classes, `any`, nullish values, truthiness, automatic semicolon insertion, open record-shape assignment, or dynamic escape tier. Familiar punctuation is not a compatibility promise.

Reason: familiarity should reduce reading cost without weakening runtime guarantees.

## D003 — Ordinary functions with restricted first-parameter dot calls

Status: accepted.

Behavior is declared by an ordinary top-level function with an ordinary, meaningfully named first parameter:

```ruda
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

Safe Ruda has no null-like value. Absence is `Option<T>`. Recoverable failure is `Result<T,E>`. Postfix `?` propagates an `Err` from a function with the same error type. Error conversion is explicit; ordinary structural conversion to a declared interface remains available at an explicit construction/return boundary. Panic represents a violated invariant, not routine control flow.

## D007 — Managed reference data with immutable fields by default

Status: provisional, prototype gate required.

Nominal record values have managed reference identity. Fields are immutable after construction unless marked `mutable`. `const`/`let` govern rebinding of a local, not deep object mutability. Strings and enum values are immutable. Arrays and maps are managed mutable containers.

Reason: this avoids source-level ownership and accidental large copies while supporting graphs, services, and FFI handles. The single-threaded isolate rule prevents data races within one heap. The prototype must measure allocation behavior and confirm that escape analysis can stack-allocate or scalar-replace common short-lived records. The required workloads include decoded/encoded JSON DTO graphs, immutable UI-style trees, request/response models, and collection-heavy transformations; microbenchmarks alone cannot accept D007.

Rejected alternative: implicit copy-on-write value semantics for every record. It makes alias behavior attractive but imposes substantial hidden runtime and FFI complexity. Ruda may later add an explicitly declared compact `value type`; it will not change `type` semantics.

## D008 — One frontend and typed MIR, two required engines

Status: accepted.

Stage 0 implements a typed bytecode VM and a C17 native backend from one canonical MIR. VM is the default for `run` and `test`; native is the default for `build`. Engine choice cannot change accepted programs or observable language behavior.

Reason: fast interactive work and standalone deployment have different performance needs but do not require different languages.

## D009 — C17 bootstrap then self-hosting

Status: accepted.

The first compiler, VM, garbage collector, task runtime, bytecode verifier, formatter, and test harness are implemented in portable C17. After the language reaches the bootstrap gate, the compiler is rewritten in Ruda. Reproducible C17 Stage 0 remains the root of trust and must be able to build Stage 1.

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

There are no user-defined syntax macros, procedural macros, import hooks, package install scripts, or implicit build scripts. A small closed set of compiler derives may generate predictable functions. External code generation is an explicit `ruda generate` action and generated files are ordinary reviewed inputs.

## D015 — Canonical formatting is non-configurable in 1.x

Status: accepted.

`ruda fmt` has one style and is idempotent. Projects cannot configure brace, indentation, semicolon, or line-width policy. Compatibility exceptions require a language-versioned formatter change.

## D016 — Deterministic safe semantics

Status: accepted.

Evaluation order is left to right. Signed and unsigned integer overflow traps in every build mode; explicit wrapping operations live in the numeric API. Map iteration is insertion-ordered. Safe code has no undefined behavior. Clock, randomness, scheduling, and external I/O are modeled as nondeterministic inputs and can be controlled by the test runtime.

## D017 — Package builds are hermetic by default

Status: accepted.

The lock file pins immutable artifacts and cryptographic hashes. Build and test do not access the network after dependency fetch. Dependencies cannot execute installation code. The root application declares ambient runtime capabilities; libraries cannot silently broaden them.

## D018 — Exceptions and inheritance

Status: rejected for 1.x.

Ruda has no catchable exceptions for expected failure and no class inheritance. `Result`, exhaustive enums, interfaces, composition, and `defer` cover the intended use cases with visible control flow.

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

Reason: “Unicode display columns” is otherwise host- and terminal-dependent, especially for ambiguous-width characters, combining marks, variation selectors, and emoji sequences. Canonical formatting must produce identical bytes on every Tier 1 platform. Unicode UAX #11 explicitly requires contextual treatment of ambiguous width and notes that combining-mark East Asian Width is not advance width, so Ruda defines the non-East-Asian deterministic tailoring above rather than inheriting an OS policy.

## D027 — Manifest syntax and formatter gate ownership

Status: accepted.

I1 owns the bounded syntactic parsers and canonical formatters for `.ruda`, `ruda.mod`, and `ruda.work`, including their use by no-argument `ruda fmt`. I7 reuses those parsers and adds manifest semantic validation, dependency resolution, canonical `ruda.lock`, and package commands; it does not introduce a second manifest grammar or formatter. A syntactically valid manifest may therefore be formatted before its module names, version constraints, capabilities, or workspace membership can be semantically resolved.

Reason: the toolchain contract assigns no-argument canonical formatting to I1, while the original I7 roadmap wording also assigned all manifest parsing/formatting to I7. Leaving both statements in place would either make the I1 gate unverifiable or encourage a temporary formatter grammar that later drifts. One early hostile-input syntax implementation and one later semantic consumer preserve the public command contract and eliminate duplicate parsers.

## D028 — Deterministic formatter target discovery

Status: accepted.

No-argument `ruda fmt` finds the nearest regular `ruda.mod` on the physical working-directory ancestor chain and formats that module tree. It also formats the nearest regular `ruda.work`, if one exists from the module root upward. A module-tree walk visits regular `.ruda` files and the root manifests in unsigned UTF-8 path order, does not follow symbolic links, ignores tool-owned `.git` and `.ruda` directories, and does not enter a nested directory that owns another `ruda.mod`. An explicit regular file formats only that recognized `.ruda`, `ruda.mod`, or `ruda.work` input; an explicit directory uses the same bounded physical-tree rules rooted at that directory.

Directory-entry count, nesting, path bytes, and total selected files are bounded before unbounded allocation or recursion. Each file retains the existing per-file transactional rule; tree formatting does not claim an all-filesystem atomic commit. Discovery/traversal failures are formatter input failures, and a symbolic link explicitly selected as the target is rejected rather than followed.

Reason: root discovery and tree selection affect which user files a write-capable command mutates. Leaving symlinks, nested modules, hidden tool state, ordering, and bounds to host directory APIs would make `ruda fmt` nondeterministic and could rewrite sources outside the selected module. Physical no-follow traversal is portable, auditable, and independent of the later package resolver.

## D029 — Structured race and readiness selection before 1.0

Status: accepted, prototype gate required.

Direct-style concurrency must provide both structured first-completion racing and readiness selection before I6 is verified. A race owns its competing operations in a nested scope, cancels and joins losers, and cannot return while losing work remains detached. A selection waits on a finite set of task completions, bounded-channel operations, timers, or cancellation-aware I/O registrations, removes losing registrations, and does not implicitly cancel independently owned tasks. Deterministic test mode resolves simultaneously ready alternatives by declared source order and records the choice for VM/native replay.

These are compiler-aware first-party library/runtime operations, not new source syntax in 0.1. Exact API spelling is frozen only after the I6 prototype covers fan-out requests, hedged calls, timeout-plus-channel loops, closed channels, simultaneous readiness, cancellation during registration, and cleanup/panic paths.

Reason: `spawn`, `wait`, and a single-operation timeout do not express common server coordination without hand-built channels and cancellation plumbing. Requiring structured race/select semantics preserves D011 lifetimes while avoiding detached winner-takes-all work.

## D030 — Native toolchain delivery is a product responsibility

Status: accepted.

The required C17 AOT backend remains the auditable Stage 0 native path, but ordinary Tier 1 users must not configure compiler, linker, SDK paths, or cache keys manually. Each official distribution provides one tested zero-manual-configuration host-build path: a version-matched managed toolchain where redistribution permits it, or verified platform-toolchain discovery plus an installer/preflight flow that performs or precisely requests the one unavoidable platform installation step. `ruda build` itself remains offline and deterministic after installation.

Missing or incompatible native tools produce one actionable diagnostic and a machine-readable `ruda doctor` result; they never fail later as raw C compiler/linker output. Cross-compilation may require an explicitly installed target pack. A future direct object-code backend may remove the external C toolchain from day-to-day builds only after it reaches the same safety, portability, and VM/native differential gates; it does not weaken the required C17 bootstrap root.

Reason: exposing generated C is valuable for audit and bootstrap, but making users assemble a C development environment would break the single-command `ruda build` product promise. Packaging and discovery complexity belongs to the toolchain, not each Ruda project.

## D031 — Width-sensitive braced list layout

Status: accepted.

Import name lists, record constructions, record/named-variant patterns, and named enum payload declarations are formatter groups rather than unconditionally multiline blocks. If the complete group fits within the D026 100-column metric and contains no hard-line comment, canonical output keeps it on one line with one space inside each brace. Otherwise it breaks after `{` and before `}`, indents one item per line, and emits a trailing comma for comma-separated lists. Named enum payload declaration fields retain D025 semicolons and do not gain an internal comma.

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

Ruda has exactly two 0.1 `for` headers. Iteration uses
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

The Ruda language owns program meaning and static validity: types, memory and
error behavior, arithmetic, control flow, task lifetimes, and any effect that
is proven to be part of a function's semantic contract. The compiler proves
those rules and exposes stable factual outputs. First-party tools may use the
same typed AST, symbol graph, effect metadata, MIR, and diagnostics for
formatting, semantic diff, dependency analysis, testing, fuzzing, vetting, and
editor features.

Repository and CI policy owns authorship, generated-code provenance,
attestation, required coverage, review authority, and release admission.
Runtime and operating-system layers own sandbox enforcement and resource
limits. Ruda source has no author/agent marker, trust level, coverage promise,
or attestation construct. Co-locating compiler, formatter, tests, audit, and
package commands under the `ruda` executable does not make every command a
language feature.

A future source-level capability annotation is justified only if the
capability set changes the statically checked semantic contract of calls and
function values. A package/network/filesystem admission policy remains in the
manifest, runtime grant, sandbox, or CI layer and must not be added to syntax
merely to make an audit policy look language-enforced.

Reason: Ruda should be a small language with precise semantics from which
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

Ruda reserves a spelling globally only when the parser needs it to determine
program structure without surrounding context. The audited 0.1 hard-keyword
set is `defer false if match return spawn true while`. The parser treats
`as break case const continue else enum extern fn for from import interface let
mutable of public scope type unsafe yield` contextually: each retains its
grammatical role where that role is unambiguous and otherwise becomes an
ordinary identifier token in the syntax tree. Hard keywords remain unavailable
as declaration, field, member, or local names in 0.1; Ruda does not add a
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
source file ending `_test.ruda`; normal builds exclude those files while test,
fuzz, and explicit all-targets checks include them. The unshadowed prelude declaration
`fn panic(message: string): never` is compiler intrinsic, but normal lexical
and package name resolution may shadow it. Its stable D036 runtime identity
applies only after resolution selects that prelude symbol.

`yield expression;` is recognized contextually as the match-arm transfer form
and remains invalid outside a block match arm. `yield()`, `task.yield()`, and
declarations named `yield` are ordinary identifier uses. `in`, `any`, `async`,
`await`, `class`, `extends`, `impl`, and `throws` are also ordinary identifiers:
Ruda omits the corresponding type or construct by grammar/semantic rules, not
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

No user-controlled Ruda identifier is copied verbatim into a generated C
identifier. The C17 backend assigns functions, types, field accessors, locals,
temporaries, descriptors, and helpers injective compiler-owned spellings from
stable symbol identity plus bounded ordinals. A generated spelling is valid
C17, avoids every C keyword and implementation-reserved namespace, cannot
collide with runtime helpers or C-library external identifiers, and remains
distinct under target filesystem case folding and backend length limits.
Unicode source spelling and normalization-distinct Ruda names remain distinct.

Explicit `extern "c"` link names are not internal generated identifiers. They
are validated foreign ABI contracts and are never silently mangled; I9 owns
their platform-specific linkage and library validation.

Reason: reserving C vocabulary in Ruda would leak the temporary Stage 0 backend
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

```ruda
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

```ruda
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

`println` requires the named capability `"io.stdout"`. `ruda run` of a local
module or script, and a native executable produced by `ruda build`, grant it by
default against the process stdout stream. `ruda test` grants a captured sink
by default; `expect.output` compares those exact bytes. `ruda exec` of an
untrusted `.rudab` denies it unless the invocation grants it. Libraries cannot
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
Public VM, native, and `ruda test` capture comparison for this surface uses the
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

```ruda
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
local module graph, beginning with `ruda check`, additionally requires exactly
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

Default batch checking excludes files ending `_test.ruda` and the module-root
`tests/` tree. `ruda check --all-targets` includes both. Traversal does not
follow symbolic links, enter `.git` or `.ruda`, or cross a nested `ruda.mod`,
and it is bounded by documented path, depth, entry, file-count, and source-byte
limits. Batch requests never consume editor overlays or persistent semantic
caches: each invocation loads immutable disk snapshots into a fresh compiler
service and checks the complete selected graph.

An accepted source construct that has not reached the authoritative checker
cannot produce a successful `ruda check`. The command reports `I0001` and exits
70 rather than treating an incomplete compiler status as a clean program.

Reason: checkpoint 8 cannot truthfully expose complete, deterministic module
checking without a stable local package identity, while moving dependency and
supply-chain policy ahead of I7 would couple an otherwise local compiler
command to unfinished resolution semantics. This narrow exception resolves
that boundary without changing the lossless manifest tree used by I1.

## D049 — Local `ruda run` has one verified VM boundary and closed outcomes

Status: accepted.

`ruda run` uses the D048 local target and root-package discovery rules. A
standalone `.ruda` target is the root `script` package. A module target selects
the canonical package represented by that target; no-target and an explicit
module-root target select the module root package. Only a function named
exactly `main` in that root package can be the entry symbol. It must be a
non-external, non-test, non-fuzz function with no parameters and one of these
exact return types:

```ruda
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
`ruda run`.

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

The Stage 0 checkpoint-10 `ruda build` path reuses D048 target discovery,
D049 entry selection, and the authoritative verified typed MIR. Native is the
only public build engine in this checkpoint. `--release`, `--output`, and
`--emit c` are accepted; `--engine vm` and `--emit mir` remain `U0005` staging
errors, while cross-target packs remain outside this checkpoint.

The native owner emits one portable C17 translation unit plus a compiler-owned
entry adapter, links the version-matched runtime archive, and invokes the
CMake-selected supported host compiler directly with an argv vector. It never
uses a command shell, consults `CC`, or accesses the network. The compiler
identity and runtime locations are distribution metadata rather than project
configuration. A missing compiler is detected by a bounded preflight before C
generation and is `B0002`/exit `1`, with one platform installation action.

The default artifact lives below
`.ruda/build/native/{debug|release}/{sha256}/app` (`app.exe` on Windows), and
`ruda build` writes that path followed by LF on success unless `--quiet` is
set. The SHA-256 key covers generated C, compiler/runtime/language versions,
host compiler identity, target host identity, profile, and the versioned
backend flag contract; absolute workspace paths do not enter it. A regular
artifact at that exact path is a cache hit. `--output` additionally compiles
and atomically replaces the exact requested sibling output. `--emit c`
atomically publishes the public-entry translation unit instead of invoking the
host compiler. Failed checking or code generation never executes or publishes
a stale temporary artifact.

Debug uses strict C17 at `-O0` (or `/Od`), release uses `-O2` (or `/O2`), and
both retain checked Ruda overflow, division, shift, bounds, panic, and root
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

The Stage 0 checkpoint-11 `ruda test` path discovers only ordinary
`@test fn(): void` declarations in `*_test.ruda`. Package tests retain their
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

`ruda build` treats the requested output profile as authoritative for every
first-party native object. Each Stage 0 toolchain build and official host
installation contains separate debug and release runtime archives. The debug
archive is compiled with the debug optimization contract (`-O0` or `/Od`); the
release archive is compiled with the release optimization contract (`-O2` or
`/O2`). Which configuration was used to compile the invoking `ruda` executable
does not affect this selection.

Before generated-C emission, the native artifact owner selects the requested
archive and validates a versioned manifest plus embedded runtime-version and
profile markers. A missing, stale, partial, or wrong-profile artifact is
`B0008`/exit `1` and never falls through to an opaque host linker failure. An
installed tool first resolves `../lib/ruda/{runtime-version}/native/{profile}`
and `../include` relative to the invoked `ruda` path. A partially present
installed candidate is an error; only the absence of the complete installed
candidate permits the development build-tree metadata fallback. Relocating a
complete installed prefix therefore preserves native builds without retaining
a dependency on the original build tree.

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

```ruda
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

`ruda run`, `ruda test`, and `ruda build` accept one
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
own bounded owners. An allocation whose checked `live + requested` equals the
limit is permitted. If it would exceed the limit, the runtime first performs
the collection attempt required by D045/D053; the post-collection `live` value
and the net additional accounted `requested` value determine the outcome.
Budget rejection is failure-atomic and does not attempt the rejected host
allocation.

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
budget, Ruda compiler path/hash/version, selected release-runtime
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
is not eligible for a budget. The reviewed per-platform, per-workload Ruda
regression limit is the baseline median multiplied by one plus the greater of
10% and that noise fraction. Cross-language ratios remain exploratory and do
not become Ruda release budgets.

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
the faithful eight-workload algorithm corpus with the full Ruda, Go, and Rust
participant matrix. The result must use release Ruda artifacts, retain at
least three warmups and eleven measured samples, pass exact schema, checksum,
artifact, manifest, metadata, and noise validation, and have reviewed
platform/workload Ruda budgets. Other platform reports may be retained as
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
in-bounds capability. Get/set transport one `RudaManagedCell`; type-directed
generated C converts that cell exactly once at the call boundary. Set validates
reference-valued elements and advances the D053 mutation generation only after
the access is known to succeed.

The native emitter may fuse an `ARRAY_REQUIRE_INDEX` only with its immediately
following matching `ARRAY_GET_IN_BOUNDS` or `ARRAY_SET_IN_BOUNDS`. The fused
access uses checked mode and retains the barrier's G4002 source span. A
non-adjacent or otherwise observable barrier remains an explicit `require`
call; subsequent verified consumers use in-bounds mode. An
`ARRAY_ASSUME_INDEX` emits no runtime bounds check. In every case the array
reference remains in the native shadow-root frame for the call, and the ABI
retains no resolved slot, array, cell pointer, capacity, or length after
returning. Calls, allocation, collection, growth, and initialized-sequence
mutation therefore continue to invalidate D058 capabilities conservatively.

Whole-program or archive LTO is not the selected boundary. The released
runtime is a relocatable, version-matched C17 archive that must work with the
distribution-selected compiler on every Tier 1 platform. LTO objects and
linker-plugin contracts are compiler-family and version coupled, are not
uniform across Clang/GCC/MSVC archives, and would make the access path depend
on the host/profile combination. The measured D059 prototype comparison is
recorded in checkpoint-23 evidence; the exact ABI is the sole retained path,
and the former generated helper plus describe/length/get-set chain is removed.

Reason: one stable C17 ABI call removes repeated handle, descriptor, length,
bounds, status, and cell-boundary work without exposing representation or
letting a linker-dependent optimization mode choose source-observable safety.
