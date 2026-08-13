# Ruda repository rules

This repository defines and implements the Ruda programming language.

## Authority order

When documents disagree, use this order:

1. `docs/spec/` — normative language and toolchain contracts;
2. accepted entries in `docs/design/decisions.md`;
3. `IMPLEMENTATION-BRIEF.md` and `docs/implementation/`;
4. examples and README prose;
5. historical research notes.

Do not silently choose between conflicting normative statements. Record the conflict, add or amend a decision, update the specification, then implement it.

## Non-negotiable product constraints

- The command is `ruda`; the source extension is `.ruda`.
- Project files are `ruda.mod`, `ruda.work`, and `ruda.lock`; bytecode bundles use `.rudab`; tool-owned local state uses `.ruda/`.
- The former command `ma`, source extension `.ma`, `ma.*` project files, `.mab` bundles, and `.ma/` tool-state path are not current compatibility aliases.
- Stage 0 is portable C17. C++ is not permitted in the compiler or runtime.
- A later compiler is self-hosted in Ruda, but the C17 bootstrap remains reproducible.
- VM and native execution consume the same typed MIR and have the same observable semantics.
- `ruda run` and `ruda test` use the VM by default; `ruda build` creates a native executable by default.
- `ruda fmt` and `ruda test` are built in. A release cannot delegate their core behavior to third-party tools.
- Ruda has algebraic enums, exhaustive pattern matching, `Option`, `Result`, and postfix `?`.
- Ruda has no `null`, `undefined`, exceptions for expected failures, inheritance, user-defined implicit conversions, or `any` escape hatch.
- Ruda has no `impl` blocks and no special method declaration. Behavior is declared by ordinary top-level functions.
- A visible function owned by a package is dot-call eligible when its first parameter has an exact nominal type owned by that package: `string(user)` and `user.string()` resolve to one symbol.
- Interfaces are structural contracts over the resulting dot-call set; records and enums remain nominal.
- Direct-style task suspension is inferred by the compiler. `scope` owns child-task lifetimes and `spawn` creates concurrency. Public source code has no `async` function color and no selectable third-party runtime.
- Safe Ruda has no undefined behavior. Raw memory and C FFI are confined to explicit `unsafe` regions.

## Change protocol

Every semantic change must include, in the same change:

1. a decision record or link to an existing accepted decision;
2. normative spec text;
3. parser/type-checker/runtime tests as applicable;
4. VM/native differential tests for observable behavior;
5. formatter fixtures for new syntax;
6. updated user-facing examples if the feature is public.

Implementation convenience is not a reason to change source semantics. Temporary shortcuts must be isolated behind an internal interface, documented in the roadmap, and covered by a removal gate.

## Definition of done

A milestone is `verified` only when its documented commands pass on all supported Tier 1 platforms and the evidence is linked from `ROADMAP.md`. “Implemented” without conformance, negative, sanitizer, and differential coverage is not done.

## C memory ownership

Every C change that adds or modifies an allocation-capable API must cover
successful cleanup, all material error paths, and every reachable deterministic
allocation-failure index. After full teardown, allocator-backed tests must
assert both `live_allocations == 0` and `live_bytes == 0`.

Direct production heap calls outside the central allocator implementation are
forbidden and enforced by `tools/check-c-memory-policy`. Linux LeakSanitizer is
a required CI contract. Sanitizer suppressions require a documented reason and
review; neither sanitizer success nor allocator counters justify an unqualified
claim that all leaks are absent.
