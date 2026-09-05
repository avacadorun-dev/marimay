# Boolean-expression formatting boundaries

Local correction to D073, based on main `7418ae8`. This report accompanies the
formatter change; it does not advance a checkpoint or public-alpha gate.

## Failure and ownership

The former statement-wide continuation flag kept an indentation open from an
`&&`/`||` operator until the enclosing statement's semicolon. In a `return
match`, this indented subsequent arms and closing braces. It also broke short
chains merely because the enclosing match contained hard line breaks.

`formatter_groups.c` now owns CST-derived logical-expression boundaries and
statement/document group lifecycle. Direct logical binary children share one
continuation; parentheses and independent arguments, fields and arms have
separate groups. Indentation ends with the owning expression. Interpolation
remains unbreakable. The formatter coordinator is 1,180 lines, below its
1,200-line ceiling; there is no architecture exception.

A standalone comment after an ungrouped arm comma now retains its next-line
position. An actually inline comment stays inline. This uses the existing
source-newline predicate rather than reattaching every following comment to
the preceding arm.

## Coverage

`test_formatter_boolean_groups.c` owns five exact-output cases: short and long
match bodies, nested matches/block arms/guards, tuple and call arguments,
record fields, parenthesized chains, comments, conditions, and interpolation.
Every case verifies parse success, semantic AST equivalence, comment bytes
and order, and idempotence. It sweeps every allocation index from an unfailing
baseline and every document-part limit up to the first successful formatting;
full teardown asserts both live allocation and byte counters are zero.

The registered `match-format` public module exercises empty-array short
circuiting, following arms, guard call counts and nested matches on VM/native.
Eleven existing example/test source files were mechanically reformatted;
assertions, runtime comparisons and negative cases were not removed. A first
full run identified seven checks that still expected the old layout. After
updating their source fixtures and reconfiguring the copied test corpus, the
entire suite was rerun successfully, not just those seven checks.

The boolean-boundary seed is included in the existing formatter fuzz corpus.
Local sustained fuzzing was not run: this host's AppleClang lacks libFuzzer,
and `cmake --preset fuzz` rejects that configuration. No replacement compiler
was installed and no fuzzing success is claimed.

## Local verification

macOS arm64, AppleClang 21.0.0:

```sh
cmake --preset release
cmake --build --preset release -j 4
ctest --test-dir build/release --output-on-failure --parallel 4
# 63/63 passed, 556.51 seconds; CLI notes process suite: 356.48 seconds

env UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/asan-ubsan -R '^(mmay_c_unit_frontend|mmay_match_format|mmay_format_.*)$' --output-on-failure --parallel 2
# 9/9 passed, 42.00 seconds

env UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/asan-ubsan -R '^mmay_c_unit_frontend$' --output-on-failure
# Final expanded document-limit sweep: 1/1 passed, 1.04 seconds
```

Architecture, C memory policy, generated freshness, roadmap and whitespace
gates pass. The local CLI notes duration is not a performance benchmark or
resolution of its historical Linux timeout.

The last observed Linux queue still had run 179 (`6c2943d`) running and run
180 (`7418ae8`) waiting. This formatter candidate had not been pushed at that
observation. These local results do not substitute for Linux/Tier 1 evidence.
Source-level nested-pattern exhaustiveness is a separate unfinished change;
the formatter neither changes match acceptance nor resolves guard mutation.
