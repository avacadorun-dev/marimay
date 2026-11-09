# D118 coalesced-record A/B screening

Status: **local paired screening**, candidate commit on
`feature/perf-record-create` against baseline `e73eb64` (`main` at
`0.1.0-alpha.2`). The GitHub CI matrix is the authoritative validation;
this record covers the unchanged-corpus performance screening only. No
Tier 1 budget, reviewed baseline or parity status is claimed.

## Mechanism

Managed record construction previously made three allocator round
trips per record: the header, the field-cell array, and a
reference-root array, while D055 has always charged one coalesced
object size. Decision D118 allocates the header and field cells as one
block (`fields` points into it at `sizeof(MarimayManagedRecord)` with
identical lifetime and tracing) and collects root sets of up to eight
reference fields into a stack buffer; larger sets keep the heap path.
Up to three deterministic allocator calls fewer per record. Validation,
publication, byte accounting, failure-atomic cleanup, record views and
generated-code surfaces are unchanged.

## Method

Both compilers built as Release from identical trees except the
candidate commit (Apple M2, macOS arm64; corpus toolchain contract
adapted locally to `0.1.0-alpha.2`, identically for both sides). Six
short alternating rounds (base, patch, base, patch, …), four catalogue
rows per round — two target (record-create, catalog-pipeline) and two
control (record-reference-copy, map-string) — 11 samples after 3
warmups per run. Every run reported `Status: passed`; checksums
identical across languages, rounds and both compiler sides. Reported
figures are the median of the six per-round patch/base medians.

## Interleaved per-round ratios (Marimay native, patch/base)

| Workload | Round ratios | Median | Reading |
|---|---|---:|---|
| record-create (target) | 0.769 0.745 0.760 0.752 0.765 0.753 | **0.756** | −24.4% |
| catalog-pipeline (target) | 0.984 0.971 0.984 0.978 0.973 0.986 | **0.981** | −1.9% |
| record-reference-copy (control) | 0.989 0.953 1.001 1.011 0.956 0.986 | **0.988** | −1.2% |
| map-string (control) | 0.988 0.970 0.993 0.957 0.979 0.983 | **0.981** | −1.9% |

Median absolute medians: record-create base 22.69 ms / patch 17.08 ms;
catalog-pipeline 34.47 / 33.73 ms; record-reference-copy 7.28 / 7.11 ms;
map-string 19.40 / 18.91 ms.

## Reading

- `record-create` improves by about 24% with tightly clustered
  per-round ratios (0.745–0.769), well above this session's noise
  floor: the removed header/fields allocation pair and the stack
  root buffer dominate that row's per-record cost.
- `catalog-pipeline` moves a consistent one to two percent; its
  remaining cost sits in string-keyed map work, not record
  construction.
- Both controls are flat to slightly faster; nothing regressed beyond
  the drift observed across the session.

## Limits

Local comparative numbers on one machine with a locally adapted
toolchain contract, not a reviewed Tier 1 baseline. Six interleaved
rounds screen slow drift; the interactive session's background load
sets the noise floor, so small effects may not resolve. Confirmation of
any claimed percentage requires the dedicated-runner protocol.
