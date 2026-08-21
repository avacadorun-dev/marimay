# Ruda requirements and traceability index

Status: authoritative requirement index. Normative details live in the linked specifications; this file prevents scope loss during implementation and release audit.

Evidence states used by implementation: `missing`, `contradicted`, `partial`, `verified`. A roadmap gate cannot be verified while any requirement assigned to it is missing, contradicted, or merely partial.

## Language

| ID | Requirement | Normative source | First full gate |
|---|---|---|---|
| LANG-001 | `.ruda` source is statically checked; no dynamic fallback | language §1 | I2 |
| LANG-002 | familiar C-family syntax with native record, construction, interpolation, visibility, and control-flow forms | language §§2–8; D002 | I2 |
| LANG-003 | no null-like value; absence is `Option<T>` | language §5.2; D006 | I2 |
| LANG-004 | expected failure is `Result<T,E>` with visible postfix `?` | language §10.1; D006 | I2 |
| LANG-005 | records and enums are nominal; interfaces are structural and sound | language §§5.4–5.8; D004 | I2 |
| LANG-006 | enum supports unit/positional/named payloads with D025 declaration-field separators | language §5.5; D025 | I2 |
| LANG-007 | `match` destructures and is exhaustive over closed data | language §9; D005 | I2 |
| LANG-008 | guards do not count toward exhaustiveness; unreachable arms rejected | language §9 | I2 |
| LANG-009 | behavior is an ordinary top-level function; eligible exact first parameters derive dot-call behavior without a second method declaration system or `impl` | language §6.2; D003 | I2 |
| LANG-010 | `value.method()` and `method(value)` resolve to one symbol and evaluate the first argument once | language §6.2 | I2/I4 |
| LANG-011 | fields immutable by default; mutable fields explicit | language §5.4; D007 | I5 |
| LANG-012 | records have managed stable reference identity in 1.0 | language §5.4; D007 | I5 |
| LANG-013 | integer overflow/bounds checks are defined in every build mode | language §§5.1,7; D016 | I4 |
| LANG-014 | evaluation order is left to right and operands evaluate once | language §7; D016 | I4 |
| LANG-015 | safe source has no undefined behavior | language §§12,14; D016 | I4/I9 |
| LANG-016 | generic constraints are bounded structural interfaces without associated-type solver | language §§5.7–5.8 | I2 |
| LANG-017 | no general overloads; same-name dot-call functions may differ only by distinct locally owned first nominal types | language §6.2 | I2 |
| LANG-018 | `defer` is lexical LIFO cleanup on every exit kind | language §10.3 | I2/I6 |
| LANG-019 | no user macros/build-time execution/operator/coercion system in 1.0 | language §14; D014 | I7 |
| LANG-020 | unsafe/FFI operations are explicit and documented | language §12 | I9 |
| LANG-021 | functions use `fn`; structured child-task lifetime uses `scope`, with `spawn` as the creation operation | language §§2.3,11; D003a | I2/I6 |
| LANG-022 | identifier acceptance uses committed Unicode 17.0.0 XID tables and does not vary with the host | language §2.1; D023 | I1 |
| LANG-023 | numeric, string, byte, raw, rune, and interpolation spellings use the closed locale-independent D024 grammar | language §2.5; D024 | I1 |
| LANG-024 | exact error propagation remains explicit, and multi-layer `mapError` ergonomics are externally measured before self-hosting/language freeze | language §10.1; D006; roadmap E0 | E0 |
| LANG-025 | shifts require exact integer types and bounded counts; left shift is checked, signed right shift sign-extends, and compound forms preserve the same semantics | language §§5.1,7; D035 | I2/I4 |
| LANG-026 | explicit `panic(message)` requires one string, has type `never`, and preserves stable code/message/call span | language §10.2; diagnostics §9; D036 | I2/I4 |
| LANG-027 | a declared-`never` function has no normal return, and its call lowers to a terminating MIR edge without a runtime value | language §§5.1,6.1; execution §13.1; D037 | I2/I4 |
| LANG-028 | exactly eight ambiguous words are hard keywords; all 21 contextual grammar words remain usable names under bounded construct lookahead, test/fuzz use attributes, panic is a shadowable prelude intrinsic, and only null/undefined remain sentinel-reserved | language §§2.3,2.6,9,10.2; toolchain §5; D038 | I1/I2/I8 |
| LANG-029 | string literal patterns compare exact decoded UTF-8 bytes, equivalent spellings are redundant, and an unguarded catch-all is required for exhaustiveness | language §9; D005/D032 | I2/I4 |
| LANG-030 | record patterns are nominal and field-complete; flat binding/wildcard forms project typed fields, are irrefutable without a guard, and preserve managed reference identity | language §§5.4,9; D004/D005/D007 | I2/I4/I5 |
| LANG-031 | `identity.same` accepts only the same nominal record type, evaluates operands once left-to-right, and observes stable managed identity rather than fields or backend addresses | language §5.4; execution §2; D007 | I2/I4/I5 |
| LANG-032 | record and named-payload construction evaluates initializers once in written source order, and canonical formatting never reorders them or requires semantic resolution | language §§5.4,7; toolchain §4.2; D040 | I1/I2/I4 |
| LANG-033 | `defer` accepts only `void`, runs exactly once in LIFO order on every exit, masks requested cancellation during cleanup, and has deterministic primary/suppressed panic precedence; fallible resource policy remains explicit | language §10.3; standard library §5; diagnostics §9; D041 | I2/I6/I9 |
| LANG-034 | signed division truncates toward zero, remainder follows the dividend and quotient identity, and zero plus `MIN / -1`/`MIN % -1` are checked before backend host operations | language §§5.1,7; execution §5.1; D042 | I2/I4 |
| LANG-035 | a public nominal type's owning-package dot-call set and structural conformance are invariant across named type, named function, and namespace import forms; import spelling changes only free-call visibility | language §§3,5.8,6.2; D003/D043 | I2/E0 |
| LANG-036 | `f64` decimal literals round exactly to binary64 nearest-even; raw bits cross value boundaries unchanged; arithmetic is strict and canonicalizes NaNs; unary negation preserves NaN payload bits; comparisons and signed-zero division follow D044 | language §5.1; execution §§5.1,15; D044 | I2/I3/I4 |
| LANG-037 | managed `[]T` construction, exact `int` indexing, mutation order, `length`, `push` growth result, and G4002 bounds identity follow D045 across MIR, VM, and native execution | language §§5.2,5.6,7; execution §§5.1,9; diagnostics §9; D045 | I2/I3/I4/I5 |
| LANG-038 | user stdout is not a language primitive or implicit conversion; `println` accepts only `string` and interpolation is not the D046 write contract | language §§1,2.5; standard library §3.1; D046 | I5 |
| LANG-039 | all eight integer primitives expose one closed checked/wrapping/saturating policy family, and `value as Target` is the only loss-detecting explicit integer conversion form; invalid divisors and shifts retain the D035/D042 panic rules | language §§5.1,7; D035/D042/D047 | I2/I4 |
| LANG-040 | managed arrays expose typed pop/truncate/clear/reserve operations, precisely release removed reference elements, keep capacity unobservable, and invalidate iteration through the non-wrapping D053 mutation generation | language §§5.6,8; standard library §4; execution §§9,13; D053 | I2/I3/I4/I5 |
| LANG-041 | the existing managed array identity has canonical right-associative `[]T` source, formatter, diagnostic, and compiler-service spelling; unresolved prelude-era `Array<T>` is rejected without capturing package-owned nominal `Array` or `Slice<T>` | language §5.2; diagnostics §5; toolchain §6.1; D054 | I1/I2/I8 |

## Execution and concurrency

| ID | Requirement | Normative source | First full gate |
|---|---|---|---|
| EXEC-001 | VM and native share typed MIR and one safe semantics | execution §§1–3; D008 | I4 |
| EXEC-002 | VM is required/default for run/test; native required/default for build | execution §3; toolchain §6 | I4 |
| EXEC-003 | portable `.rudab` is typed and verified before execution | execution §4 | I3 |
| EXEC-004 | generated C17 avoids C undefined behavior | execution §5 | I4 |
| EXEC-005 | suspension effect inferred internally; no source `async`/`Future` color | execution §6; D010 | I6 |
| EXEC-006 | standard I/O parks only current task, not isolate carrier | execution §§6,10 | I6 |
| EXEC-007 | concurrency created explicitly by `spawn` inside `scope` | execution §7; D011 | I6 |
| EXEC-008 | `Task<T>` is affine/scope-bound and cannot escape | execution §7.2 | I6 |
| EXEC-009 | every scope exit cancels and joins unfinished children | execution §7.3 | I6 |
| EXEC-010 | panic/cancellation/defer/task ancestry preserve source traces | execution §§7,14 | I6 |
| EXEC-011 | cancellation is structural control, checked at finite safepoints | execution §7.6 | I6 |
| EXEC-012 | one isolate runs one heap task at a time; parallelism uses separate heaps | execution §8; D012 | I6 |
| EXEC-013 | safe code has no mutable cross-isolate reference | execution §8 | I6 |
| EXEC-014 | GC is precise per isolate; no user finalizers | execution §9 | I5 |
| EXEC-015 | blocking FFI uses bounded backpressured pool | execution §§10–11 | I6/I9 |
| EXEC-016 | deterministic record/replay compares VM/native semantic traces | execution §13 | I4/I6 |
| EXEC-017 | runtime capabilities constrain standard APIs and sandboxed bundles | execution §12 | I9 |
| EXEC-018 | execution prototype gates pass before concurrency syntax freeze | execution §16 | I6 |
| EXEC-019 | structured race cancels/joins owned losers and readiness selection unregisters losing waits without detached work | execution §7.7; D029 | I6 |
| EXEC-020 | the C17 backend assigns injective compiler-owned identifiers and never copies user names into C keyword, implementation-reserved, library, runtime-helper, or case-folded collision namespaces; explicit foreign link names remain separately validated ABI contracts | execution §5.1.1; D039 | I4/I9 |
| EXEC-021 | VM, native, and test-capture agree on D046 stdout bytes, `Result` classification, and `io.stdout` denial; internal `ok:f64:` traces are not user stdout | execution §§10.1,13.1; toolchain §§5.3,6.4; D046 | I4/I5/I8 |

## Toolchain and ecosystem

| ID | Requirement | Normative source | First full gate |
|---|---|---|---|
| TOOL-001 | one executable `ruda` owns first-party workflow | toolchain opening; AGENTS | I0/I8 |
| TOOL-002 | no-argument `ruda fmt` formats current module source and manifests canonically, with manifest syntax owned by I1 and reused semantically by I7 | toolchain §§1,3–4; manifest; D027 | I1 |
| TOOL-003 | formatter has no 1.x style configuration, uses the platform-independent D026 column metric, and is idempotent | toolchain §4; D026 | I1 |
| TOOL-004 | no-argument `ruda test` runs current module in VM | toolchain §§1,5 | I3/I8 |
| TOOL-005 | tests need no external framework/config and are isolated/deterministic | toolchain §5 | I8 |
| TOOL-006 | `ruda test --engine both` detects semantic divergence | toolchain §5.5 | I4 |
| TOOL-007 | unit/integration/doc/property/fuzz use one first-party harness | toolchain §5 | I8 |
| TOOL-008 | `ruda check` runs the authoritative frontend/checker/MIR verification without a backend, checks a deterministic complete local module graph under D048, is unaffected by editor overlays, and cannot report success for checker-incomplete accepted syntax | toolchain §§1.1,6.1; manifest; diagnostics; D048 | I2 |
| TOOL-009 | `ruda run` defaults VM; `ruda build` defaults native | toolchain §§6.2–6.3 | I4 |
| TOOL-010 | diagnostics have stable codes, spans, fixes, JSON, and offline explanations | diagnostics; toolchain §§2,8 | I0–I8 |
| TOOL-011 | manifest/lock resolution is deterministic and offline reproducible | toolchain §§3,7 | I7 |
| TOOL-012 | dependencies cannot run install/build/procedural code | toolchain §7.2; D014/D017 | I7 |
| TOOL-013 | artifacts are immutable, hashed, and provenance/audit aware | toolchain §7 | I7 |
| TOOL-014 | core dependency commands remain shallow (`ruda add`, not required nested tooling) | toolchain §7 | I7 |
| TOOL-015 | `ruda doc`, `vet`, `fix`, `bench`, `fuzz`, `audit` are first party | toolchain §§7–8 | I8 |
| TOOL-016 | source/language/format/package compatibility is versioned | toolchain §10 | P1 |
| TOOL-017 | the I2 compiler service owns overlay diagnostics, definition, symbol search, and exact checker-backed dot-call completion; I8 productionizes LSP transport | toolchain §6.1 | I2/I8 |
| TOOL-018 | official Tier 1 host installation makes native `ruda build` usable without per-project/manual C compiler, linker, or SDK selection | toolchain §6.3; D030 | I4/I9 |
| TOOL-019 | language/product name, command, domains, module/registry namespace, artifact identity, and installer collisions are evidence-audited and controlled before public distribution | D001; naming audit; roadmap N0 | N0 |
| TOOL-020 | formatter uses D031 width-sensitive braces for import/record/pattern/payload lists while keeping executable and declaration bodies structurally multiline | toolchain §4.2; D031 | I1 |
| TOOL-021 | `ruda build` selects and validates a version-matched runtime for the requested debug/release output profile independently of the compiler's own build configuration, remains relocatable when installed, and hashes runtime content into the build cache key | toolchain §6.3; execution §5.6; D052 | I4 |

## Standard library

| ID | Requirement | Normative source | First full gate |
|---|---|---|---|
| STD-001 | prelude remains small and contains Option/Result/Error/collections only | standard library §§1–2 | I5 |
| STD-002 | strings are valid UTF-8 with explicit byte/rune/grapheme views | standard library §3 | I5 |
| STD-003 | collections are bounded and map/set iteration insertion-ordered | standard library §4 | I5 |
| STD-004 | resource and whole-input APIs expose cleanup and size limits | standard library §§5,9–10 | I9 |
| STD-005 | timeouts, structured race/select, channels, and isolate APIs preserve task-scope invariants | standard library §§6–8; D029 | I6 |
| STD-006 | HTTP/TLS support streaming, limits, cancellation, graceful shutdown | standard library §9 | I9/P2 |
| STD-007 | JSON typed codecs do not use unrestricted private reflection | standard library §10 | I8 |
| STD-008 | logging/tracing are bounded, contextual, and redactable | standard library §11 | P2 |
| STD-009 | C API is outside prelude and enforces handle/suspension safety | standard library §13 | I9 |
| STD-010 | every production std package meets tests/platform/security/benchmark gate | standard library §14 | P1 |
| STD-011 | 0.1-dev preview stdout is `std/io.println(text: string): Result<void, io.Error>` with explicit `std/convert.formatF32`/`formatF64`, `"io.stdout"` capability, UTF-8 plus LF, and line-atomic publish | standard library §§3,3.1; D046 | I5/I6 |

## Implementation, bootstrap, and production

| ID | Requirement | Normative source | Full gate |
|---|---|---|---|
| IMPL-001 | Stage 0 compiler/runtime/VM/tooling are portable C17 | implementation brief §§3–6 | I0–I9 |
| IMPL-002 | compiler phases have explicit ownership and structured diagnostics | implementation brief §§6–7 | I2 |
| IMPL-003 | unoptimized MIR oracle precedes backend optimization claims | implementation brief §7.9 | I2 |
| IMPL-004 | C backend and runtime sanitizer suites cover generated programs | brief §8; test strategy | I4–P3 |
| IMPL-005 | package/verifier parsers treat input as hostile and bounded | brief §§6.3,11 | I3/I7 |
| IMPL-006 | compiler semantic facts remain reusable by tools while authorship, provenance, coverage, review, and release trust policy stay outside source semantics | D034; principles §16; brief §3 | I2/I8/P3 |
| BOOT-001 | Stage 0 builds Stage 1; Stage 1 builds Stage 2 | brief §12; roadmap I10 | I10 |
| BOOT-002 | normalized Stage 1/2 artifacts and conformance are reproducible | roadmap I10; tests §8 | I10 |
| BOOT-003 | maintained C17 root of trust remains available | brief §12; D009 | I10/P4 |
| PROD-001 | claimed Tier 1 platforms pass full runtime/tool/installer matrix | tests §11; roadmap P1 | P1 |
| PROD-002 | compatibility, fuzz, sanitizer, supply-chain, and performance budgets gate release | roadmap P1/P3; tests | P3 |
| PROD-003 | reference service exercises public HTTP/TLS/database/tasks/isolate/ops APIs | brief §14; roadmap P2 | P2 |
| PROD-004 | reference service survives specified load and failure injection with bounded cleanup | roadmap P2 | P2 |
| PROD-005 | independent completion audit maps every requirement to direct evidence | roadmap P4 | P4 |
| PROD-006 | external I4–I6 ergonomics pilots close semantic blockers before I10 self-hosting begins | roadmap E0; brief §12 | E0/I10 |
| PROD-007 | no public preview/alpha, registry namespace, package publication, or signing identity ships under a provisional naming decision | naming audit; roadmap N0 | N0/P0 |
