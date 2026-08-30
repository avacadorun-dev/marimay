# Checkpoint 37: first-party CLI parser

Date: 2026-08-30.

Status: verified local checkpoint evidence.

Verified subject: the local CP37 working tree that adds ordinary `std/cli`
plus the owner-layer bytecode VM repair required by the saved fuzz
regression. The user separately authorizes any checkpoint commit and push.
No tag, publication, external identity operation, parent product-gate claim,
or cross-platform claim is authorized by this report.

Host: macOS arm64, CMake 4.4.2, Apple Clang.

## Authority and delivered contract

Accepted
[D081](../../design/decisions.md#d081-bounded-declarative-first-party-command-line-parsing)
and the corresponding
[standard-library](../../spec/standard-library.md) parser section define this
boundary:

- `std/cli` is an ordinary first-party package over shipped strings/text,
  arrays, `Map`/`Set`, `Option`, and `Result`;
- a parser declares one program name, summary, ordered options, ordered
  positionals, and explicit limits;
- long/short flags, required and optional values, attached `=`, clustered
  shorts, `--`, Unicode names, typed definition/usage errors, and
  deterministic byte-width help are public;
- recommended usage exit is 2; managed-budget failure remains `I0002`;
- the C17 toolchain embeds the three source documents and does not recognize
  parser symbols or reconstruct parsing in host C.

Subcommands, aliases beyond one long/short pair, completion, environment
defaults, and display-width help remain absent rather than stubbed.

## Implementation and ownership

`std/cli/cli.my` owns the public model, factories, definition validation, and
stable messages. `std/cli/parse.my` owns argument consumption and typed usage
errors. `std/cli/help.my` owns deterministic bounded rendering.
`cli_standard_cli_package.c` is the narrow three-document embedding owner.

The D081 workload also closed three pre-existing generic-nominal defects in
their existing owners: erased-void wildcard binding, recursive imported
nominal layout closure over nested Map/Set fields, and codec admission of
every verified register type in nominal layouts.

A saved local bytecode-fuzz input decoded, verified, and round-tripped the
codec, then aborted the call-free run-status contract at
`tests/fuzz/bytecode_fuzz.c:288`. The production defect was not a codec
mismatch: `FS_CLOSE_BEST_EFFORT` is a void opcode, and `vm_fs.c` interned a
Result layout before dispatching it, so the VM returned
`MMAY_BYTECODE_RUN_INVALID_BYTECODE` for verified bytecode. The owner repair
dispatches that opcode before Result-layout intern. Focused coverage lives in
`tests/c-unit/test_cp34_fs_pipeline.c` and
`tests/fuzz/corpus/bytecode/cp37-close-best-effort`.

## Test inventory and exact observations

Focused public and C tests cover empty arguments, clustered shorts, attached
and separated values, negative numeric values, optional values that do not
consume the next argument, `--`, Unicode names, typed usage and definition
errors, generated flag cases, deterministic help, heap-limit VM/native
parity, and every standard-package embedding allocation-failure index with
zero-counter teardown.

Public commands:

```sh
build/debug/mmay fmt tests/public-alpha/cp37-cli-parser --check
build/debug/mmay check tests/public-alpha/cp37-cli-parser
build/debug/mmay test tests/public-alpha/cp37-cli-parser --engine both
build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --help
build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --count 3 input.txt
```

Help begins `Usage: cp37-demo [options] [<input>] [<extra>...]`. The count
path prints `count=3 input=input.txt`. Public package tests passed 12/12
under both engines.

The saved fuzz input now replays without abort. The applicable ASan/UBSan
bytecode libFuzzer campaign completed its required 600-second threshold
without a crash, timeout, sanitizer report, or new saved failure artifact:
5,149,227 executions in 601 seconds, final `cov: 7732`, `ft: 27015`, and an
in-memory corpus of 1,902 inputs / 134 KiB.

## Validation

| Command | Result |
|---|---|
| CP37 public `fmt --check`, `check`, `run --help`, `run --count`, `test --engine both` | passed; 12/12 public tests |
| `ctest --preset debug --output-on-failure` | 40/40 passed in 354.39 s |
| `ctest --preset release --output-on-failure` | 40/40 passed in 249.35 s |
| `ctest --preset asan-ubsan --output-on-failure` | 40/40 passed in 1187.92 s |
| roadmap, architecture, C-memory-policy, generated-file, doc-link, spec-term, example, syntax-coverage, and diff checks | passed |

Final artifact SHA-256 values:

- Debug compiler: `cd1cc36594a61623c63b7094bfe354f6f64441bcfd2c8eabc50755d0c778fad4`;
- Release compiler: `585deb28ef12ac1e52aa763eb3ad2e9b2e5102bac18aa84a8ac171d9ba4ae919`;
- ASan/UBSan compiler: `078c4b8b09f84e08b3a081f8912bf9011521d8d6567f95730ee5a5a2ee00683c`.

## Validation scope and remaining limitations

This is local macOS arm64 evidence only. Apple sanitizer success does not
substitute for Linux LeakSanitizer or the complete claimed Tier 1 matrix.
Subcommands and completion remain absent. Typed JSON remains CP38.

CP37 therefore verifies only its local checkpoint scope. CLI0 and all parent
product gates retain their prior states. CP38 becomes the sole ready
checkpoint.
