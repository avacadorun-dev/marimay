# I2 algebraic enum declarations — iteration 15 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement deterministic, bounded nominal enum declarations and owned
unit/positional/named payload metadata under the existing D004/D025 contract.

## Existing normative contract implemented

- Language §5.5 defines enums as closed nominal sums with unit, positional, and
  named payload variants.
- D004 keeps declarations nominal even when every variant and payload type is
  identical. D025 keeps named payload declarations semicolon-terminated.
- Public enum payload types are part of the public package surface and therefore
  obey language §3 visibility. Private enums may use private payload types.
- This iteration changes no syntax, tag layout, construction, pattern, or
  runtime semantics and adds no design decision.

## Implemented

- Supported non-generic enum names join aliases and records in the canonical
  source-path/declaration-order predeclaration pass. A stable declaration symbol
  interns one `RUDA_TYPE_KIND_NOMINAL` identity before any payload or body is
  resolved.
- Aliases may target later enums. Record fields and enum payloads may form
  self-recursive, forward, and mutually recursive nominal graphs without
  recursive type expansion.
- Checked enum metadata owns the enum name/symbol/type/visibility, ordered
  variants, variant kind, ordered payload types, and names for named payload
  items. Positional items explicitly carry no synthetic name.
- Duplicate enum, variant, and named-payload field names retain `R0002` at the
  repeated declaration. Unknown payload types retain `T0001`; private payloads
  in public enums retain iteration 14's `R0003`.
- `max_enums`, `max_variants_per_enum`, and
  `max_payload_items_per_variant` bound syntax-driven work even when invalid
  duplicate items are not published.
- Nested enum/variant/payload ownership transfer is failure-atomic under the
  shared allocator-injection suite. Generic enums and declaration attributes
  remain explicit incomplete boundaries.

## Coverage added

- One checked enum exercises unit, multi-item positional, named D025 payload,
  and self-recursive variants. Assertions pin kind, order, names, exact types,
  visibility, descriptor symbol, and function/alias type reuse.
- A second enum with the same variants/payload types proves distinct nominal
  symbol and type identity.
- Mutual `record`/`enum` fixtures prove payload-to-record and field-to-enum
  resolution. Reversed caller-supplied multi-file order proves stable enum
  symbol/type identity.
- Negative fixtures cover duplicate variants, duplicate named payload fields,
  unknown payload types, primitive redefinition, cross-kind top-level
  duplicates, public/private payload visibility, generic/attribute boundaries,
  and every new work limit.
- Single-file and package allocator-failure programs now include enum payload
  ownership and require zero live allocations after every injected failure.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker and checker-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | all committed generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2/I3 work

The source checker does not yet accept qualified enum construction, payload
projection, enum patterns, or `match`; no usefulness or exhaustiveness result is
claimed. Typed MIR has no enum construction/tag/projection instructions yet.

The compiler-owned tag layout, bytecode encoding, runtime values, stack traces,
and VM/native differential behavior remain I3/I4 work. Declaration metadata is
an analysis fact for later checker/editor/package consumers, not evidence that
enum programs can execute and not a trust or policy mechanism.
