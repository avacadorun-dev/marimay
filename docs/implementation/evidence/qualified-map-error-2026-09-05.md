# Qualified `mapError` and notes ergonomics

This corrects ordinary imported-function resolution under the accepted D043
and D069 decisions. It adds no implicit error conversion, first-class function
representation, syntax, or new JSON codec policy.

## Correction

On compiler main `01aec63307be70136b5375fd32a0091de11fb0f1`, the new
`tests/public-alpha/map-error-imports` fixture fails with T0002:
`mapError enum mapper must be a positional constructor with one payload`.
Both `json.message` and an application package's `failure.message` are
incorrectly classified as enum constructors.

The mapper owner now recognizes file-local namespace imports before enum
interpretation and delegates qualified function lookup to the existing
resolver. It preserves unary exact-input checking and records the function
occurrence. Private and unknown imports retain their resolver diagnostics;
lookup never falls back to a same-named local function. Named aliases and
qualified positional enum constructors remain covered.

`test_checker_map_error_imports.c` covers these successes plus private, unknown,
wrong-parameter, wrong-arity and non-function names. Each case measures a
baseline allocation trace and injects failure at every allocation index. Full
teardown always asserts zero live allocations and zero live bytes.

The process fixture verifies real `std/json` error mapping with postfix `?`,
including `Result<void, json.Error>`, and an observable counter proving that
the mapper runs once on `Err` and never on `Ok`. Both VM and native execute it.

## Application exercise

The notes model uses one `required<T>` for integer, string and array fields,
`decodeJson(contents).mapError(json.message)?`, and one match per field-loop
iteration. The unreachable field-presence panic branches are gone. The model
is 243 lines instead of 271; required-field error ordering, JSON format,
reference identity and add/remove behavior are unchanged. Missing-field
coverage includes all six database/note fields.

The standalone `marimay-notes` repository receives the same model and test
changes. Its seven tests pass with the corrected Release compiler and
`--engine both`; canonical formatting passes. The ASan/UBSan compiler also
passed the seven-test model before the extra missing-field assertions, on
both engines.

Local macOS arm64 / AppleClang 21.0.0 results:

- ASan/UBSan checker plus mapper formatter: 2/2 CTest groups pass, 8.16 seconds.
- ASan/UBSan mapper process fixture: 2 source tests pass on both engines.
- Release mapper process fixture: passes, 12.41 seconds.
- Release updated notes-model process fixture: passes, 29.61 seconds.
- The complete focused Release command below passes 8/8 groups in 199.28
  seconds, including native differential and bytecode suites:

  ```sh
  ctest --test-dir build/release -R '^(mmay_c_unit_(checker|bytecode|native)|mmay_generic_lifetime|mmay_map_error_imports|mmay_format_(generic-lifetime|map-error-imports)|mmay_cli_notes_model)$' --output-on-failure --parallel 4
  ```
- The final standalone notes sources, including every missing-field assertion,
  pass all seven tests with the ASan/UBSan compiler and `--engine vm`.

These results do not advance a checkpoint or public-alpha acceptance gate;
Linux LeakSanitizer and the remaining Tier 1 CI matrix are still required.

## Separate follow-up found during testing

A match arm spelling `failure.Wrapped.Input(value)` currently exits with
internal checker status 70 instead of a source diagnostic; nesting it inside
`Err(...)` also fails. A debugger stops in
`mmay_checker_private_enum_match_name`, called by positional-pattern analysis.
This concerns qualified constructor **patterns**, not the corrected mapper
expression. It remains an explicit frontend follow-up; this change does not
claim that all enum-pattern or ergonomics defects have been resolved.
