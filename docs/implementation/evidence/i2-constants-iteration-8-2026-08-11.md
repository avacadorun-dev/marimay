# I2 bounded top-level constants — iteration 8 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement the pure `bool`/`i64` portion of the existing top-level
constant contract without introducing runtime package initialization or
claiming the remaining constant-expression forms.

## Implemented

- Top-level `const` declarations join functions in one exact package namespace;
  duplicate function/constant names are diagnosed uniformly. The indexed name
  entry is tagged, so calling a known constant reports a type mismatch rather
  than a misleading unknown-name error.
- Explicit `bool`/`i64` (including `int`) declarations and inferred
  `bool`/`i64` constants are supported. Constant initializers may use supported
  literals, unary and checked arithmetic operations, comparisons/equality,
  parentheses, and other package constants.
- Constant dependencies resolve across declaration order and source files.
  Evaluation state detects cycles; canonical source order determines published
  constant order while path/name-derived 128-bit symbol IDs remain independent
  of caller AST order.
- Initializers lower through the same typed MIR expression path used by
  functions, are verified, and execute in the MIR oracle. Checked overflow and
  division by zero therefore cannot be folded with host-C behavior; they
  produce stable `T0010 CONSTANT_EVALUATION` diagnostics.
- The evaluator has explicit package-wide step, dependency-depth, constant
  count, MIR work/register/instruction, and allocator bounds. It has no call
  resolver or ambient I/O/runtime state. Ordinary function calls receive
  `T0010`; compiler-designated pure constant functions remain unimplemented.
- Accepted constants are owned results containing their exact name, stable
  symbol, type, and value. Function lowering embeds their verified primitive
  values into MIR, including cross-file references.
- A diagnostic in any signature, constant, or body clears both constants and
  functions plus the function symbol index. The checker never exposes a
  partially authoritative package result.

## Coverage added

- Forward `const` dependencies infer `i64` and `bool`; a checked function reads
  the values and the program oracle returns the expected result.
- A function in one source reads a constant from another source while also
  making a cross-file function call. Reversed AST input preserves the constant
  and function symbols and executable result.
- Exact negative fixtures cover dependency cycles, checked overflow, division
  by zero, an ordinary call during constant evaluation, declared/inferred type
  mismatch, unknown dependency, function/constant duplicate, and calling a
  constant as a function.
- Dedicated options tests cover constant count, dependency depth, and shared
  evaluator step limits. Limit diagnostics leave no published constants.
- A body error after successful constant evaluation proves package artifact
  clearing. String constants explicitly return
  `RUDA_CHECK_INCOMPLETE_LANGUAGE` rather than a false acceptance.
- Single- and multi-source allocator-failure walks now include constant
  collection, indexing, recursive evaluation, MIR verification/interpretation,
  publication, cross-file reads, and cleanup. Every attempt ends with zero live
  allocations and bytes.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, type-table, and checker-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | `T0010` header/reference and all generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | all checks green; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2 gate work

This is not the complete constant-expression language. String concatenation,
immutable enum construction/payloads, compiler-designated pure constant
functions, richer primitive arithmetic, imported constants, public/private API
metadata, bounded trace notes, and incremental invalidation remain open.
Unsupported syntactically valid forms retain the explicit incomplete status.

The package namespace remains flat and unqualified; imports, visibility,
nominal types, overload-aware dot calls, generics, interfaces, closures,
effects, and the compiler service are still required. The public `ruda check`
command remains unavailable. No runtime initializer was added, and no source
syntax, language rule, package format, ABI, VM/native behavior, or D034 trust
boundary changed; this iteration implements an already normative pure subset.
