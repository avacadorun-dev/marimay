# I0 memory-safety gates — iteration 4 — 2026-08-13

Gate: I0 repository/bootstrap foundation.
Iteration status: implemented locally; external CI evidence pending.
Baseline: `9bdf844` (`feat: add managed array MIR oracle`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered slice

- `Linux x86_64 LeakSanitizer` is a separately named pull-request/push job
  with `detect_leaks=1`, fail-fast ASan/UBSan settings, a pinned checkout
  action, and credentials disabled. Its stable name is suitable for a GitHub
  Ruleset required-status check.
- A scheduled/manual Ubuntu Valgrind workflow builds ordinary Debug C17 and
  fails on definite or indirect leaks in the parent C-unit process while still
  reporting all of that process's leak kinds. The job prints the
  Valgrind/compiler versions and tested commit SHA. Child tracing is deferred
  because the suite also launches system compilers.
- `tools/check-c-memory-policy` token-scans production `.c`, `.h`, and compiled
  `.inc` fragments after C line-splicing and removal of comments and
  string/character literals. Any raw heap identifier is rejected under
  `bootstrap/` and `runtime/`; only `malloc`, `realloc`, and `free` in the
  central allocator implementation are allowed. Built-in negative self-tests
  cover direct calls, function-pointer and macro aliases, token-pasted and
  line-spliced names, compiled `.inc` discovery, inert comments/literals, and
  the exact allowlist. Production token-paste operators are rejected unless a
  future use receives an exact reviewed allowlist.
- Repository instructions and the normative test strategy require successful
  cleanup, material error-path coverage, every reachable allocation-failure
  index, and zero `live_allocations` plus `live_bytes` after teardown.
- Existing base tests now check the missing byte balances, and allocator tests
  cover successful shrink accounting in addition to growth, failure, and
  release-to-zero.

The evidence language is intentionally bounded. Aggregated allocator counters
trust the caller-reported sizes and do not track pointer identity. macOS
ASan+UBSan does not establish the separate Linux LSan result, and none of these
checks proves behavior that the tests do not execute.

## Local verification

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-c-memory-policy` | 0 | production heap boundary clean |
| `cmake --build --preset debug --parallel 4 && ctest --preset debug --output-on-failure` | 0 | 1/1 passed in 33.39 s under concurrent validation load |
| `cmake --build --preset asan-ubsan --parallel 4 && ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed in 41.41 s without an ASan/UBSan report under concurrent validation load |
| `PYTHONPYCACHEPREFIX=/tmp/ruda-python-cache python3 -m py_compile tools/check-c-memory-policy` | 0 | policy checker syntax valid |
| Ruby standard-library YAML parse of both workflow files | 0 | workflow YAML structurally parseable |
| `./tools/check-generated` | 0 | generated artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation and syntax invariants passed |
| `git diff --check` | 0 | patch whitespace clean |

## Required external follow-up

- Obtain a green `Linux x86_64 LeakSanitizer` run for the committed SHA before
  calling this checkpoint leak-verified.
- Configure the `main` GitHub Ruleset to require exactly that status from
  GitHub Actions, without bot/agent bypass. Repository files cannot enforce
  this hosting setting.
- Obtain the first green scheduled or manual `Linux x86_64 Valgrind` run and
  link its immutable run evidence here.
- Extend Valgrind evidence to Ruda child processes without tracing external
  system compiler/linker processes or relying on broad suppressions.

I0 remains `in-progress`; this checkpoint does not claim universal absence of
memory leaks or promote any release gate to `verified`.
