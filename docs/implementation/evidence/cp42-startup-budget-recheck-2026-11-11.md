# CP42 startup budgets re-checked: two rows pass, the third measures the platform

Local macOS arm64 evidence, 2026-11-11, parent `f2c9bf0`.
Written by hand from the two applications' own measurement harnesses; raw
reports stay in each application's `build/measure-*/report.json`.

CP42 has been open on three startup rows recorded on 2026-09-09:

| Row | Recorded 2026-09-09 | Budget |
|---|---|---|
| First native execution | 392 / 387 ms | 100 ms |
| Source-lint small VM run | 500-532 ms | 500 ms |
| Source-lint warm `test --engine both` | 5.506 s | 2 s |

Those numbers were never re-measured. They are now, at the release compiler, by
running each application's own `tools/measure.py` against the budgets the
application defined before measurement.

## Environment

| Field | Value |
|---|---|
| Compiler | release build, SHA-256 `4bf288523efa4180d73042964138748bf83b78642afdd89d90a0792bd9088cf8` |
| Version | `0.1.0-alpha.3` |
| Host | macOS 27.0.1 arm64 |
| Budgets source | each application's `BUDGETS.md` as measured, SHA-256 `b5b0dc2c5a97b8b3`;
both files now state the D125 rule (`marimay-delimited` `6bf9d9c`, `marimay-source-lint`
`1df3c23`), so a re-run measures the new text |
| Harness | `tools/measure.py` of each application |

## Results

`marimay-delimited`:

| Measurement | Seconds | Budget | Within |
|---|---:|---:|---|
| small native, first execution | 0.2461 | 0.10 | **no** |
| small native, repeats | 0.0075 / 0.0052 / 0.0050 | 0.10 | yes |
| small VM, four runs | 0.1555 / 0.1616 / 0.1581 / 0.1536 | 0.50 | yes |
| warm `test --engine both` | 0.436 | 2.00 | yes |

`marimay-source-lint`:

| Measurement | Seconds | Budget | Within |
|---|---:|---:|---|
| small native, first execution | 0.2469 | 0.10 | **no** |
| small native, repeats | 0.0108 / 0.0078 / 0.0070 | 0.10 | yes |
| small VM, four runs | 0.2211 / 0.2122 / 0.2260 / 0.2132 | 0.50 | yes |
| warm `test --engine both` | 1.856 | 2.00 | yes |
| stateful-512k native / VM | from the same run, both within their 2 s / 20 s budgets | | yes |

So: the VM-run row and the warm test-loop row **pass** at this revision, with
roughly 2.4× and 3.0× headroom respectively. The first-native row is the only
one left, and it is not what it looks like.

## Why the first-native row fails

`int main(void) { return 0; }`, compiled with the same host compiler, measures
**0.24 s on its first execution** after the link and `0.00 s` afterwards. The
first execution of a freshly linked Mach-O binary pays page-in and platform
code-signing validation before `main` runs; the cost is the platform's, not the
program's.

Both applications measure 0.246-0.247 s on that first run — within 10 ms of the
trivial baseline — and 5-11 ms on every subsequent run. In other words, Marimay's
own startup cost is about 6 ms, and the 100 ms budget was being compared against
a platform event it does not control.

## Disposition

D125 redefines the row as application startup rather than platform first-exec:
the acceptance number is the repeated execution (≤100 ms, measured at 5-11 ms),
and the first execution is reported for information with the same-host trivial
baseline beside it. The recorded floor is 246 ms first execution against a
240 ms trivial baseline.

## What this does not prove

- One host, one compiler revision, no OS cache eviction and no fresh-host claim.
- This is a re-measurement, not a paired A/B against the 2026-09-09 numbers: the
  compiler changed in between, so the improvement cannot be attributed to one
  change.
- No Linux, Windows, x86_64 or Tier 1 evidence; no sanitizer run; the VM
  stateful-512k figure above is reported from the same harness run and is not a
  full performance claim.
