# CP42 notes migration and startup convergence: local follow-up

Scope: local working-tree evidence, not checkpoint or release verification.
Compiler base: `19cfe17d4715c1bb958a11bee8532819ea050cce`, branch
`feature/typed-json-ergonomics`. Notes base: `08a6576`, separate sibling
`marimay-notes` repository. Host: macOS arm64, AppleClang 21, portable C17,
Debug compiler. This follows the
[initial JSON slice](cp42-typed-json-first-slice-2026-09-05.md), preserving its
record of the initial entry-signature fixture failure.

## JSON and application observation

The typed-JSON standalone fixture now has a valid int-status entry wrapper.
Its public CTest exercises the actual source `std/json`, four both-engine
tests, canonical Unicode output, debug/release native executables, and exact
VM/native error/status parity on hostile inputs. The signature-only checker
fixture remains a separate allocation/typing test, not a JSON parser oracle.

The sibling notes model uses `json.decode<Database>(contents)` and
`json.encode(database)` instead of manual field readers/writers. Existing
JSON field names, required/unknown/duplicate-field checks and domain validation
remain. CLI help, domain/usage/JSON/filesystem/stream error messages and the
README are Russian; ASCII command/flag names remain compatible.

From the sibling notes directory:

```sh
MMAY=../marimay-json-ergonomics/build/debug/mmay
"$MMAY" fmt . --check
"$MMAY" check .
"$MMAY" test . --engine both
sh tools/smoke-ru "$MMAY"
```

All ten notes tests passed in both engines. The process smoke test also passed
in VM and release native: add/list/show/remove, non-reused IDs, empty text,
bad IDs/options, Russian text/tags/filename, and identical persisted JSON.
Smoke files live in an isolated `build/smoke-ru.*` directory; the user's base
was not used. A VM read without a filesystem grant reported the expected
Russian capability-denied message. `fmt`, `check`, and `git diff --check` passed.

## Startup defect and repair

The user reported that an interactive notes `run` was excessively slow.
After stopping the agent's concurrent long checks, local observations were:

| Command from notes directory | Before repair | After repair |
|---|---:|---:|
| Debug compiler `check .` | 0.13 s | not rebaselined |
| Debug compiler `run . -- --help` | 26.18 s | 0.54 s; subsequent 0.59 s |
| Already-built native `--help` / `list` | below 0.01 s timer resolution | unchanged execution path |

These are exploratory local wall-clock observations, not a D056 compatible
multi-platform distribution or a CI performance baseline. Background load and
sample counts do not support an exact universal speedup claim. They do show
that the delay precedes execution and is not proportional to database size.

A three-second stack sample placed 2,430 of 2,431 main-thread samples in
`mmay_mir_optimize_integer_ranges`. Temporary per-function probes identified
many ordinary loops reaching nearly 100,000,000 analysis work units during
the narrowing phase, returning `LIMIT_REACHED` and retaining checked code.
The small early-return `find` source in `tests/public-alpha/run-startup`
reproduced this independently of JSON. All temporary probes were removed.

D062 narrowing now replaces type-extreme loop-header endpoints but retains
already finite endpoints when another tightening would count through an
integer interval. A recomputed expansion is never suppressed. The normal
budget, transactional fallback, overflow semantics and independent MIR/wire
proof reconstruction remain. No cache or blanket optimizer bypass was added;
`run`, `build` and `test` share this repair.

The first broad run found a related CP37 bytecode-adapter defect: a void call's
call-table index was incorrectly interpreted as a destination register. The
adapter now consults the existing opcode-effects owner before killing scalar
facts. The CP37 process suite passed after the correction, including under
ASan/UBSan. A focused source test includes more void calls than registers and
checks their observable effects in both engines.

## Regression and cleanup coverage

`test_mir_range_convergence.c` checks forward search with early return, shallow
array copying, reverse search and nested loops under 16,384 work units. It
requires successful optimization and actual proven induction arithmetic, not
just an early fallback. Every deterministic optimizer allocation-failure index
is exercised, the surviving MIR is verified, and full teardown checks both
`live_allocations == 0` and `live_bytes == 0`. Existing interval, MIR, hostile
bytecode, overflow/panic and native differential suites also passed locally.

```sh
cmake --build --preset debug -j 4
ctest --test-dir build/debug --output-on-failure -j 4
cmake --build --preset asan-ubsan -j 4
ctest --test-dir build/asan-ubsan \
  -R '^(mmay_c_unit_runtime|mmay_c_unit_bytecode|mmay_cp37_cli_parser|mmay_run_startup)$' \
  --output-on-failure -j 2
```

The final four-test ASan/UBSan selection passed, including three public
both-engine startup regressions. An earlier six-test sanitizer selection also
passed the frontend, runtime, bytecode, typed-JSON process and all-index CP42
checker tests. The final complete Debug rerun passed all 67 CTests in 306.41
seconds. In that run the typed-JSON process suite took 36.00 seconds and the
native C-unit shard took 228.63 seconds; the latter includes many separate
host-C compilations and is not an isolated VM/startup measurement.

The architecture, C memory policy, spec terms, documentation links, example
syntax and generated-artifact gates passed. The roadmap gate passed after
linking this follow-up; CP42 remains the sole in-progress checkpoint.

This report does not establish Linux LeakSanitizer or complete Tier 1 evidence,
close CP42/CLI0, claim a sustained fuzz campaign, or publish an artifact.
Structured JSON field/index paths and intrinsic completion metadata still
need completion; collection helpers, mutation return contracts, Option control
flow and a consistent notes mutation API remain subsequent CP42 work.
