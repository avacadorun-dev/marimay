# Generic declaration lifetime regression

Scope: implementation correction to existing D013 generic calls and function
publication. No source syntax, inference rule, runtime representation, public
alpha gate, or checkpoint status changes.

## Reproduction and cause

The notes JSON workload motivated a single `required<T>` helper for integer,
string and string-array fields. On compiler main
`01aec63307be70136b5375fd32a0091de11fb0f1`, an ASan/UBSan `mmay test --engine vm`
probe aborts with heap-use-after-free in function body checking. The committed
`tests/public-alpha/generic-lifetime` regression also reproduces invalid active
type-binding access on that compiler (exit 134 on macOS arm64).

Specialization appends declarations to a relocatable vector. The body checker
retained both an element pointer and pointers to that element's type-binding
vector descriptors. A second independent borrow crossed argument compilation:
`forward(identity(value))` retained the outer generic declaration while the
inner call could relocate it. The expanded regression reproduced this second
ASan failure even with the body-only correction in place.

The body now borrows a stack-stable metadata snapshot, with ownership retained
in the indexed table entry. Publication reacquires that entry before moving its
name. Generic call inference likewise reacquires its base by index after
argument compilation. Neither fix reserves excess capacity to hide relocation.

## Coverage and local results

Platform: macOS arm64, AppleClang 21.0.0, C17, warnings as errors.

- `test_checker_generic_lifetime.c` checks repeated vector growth, nested
  specializations, use of `T` after specialization, published names/test flags,
  implicit void return, explicit return, missing-return diagnostics and type
  diagnostics. Each source case first measures its allocation trace, then
  injects failure at every index. Every complete teardown asserts both
  `live_allocations == 0` and `live_bytes == 0`.
- `ctest --test-dir build/asan-ubsan -R
  '^(mmay_c_unit_checker|mmay_generic_lifetime|mmay_format_generic-lifetime)$'
  --output-on-failure --parallel 3`: 3/3 passed, 61.22 seconds.
- `ctest --test-dir build/release -R
  '^(mmay_c_unit_checker|mmay_generic_lifetime|mmay_format_generic-lifetime|mmay_cli_notes_model)$'
  --output-on-failure --parallel 3`: 4/4 passed, 36.52 seconds.
- The process regression runs all three source tests on both VM and native;
  it includes real `std/json` generic required fields and preservation of later
  test names/bodies. The existing notes model regression also uses both engines.
- The original standalone ergonomics probe passes all three tests with the
  corrected ASan/UBSan compiler and `--engine both`.

This is local regression evidence, not a claim of complete sanitizer coverage
or Tier 1 verification. Linux LeakSanitizer and the remaining CI matrix are
still required. The unrelated `mapError(json.message)` resolution defect and
proposed syntax/JSON ergonomics improvements are outside this correction.
