# D115 seeded-hash A/B screening on the string-keyed laggards

Status: **local paired screening passed**, candidate commit `9bba4a1` on
`feature/perf-string-runtime` against baseline `49a6a26` (`origin/main`).
The GitHub CI matrix for the candidate revision is the authoritative build
and test validation; this record covers the unchanged-corpus performance
screening only. No Tier 1 budget, all-32 reviewed baseline or parity status
is claimed or changed.

## Mechanism

Decision D115 replaces SipHash-2-4 with the `mmay_hash_seeded` owner (XXH64
structure, both injected collection seed words folded into the standard seed
slot) for managed Map/Set key hashing. Hashing is unobservable per the
ordered-collection decision; iteration order, key equality, probing,
capacities, failure atomicity and D055 accounting are unchanged. SipHash-2-4
remains for the compiler's internal tables. Known-answer vectors are pinned
against the canonical xxHash reference implementation.

## Method

Both compilers built as Release from identical trees except the candidate
commit (Apple M2, macOS arm64). Four alternating passes, two per side
(base, patch, base, patch), each 11 samples after 3 warmups with the
standard harnesses: the expanded eleven-algorithm matrix (Marimay, Go,
Rust) and the eight catalog rows most affected by string-key hashing plus
three control rows. Every pass reported `Status: passed`; checksums were
identical across languages, rounds and both compiler sides. Reported numbers
are per-side medians of the two pass medians. Machine isolation is the
ordinary local contract, not a dedicated runner.

## Expanded algorithm matrix, Marimay native median ms

| Workload | base `49a6a26` | candidate `9bba4a1` | change |
|---|---:|---:|---:|
| insertion | 51.10 | 51.77 | +1.3% |
| heap-sort | 33.19 | 33.14 | −0.2% |
| stack | 10.18 | 10.40 | +2.1% |
| queue | 10.10 | 10.41 | +3.0% |
| min-heap | 49.50 | 49.38 | −0.2% |
| dsu | 32.27 | 32.45 | +0.5% |
| bfs | 70.30 | 70.25 | −0.1% |
| dijkstra | 42.94 | 42.86 | −0.2% |
| frequency-map | 35.05 | 30.34 | −13.4% |
| unique-set | 27.94 | 21.82 | −21.9% |
| rune-frequency | 22.71 | 21.28 | −6.3% |

## Catalog rows, Marimay native median ms

| Workload | base `49a6a26` | candidate `9bba4a1` | change |
|---|---:|---:|---:|
| title-search-10k (control) | 6.89 | 6.65 | −3.6% |
| normalize-title | 55.87 | 54.97 | −1.6% |
| word-frequency | 140.27 | 134.66 | −4.0% |
| report-builder (control) | 12.18 | 12.30 | +1.0% |
| map-int | 7.88 | 7.19 | −8.8% |
| map-string | 21.77 | 19.23 | −11.7% |
| map-string-book | 25.46 | 25.11 | −1.4% |
| set-string-churn | 33.88 | 32.81 | −3.2% |

## Reading

- Every string-keyed or hash-heavy row improved; the scalar/structure
  anchors (stack, queue, heap-sort, min-heap, bfs, dijkstra) moved by at
  most about ±3%, inside the observed noise of this local machine.
- `unique-set` (−21.9%), `frequency-map` (−13.4%) and `map-string` (−11.7%)
  carry the expected mechanism: integer and short-string key hashing on the
  get/put hot path. `map-int` (−8.8%) confirms the raw-word entry point
  gain even for non-string keys.
- `word-frequency` (−4.0%) and `normalize-title` (−1.6%) remain far from Go
  (about 7x and 6x local medians). Their measured dominant costs are string
  splitting / grapheme allocation and builder append, not key hashing; the
  D115 change is correctly small there. Those rows need a separate
  allocation-path investigation and are not closed by this record.
- No row regressed beyond the local noise floor in either round of either
  side.

## Limits

Local comparative numbers on one machine, not a reviewed Tier 1 baseline.
Two passes per side screen drift; they do not replace the dedicated-runner
protocol. Hash-flooding posture now rests on the existing seed-injection
contract with fixed default seeds, as recorded in D115; no SipHash-equivalent
DoS-resistance claim is made.
