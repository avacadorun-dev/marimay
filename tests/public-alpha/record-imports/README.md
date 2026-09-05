# Imported record regression

This focused companion to the CLI notes workload tests D004 record
construction through namespace imports, renamed imports, local/exported
aliases, expected generic types, Unicode and field shorthand. The deliberately
reordered initializers assert source-order evaluation. Identity checks cover
same and distinct imported records, including generic and empty records.

`mmay_record_imports` in CTest runs both VM and native engines. The checker
companion `test_checker_record_imports.c` covers private types/fields, missing,
duplicate and unknown fields, generic type errors, and every deterministic
checking allocation-failure index through successful teardown.

```sh
build/debug/mmay fmt tests/public-alpha/record-imports --check
ctest --test-dir build/debug -R '^(mmay_c_unit_checker|mmay_record_imports)$' --output-on-failure
```
