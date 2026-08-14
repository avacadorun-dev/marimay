# Normative readability examples

These examples are executable specification fixtures once the parser exists. Until then they are reviewed manually and by the documentation consistency checks.

- `tour.ruda` exercises records, algebraic enums, exhaustive match, first-parameter dot calls, structural interfaces, deterministic `f64` arithmetic, and the absence of `impl`.
- `concurrency.ruda` exercises direct-style I/O, `Result`, explicit error mapping, scoped `spawn`, and cancellation by early `?`.
- `service.ruda` sketches a production HTTP handler without async-colored signatures.
- `tour_test.ruda` shows the built-in test syntax.
- `matrix.ruda` is the first source-checkable D045 array kernel: a loop-based
  2-by-3 times 3-by-2 `f64` multiplication whose checksum is `415.0`. Under
  D046 it writes that checksum with `println(formatF64(...))?`, which must
  publish `415.0\n` on VM, native, and test capture once `std/io` is
  implemented. Engine writes remain a later checkpoint.

An implementation change that makes an example invalid must update the normative spec and corresponding decision in the same change. After I2, all examples are compiled by `ruda check`; after I6, they run under `ruda test --engine both` with deterministic service fixtures.
