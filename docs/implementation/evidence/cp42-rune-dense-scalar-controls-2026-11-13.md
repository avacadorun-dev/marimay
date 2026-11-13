# Controls for the rune stride clause

Local macOS arm64 controls measured 2026-11-13 after
[the rune scalar record](cp42-rune-dense-scalar-2026-11-13.md) shipped as
`5fceb8a`. The shipped record is not edited; this note supplies the control data
that the #26 exit rule asks for.

## Why a second pass

The first paired corpus pass put four rows the mechanism cannot reach above the
2% median threshold: `catalog-interface` +3.72%, `set-string-churn` +3.61%,
`report-builder` +2.72% and `stack` +2.24%. Two of those exceed a 2% threshold
and none of them iterates a `[]rune`, so the question was noise or a real
code-layout effect.

## Same-binary controls

Six rows re-measured from one binary against itself, three warmups and eleven
alternating samples:

| Row | A median ms | A' median ms | A/A delta |
|---|---:|---:|---:|
| `catalog-interface` | 11.41 | 11.51 | +0.88% |
| `set-string-churn` | 31.71 | 31.69 | −0.06% |
| `report-builder` | 11.84 | 11.56 | −2.30% |
| `stack` | 9.82 | 9.90 | +0.83% |
| `rune-frequency` | 19.52 | 19.53 | +0.07% |
| `map-int` | 7.13 | 7.07 | −0.86% |

A binary moving 2.3% against itself on a 12 ms row bounds what a single pass can
call a regression at this scale.

## Repeated paired pass

The same four flagged rows re-paired from the two pinned binary sets:

| Row | First pass | Repeated pass |
|---|---:|---:|
| `catalog-interface` | +3.72% | −0.04% |
| `set-string-churn` | +3.61% | −0.30% |
| `report-builder` | +2.72% | +0.89% |
| `stack` | +2.24% | −0.45% |

No flagged row repeats, and three of the four change sign. The clause also cannot
reach those rows by construction: the only new input case in
`mmay_native_c_dense_scalar_stride` is `MMAY_TYPE_RUNE`, so a program without a
`[]rune` takes the same decision at every call site and emits the same element
access code.

## What this adds

`rune-frequency` keeps its paired improvement (−9.94%) and the other 31 rows have
no confirmed regression above the rule's threshold. `frequency-map` and
`unique-set` remain open, and `word-frequency` remains above its 2.0× Go target.
