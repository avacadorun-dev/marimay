# Design baseline consistency audit

Audit date: 2026-08-10.  
Scope: repository design package before implementation.

## Result

The baseline is internally consistent enough to hand to an implementation agent. It is intentionally not marked as an implemented language. D001 accepts `Ruda`, `ruda`, and `.ruda`; N0 remains an external distribution-identity release blocker and does not make the repository identity provisional. Four architectural decisions additionally remain gated by executable prototypes or benchmarks: managed record representation (D007), inferred suspension (D010), isolate parallelism (D012), and shape-shared generics (D013). D010 is accepted in intent but still has a mandatory feasibility gate; D007/D012/D013 remain provisional.

No provisional item permits an implementation to silently substitute source-level async, ownership syntax, shared mutable threads, a dynamic VM, or per-backend semantics.

## Surfaces compared

- README and examples;
- design principles and decisions;
- language, execution, toolchain, standard-library, and diagnostic specs;
- implementation brief, roadmap, and test strategy;
- requirement traceability index;
- local Markdown links and code-fence structure.

## Resolved findings

1. Removed `Eq`/`Hash` derive from the mutable `User` example because the language rejects value equality/hash derivation over mutable fields.
2. Added `fuzz` to the keyword set and grammar after the toolchain introduced a native fuzz declaration.
3. Defined the closed compiler-attribute model used by `@derive`, `@link`, and `@blocking`.
4. Defined `Error.message()` in the prelude and zero-argument construction for a positional `void` payload, making `Ok()` valid.
5. Defined record-construction field shorthand used by examples.
6. Replaced the pseudo-static `Headers.json()` with package function `http.jsonHeaders()` because Ruda has no class/static-method model.
7. Replaced `Array.of` wording with prelude `arrayOf` for the same reason.
8. Clarified that `spawn` may be nested anywhere under the active lexical `scope` block, not only as its immediate statement child.
9. Defined the only user-type operator hook: exact equality contract used by `==`/`!=`; general operator overloading remains absent.
10. Defined insertion-order behavior for update versus delete/reinsert in maps and sets.
11. Kept `?` conversion exact and made error mapping explicit in cross-domain examples; structural interface boxing remains an explicit boundary conversion.
12. Separated task concurrency from isolate parallelism and documented that lack of simultaneous data races does not remove logical ordering races.
13. Replaced compatibility-oriented surface forms with one Ruda-native grammar: `fn`, `type User { ... }`, `User { ... }`, named enum payload braces, `$"..."` interpolation, one `public` modifier, and unparenthesized control flow.
14. Established ordinary first-parameter functions as the sole source of dot-call behavior and interface conformance; package ownership supplies coherence without a second declaration form.
15. Renamed the structured-concurrency lifetime block to `scope`; `spawn` remains the only task-creation operation.
16. During I0 implementation, clarified non-source diagnostics through D021: the required JSON `primary` field is nullable, so command/package/runtime failures do not invent source spans.
17. Defined exact byte versus logical position behavior through D022: byte-zero BOM handling and LF/CRLF/lone-CR mapping are stable across source snapshots and formatter normalization.
18. Pinned Unicode identifier membership through D023: language `0.1-design` uses committed Unicode 17.0.0 XID ranges, exact source spelling, and a standalone wildcard/discard `_`.
19. Closed the 0.1 literal grammar through D024: numeric separators/bases/exponents, string/rune/byte escapes, raw strings, and interpolation boundaries are now locale-independent and testable.
20. Resolved the named-enum-payload separator conflict through D025: declaration fields use `name: Type;`, while the variant list retains commas.
21. Closed formatter-width host dependence through D026: Unicode 17 scalar properties define the 100-column metric, with ambiguous width narrow and no host `wcwidth` dependency.
22. Resolved manifest formatter gate ownership through D027: I1 owns the bounded `ruda.mod`/`ruda.work` syntax and canonical bytes required by no-argument `ruda fmt`; I7 reuses them for semantic resolution and lock/package workflows.
23. Closed formatter target-selection ambiguity through D028: nearest-module discovery, optional ancestor workspace formatting, nested-module boundaries, no-follow symlink behavior, deterministic path order, and traversal limits now have one cross-platform contract.
24. Added the pre-1.0 structured coordination gate through D029: race owns/cancels/joins losers, readiness selection unregisters losing waits without cancelling independently owned tasks, and both replay deterministically.
25. Made native toolchain usability an owned product constraint through D030: the C17 AOT/root-of-trust remains, while official Tier 1 installation and `ruda doctor` absorb compiler/linker/SDK discovery instead of user project configuration.
26. Reopened the former D001 after confirming an indexed “Malang Compiler” Go command module: the historical project-name collision is evidence-backed and triggered a coordinated pre-release rename rather than an undocumented compatibility fork.
27. Closed braced-list layout ambiguity through D031: import names, record construction/patterns, and named enum payload fields use the D026 fit oracle, while executable and declaration bodies remain structurally multiline.
28. Accepted the replacement D001 and completed the repository identity migration to `Ruda`, `ruda`, `.ruda`, `.rudab`, `ruda.mod`/`ruda.work`/`ruda.lock`, and `.ruda/`; former names remain only in explicit historical records, while N0 continues to own legal and external distribution clearance.
29. Separated semantic facts from trust policy through D034: the compiler may
    expose typed trees, symbol/effect graphs, MIR, and diagnostics to every
    first-party tool, while author/agent identity, provenance, coverage,
    review, and release admission remain repository/CI concerns rather than
    source constructs.
30. Closed shift semantics through D035: exact integer operands and bounded
    counts are checked, left shift traps on an unrepresentable mathematical
    result, signed right shift sign-extends, and no backend inherits host-C
    signed-shift behavior.
31. Fixed explicit source panic identity through D036: `panic(message)` is a
    one-string-argument `never` expression with code `G4000`, exact message
    bytes, and the complete call span across every execution engine.
32. Closed declared-`never` call behavior through D037: arguments evaluate in
    ordinary order, the call has no normal MIR successor, and no backend may
    fabricate a runtime bottom value or fall through the call.
33. Replaced and then exhaustively re-audited the reserved-word boundary
    through D038: eight genuinely ambiguous hard keywords remain global,
    twenty-one grammar words are contextual, `null` and
    `undefined` remain sentinel-reserved, and ordinary `@test`/`@fuzz`
    functions plus the shadowable prelude `panic` replace special declarations
    and call syntax. This supersedes resolved finding 2's former `fuzz` keyword
    and the keyword aspects of findings 3 and 31.
34. Closed the Ruda-to-C17 identifier boundary through D039: internal emitted C
    identifiers are injective compiler-owned names rather than source spelling,
    so C keywords, implementation-reserved identifiers, library/runtime names,
    Unicode, truncation, and filesystem case folding cannot leak into Ruda
    source acceptance; explicit foreign link names remain separate ABI
    contracts owned by I9.
35. Made D007 record identity executable in I2: `identity.same` accepts exactly
    one nominal record type on both sides, evaluates operands once from left to
    right, lowers to verified typed MIR, and compares stable execution handles
    rather than fields or backend addresses. The oracle proof does not accept
    D007's still-open production-GC and allocation-performance gate.
36. Closed the formatter/evaluation-order conflict through D040: field names
    choose storage destinations, written construction order controls
    initializer effects, and syntax-only formatting preserves that order.
37. Closed the fallible-`File.close()`/void-`defer` conflict through D041:
    explicit `close()` observes errors, `closeBestEffort()` is an idempotent
    non-panicking fallback, and cancellation plus cleanup-panic precedence are
    now fixed across every exit kind.
38. Repaired the `?` example against D006 and `std/fs`: `readText` receives its
    required limit and `fs.Error` is mapped explicitly to `ConfigError`; the
    corresponding pass/fail doc obligations remain visible until the library
    surface is checker-backed.
39. Closed signed division/remainder through D042: quotient rounding,
    remainder sign/identity, zero divisors, and both `MIN/-1` operations now
    have one VM/C-safe contract and executable I2 oracle cases.
40. Closed import-dependent structural conformance through D043: public
    behavior owned with an imported nominal type participates regardless of
    named-type, named-function, or namespace import spelling; free-call
    spelling remains import-dependent and E0 still measures discoverability.
41. Closed deterministic `f64` execution through D044: decimal literals use
    locale-independent nearest-even binary64 conversion; arithmetic, NaN,
    signed-zero, comparison, raw-bit boundary, and strict native-host rules now
    have one VM/C-safe contract with executable differential coverage.
42. Closed managed-array access and growth ambiguity through D045: construction
    and inference use exact element types, indexes are exact `int`, G4002 owns a
    stable message and complete access span, indexed assignment checks before
    right-hand-side effects, and recoverable logical growth is distinct from a
    VM/isolate heap-limit outcome.
43. Closed 0.1-dev user stdout through D046: `std/io.println` accepts only
    `string` and returns `Result`, `std/convert` owns explicit `formatF32`/
    `formatF64` spellings, `"io.stdout"` is a named capability, published bytes
    are UTF-8 plus one LF, and the internal `ok:f64:` harness is not a public
    API.
44. Closed integer policy and conversion syntax through D047: the eight exact
    integer primitives share one checked/wrapping/saturating function family,
    invalid operands have explicit Option-versus-panic behavior, and
    `value as Target` is either total or loss-detecting without implicit,
    wrapping, saturating, or integer/float conversion.

## Automated structural checks performed

- all local Markdown link targets exist;
- every Markdown code fence is balanced;
- example braces, parentheses, and brackets are balanced under a lightweight lexical check;
- banned source constructs (`impl`, source `async`/`await`/`Future`, `null`, `undefined`, classes) do not occur in executable example code;
- the repository map and all linked deliverables resolve;
- each normative requirement category maps to a roadmap gate.

These checks are not a substitute for parsing. At I1, the real lossless parser and `ruda fmt --check` replace the lightweight example check. At I2/I6, examples become compile/run conformance fixtures.

The reproducible commands and final results are recorded in the [D0 verification evidence](../implementation/evidence/d0-2026-08-10.md).

## Intentional non-final details

The following precision belongs to its implementation gate and is explicitly forbidden from drifting without a decision/spec/test update:

- exact generated parser grammar and recovery tables — I1;
- diagnostic numeric code allocation — I0/I2;
- concrete MIR serialization — I2;
- opcode numbers and exact `.rudab` binary field widths — I3;
- native internal helper ABI and C partitioning — I4;
- GC allocation thresholds and performance budgets — I5;
- scheduler quantum/poller adapters and direct-suspension feasibility — I6;
- registry wire protocol/trust-root operations — I7;
- secondary standard-library dot-call sets — I5–I9;
- final Tier 1 claims and TLS backend matrix — I9/P1;
- controlled public domains, organization, registry/package namespace, signing identity, and external distribution clearance — N0 before I7/public E0/P0.

Each is an implementation detail only within the constraints already stated. For example, I3 may assign opcode numbers but may not make bytecode dynamically typed; I6 may choose platform pollers but may not expose executor selection.

## Review questions for the next design round

These are product questions, not blockers for the initial vertical slice:

- Does managed reference identity for every `type` feel simpler than adding an explicit value-record form?
- Is `case Pattern => expression` plus `yield` for multiline match arms the clearest surface after user testing?
- Are mutable fields explicit enough, or should mutation be restricted further to functions in the owning package?
- Does exact error mapping remain readable in larger domain boundaries?
- Does `scope { spawn ...; task.wait(); }` remain clearer than group APIs under timeouts/races?

Answers require prototypes and users, not preference-only edits. Any accepted change updates the decision, normative spec, examples, and requirement/test mapping together.
