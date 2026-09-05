# Qualified enum patterns

D084 clarifies the existing qualified-identifier grammar: `Enum.Variant` and
`namespace.Enum.Variant` patterns identify the exact matched nominal enum.
The contextual `namespace.Variant` spelling remains supported. Concrete type
aliases must retain their complete generic instantiation.

Before this correction, a three-component pattern such as
`failure.Wrapped.Input(value)` could exit with internal status 70. The focused
`checker_enum_pattern_names.c` owner now resolves qualifiers before pattern
analysis and nested binding compilation. Qualified nested unit variants are
constructor tests, not local bindings. Unknown, private, overlong, shadowed,
and wrong-nominal qualifiers produce source diagnostics.

The focused checker suite covers accepted spellings, alias instantiation,
visibility, diagnostics, redundancy and missing alternatives. Every case
replays all allocation indices measured on its baseline and asserts both
`live_allocations == 0` and `live_bytes == 0` after full teardown. The public
`enum-pattern-names` module exercises unit, positional and named variants,
nested constructors, guards, alternatives, imports and aliases on both engines
and is registered as a formatter fixture.

## Local verification

macOS arm64 / AppleClang 21.0.0:

```sh
ctest --test-dir build/asan-ubsan -R '^(mmay_c_unit_checker|mmay_enum_pattern_names|mmay_format_enum-pattern-names)$' --output-on-failure --parallel 3
# 3/3 passed, 44.39 seconds
ctest --test-dir build/release -R '^(mmay_c_unit_(checker|bytecode|native)|mmay_enum_pattern_names|mmay_format_enum-pattern-names|mmay_map_error_imports|mmay_record_imports)$' --output-on-failure --parallel 4
# 7/7 passed, 213.82 seconds
```

Architecture, C memory policy, generated artifacts, roadmap and whitespace
checks also pass. These results do not advance a checkpoint or establish
Tier 1 acceptance. Separately, the previous main commit `6c2943d` completed
its full local Release suite: 58/58 groups in 411.05 seconds. That earlier
full run does not include this correction.

## Remaining nested exhaustiveness defect

For `Result<Choice<int>, int>` where `Choice` has `Empty`, `Value(T)` and
`Named { value: T }`, the arms below still produce T0013 with witness `Ok(_)`:

```text
Ok(model.Choice.Empty) => 1,
Ok(model.Choice.Value(item)) => item + 2,
Ok(model.Choice.Named { value: item }) => item + 3,
Err(error) => -error,
```

The current coverage analysis treats nested constructors conservatively. The
runtime fixture therefore has an explicit `Ok(_)` remainder arm; its tests
prove that each known value selects the preceding intended arm. Correcting
qualifier resolution does not resolve this pre-existing exhaustiveness defect.
It remains required follow-up, not evidence that nested matching is complete.
