# D117 strided-root split A/B screening

Status: **local paired screening**, candidate commit `7fc1226` on
`feature/perf-text-split` against baseline `e73eb64` (`main` at
`0.1.0-alpha.2`). The GitHub CI matrix is the authoritative validation;
this record covers the unchanged-corpus performance screening only. No
Tier 1 budget, reviewed baseline or parity status is claimed.

## Mechanism

The split/graphemes text paths previously allocated a second raw array
of slot pointers on every call solely to present the root frame with
the classic pointer-array layout, then filled it, pushed roots through
it and freed it. Decision D117 adds an optional byte stride to the
managed root frame: `mmay_managed_root_push_strided` roots inline
reference slots at `base + index * stride`, with the same
invalid-or-live validation, reverse push order and provenance rules;
stride zero preserves the classic layout exactly. Both the generated C
emitter and the VM twin now root the piece cells directly. One raw
allocation, one fill loop and one deallocation fewer per call on both
engine paths; managed-object counts, tracing, publication, D055
accounting and program outputs are unchanged.

## Method

Both compilers built as Release from identical trees except the
candidate commit (Apple M2, macOS arm64; the corpus toolchain contract
was adapted locally to `0.1.0-alpha.2`, identically for both sides).
An initial 2×(6 catalogue rows + 8 anchors) pass exhibited a monotone
machine slowdown of 8–45% across the session (ambient load from the
interactive session itself), so the screening was redone as six short
alternating rounds (base, patch, base, patch, …), four catalogue rows
per round (two target, two control), 11 samples after 3 warmups per
run. Every run reported `Status: passed`; checksums identical across
languages, rounds and both compiler sides. Reported figures are the
median of the six per-round patch/base medians.

## Interleaved per-round ratios (Marimay native, patch/base)

| Workload | Round ratios | Median | Reading |
|---|---|---:|---|
| word-frequency (target) | 0.987 1.025 0.994 0.987 0.994 1.051 | **0.994** | −0.6%, within noise |
| normalize-title (target) | 1.001 1.041 1.021 0.996 1.003 1.054 | **1.012** | +1.2%, within noise |
| title-search-10k (control) | 1.021 1.122 1.025 0.951 0.986 1.073 | **1.023** | +2.3%, within noise |
| map-string (control) | 1.005 1.121 1.035 0.990 1.027 1.061 | **1.031** | +3.1%, within noise |

Median absolute medians: word-frequency base 135.77 ms / patch 134.51
ms; normalize-title 55.92 / 56.35 ms; title-search-10k 7.26 / 7.30 ms;
map-string 19.93 / 20.46 ms. Per-run dispersion (MAD) on the target
rows was 0.7–0.8 ms (word-frequency) and 0.7 ms (normalize-title).

## Reading

- The candidate removes a deterministic per-call cost (one raw
  allocation, one pointer fill loop, one deallocation per
  split/graphemes call on both engines). Its expected effect on the
  target rows is small — the measured cost of those rows is dominated
  by per-piece managed publication, not by the temporary scaffolding.
- The controls move by as much as the targets (+2–3%), which bounds
  the screening's resolving power: the interactive session's own
  background load (window server and automation at tens of percent CPU)
  keeps the noise floor at or above the expected effect size. No
  regression beyond that floor is measurable on any row.
- A quantitative claim (a concrete percentage) is therefore not made
  here. Confirmation requires the dedicated-runner protocol the
  checkpoint budgets mandate; the architectural reduction stands on
  its own and the full 89-test local suite plus the CI matrix guard
  correctness.

## Limits

Local comparative numbers on one loaded machine with a locally adapted
toolchain contract, not a reviewed Tier 1 baseline. Six interleaved
rounds screen slow drift; they cannot substitute for the quiet-runner
protocol. The split/graphemes paths still pay one managed publication
per produced string; this change removes only the temporary scaffolding
indirection and does not close issue #3.
