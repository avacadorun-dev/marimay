# Stage 0 architecture checkpoint 1 — 2026-08-13

Gate impact: no gate state changed. This is a behavior-neutral implementation
architecture and refactoring plan supporting the active I2–I5 work.

Baseline: `a9ca87c` exists and is an ancestor of the current `main` history.
Observed HEAD before this checkpoint: `49eb5a7` (`feat: establish managed array
runtime`). The working tree already contained an uncommitted managed-array MIR
slice in `bootstrap/include/ruda/mir/mir.h` and
`bootstrap/src/mir/mir.c`; this checkpoint did not discard or include it.

## Delivered

- Recorded primary-source compiler architecture research separately from Ruda
  normative semantics.
- Defined the Stage 0 phase/ownership map and allowed dependencies.
- Reaffirmed typed verified MIR as the only VM/native convergence point.
- Defined verifier boundaries, query-shaped but non-persistent Stage 0 APIs,
  and the shared runtime semantics boundary.
- Inventoried current large-file responsibilities and specified a low-risk
  extraction order for tests, MIR, and checker code.
- Defined an extraction contract that forbids combining mechanical moves with
  language feature changes.

## Baseline finding

A fresh incremental compile of the pre-existing working tree failed because
new `RUDA_MIR_ARRAY_*` enum values were not handled in all exhaustive switches
in the MIR interpreter, bytecode lowering, and C emitter. A previously built
`ruda_c_unit` binary still passed, so that test result is not accepted as
evidence for the current sources. The managed-array slice must restore a clean
build and add focused tests before it is committed.

## Verification commands

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-doc-links` | 0 | local documentation links valid |
| `./tools/check-spec-terms` | 0 | normative names and terms consistent |
| `./tools/check-example-syntax` | 0 | public example fences pass the current syntax checks |
| `./tools/check-syntax-coverage` | 0 | 74/74 syntax kinds and 115/115 grammar productions tracked as covered |
| `./tools/check-generated` | 0 | bytecode, diagnostic, Unicode, and syntax generated artifacts current |
| `git diff --check` | 0 | no whitespace errors |

No I2, I3, I4, or I5 gate is marked verified by this documentation
checkpoint. The C build failure described above remains assigned to the active
managed-array MIR checkpoint rather than being hidden by stale test output.
