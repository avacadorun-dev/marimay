# CP42 performance timebox: start record and measured floors

Local macOS arm64 evidence, 2026-11-11; issue #26 (with #3 in the same wave).
[Raw probe sources, hashes, commands and every observation](cp42-perf-timebox-2026-11-11.json).

This is the opening record of the bounded perf timebox, not a result. It fixes
the timebox, records which mechanisms are already known to be dead, replaces the
repeated "the allocation path is the bottleneck" claim with a measured
decomposition, and names the one open discrepancy that must be closed before any
mechanism is selected. It closes no performance row and changes no gate state.

## Timebox

- **Start** 2026-11-11. **Hard stop** 2026-12-09 (four weeks, ~5 h/week, ~20 h).
- Scope is exactly `frequency-map`, `unique-set`, `rune-frequency` (#26) and
  `word-frequency` / the `text.split` path (#3). Nothing else enters.
- Per row the exit is one of two things, never a third: a **paired improvement**
  with unchanged checksums, unchanged insertion order, unchanged allocation and
  GC counters, and no confirmed regression above 2% median / 5% P90 on the other
  31 corpus rows; or an **accepted debt** entry naming the measured floor, the
  rejected mechanism, the next hypothesis and a decision record that removes the
  row from CP42 scope.
- If the timebox ends mid-candidate the candidate is abandoned, not carried into
  alpha.5. A negative result with a recorded mechanism is a completed task.

## Baseline for acceptance

The acceptance reference stays the unchanged 32-workload corpus, not the probes
below. These are the current accepted numbers from the alpha.3 five-participant
run (3 warmups, 11 samples, rotating order):

| Row | Marimay native, ms | Go, ms | Rust, ms | × Go |
|---|---:|---:|---:|---:|
| `word-frequency` | 142.999 | 21.740 | 32.265 | 6.58 |
| `frequency-map` | 31.668 | 16.369 | 20.060 | 1.93 |
| `unique-set` | 24.833 | 18.206 | 22.451 | 1.36 |
| `rune-frequency` | 23.203 | 6.667 | 7.787 | 3.48 |

Those numbers remain valid as the start line because nothing the compiler emits
changed between the benchmarked base `68e1522` and this record's revision
`900c25b`: `git diff --stat 68e1522..900c25b -- bootstrap runtime std` is empty.
The intervening commits are documentation, tools and CI only.

## Reviewed rejections

The issue requires reading these before touching anything. Each one bounds what a
new candidate may do; none of them bounds what a new candidate may *measure*.

| Candidate | Mechanism | Why it was rejected | What it forbids now |
|---|---|---|---|
| `cp42-shared-string-storage-2026-09-11` | one block for header and payload, direct concat into it | Native `min-heap` +3.78% median, +3.49% on repeat | coallocating header and payload again without a min-heap-safe result |
| `cp42-string-publication-2026-09-11` | shared construction plus a D055 budget preflight owner | confirmed per-row Native regression under an improved aggregate | publication-path rewrites justified by aggregate gains |
| `cp42-string-array-projection` (v1–v3, 2026-09-12) | remove near-all invariant-header publication | v3 `title-search-10k` ratio 0.9599 missed the 5% row minimum; ON VM missed 3% | a further binding-layout tweak inferred from store counts alone |
| `cp42-string-body-sharing-2026-09-13` | cache and share string bodies | `word-frequency` Native went 400,100 → 19 body allocations and only −4.67% | another cache-size/hash microvariant series from allocation counts |
| `cp42-jit-*` (7 rejected, 2026-09-11…13) | cell projection, immediate enums, local views, string metadata, search loop | each missed its pre-declared gate or bought no useful VM gain | JIT default-on; profile-driven instruction selection from ambiguous samples |

The `string-body-sharing` row is the important one for this timebox: it proves
that removing nearly every string-body allocation in `word-frequency` moves the
row by less than 5%. Allocation count is therefore not a sufficient explanation
of the 6.58× and cannot be the first hypothesis again.

## Measured decomposition

Two probe families were derived from the unchanged corpus rows. They are
screening probes for attribution, not acceptance evidence: they build with
release compiler `4bf2885` and the same `--max-heap-bytes 67108864`, run as fresh
processes, and are timed with 5 warmups and 21 samples in alternating order. The
probe `full` variant runs 92.3% of the corpus `word-frequency` row, so it locates
cost but does not replace the row.

```sh
# every probe, from the probe directory
/…/marimay/build/release/mmay build probe_<name>.my --release \
  --max-heap-bytes 67108864 --output build-probe_<name>
python3 timeit.py ./build-probe_split1 ./build-probe_split ./build-probe_map ./build-probe_full
python3 timeit.py ./build-probe_rune_loop ./build-probe_rune_push ./build-probe_rune_map ./build-probe_int_map
```

### `word-frequency` family — 10 rounds × 10,000 lines

| Probe | Work | Median ms | Min ms |
|---|---|---:|---:|
| `split1` | 100,000 `text.split` calls, 1 piece each | 43.424 | 41.576 |
| `split` | 100,000 `text.split` calls, 4 pieces each (400,000 strings) | 95.420 | 92.855 |
| `map` | 400,000 `Map<string,int>` get+put, no split | 39.558 | 38.522 |
| `full` | corpus `wordFrequency` body | 132.021 | 130.483 |

`split` + `map` = 135.0 ms against 132.0 ms measured, so the two costs are
additive within 2.2%: neither hides inside the other.

| Derived quantity | Value |
|---|---:|
| fixed cost per `text.split` call | 260.9 ns |
| marginal cost per produced piece | 173.3 ns |
| cost per string-keyed get+put pair | 98.9 ns |
| split share of the row | 72% |
| map share of the row | 30% |

### `rune-frequency` family — 450,000 runes

| Probe | Work | Median ms | Min ms |
|---|---|---:|---:|
| `rune_loop` | iterate the 15-rune pattern 30,000 times, no array, no map | 4.714 | 4.249 |
| `rune_push` | the same loop plus one `[]rune` push per element | 9.046 | 8.335 |
| `rune_map` | the same loop plus one `Map<rune,int>` get+put per element | 20.792 | 20.238 |
| `int_map` | the same loop plus one `Map<int,int>` get+put per element | 17.599 | 17.433 |

| Derived quantity | Value |
|---|---:|
| cost per `[]rune` push | 9.6 ns |
| cost per `rune`-keyed get+put pair | 35.7 ns |
| cost per `int`-keyed get+put pair | 28.6 ns |
| map share of the row | ~86% |

`rune_map` + `rune_push` − `rune_loop` = 25.1 ms against the corpus row's
23.2 ms, so the probes overstate by 8% and the map dominates the row.

### Isolated primitive floors (C, same runtime archive)

| Primitive | Result |
|---|---:|
| `mmay_managed_string_new`, 400,000 objects, no roots | 68.5 ns/op |
| `mmay_managed_map_get` + `put`, raw64 keys, 4,096 keys, 400,000 pairs | 25.8 ns/pair |
| emitted split scaffold replicated in C, 1 piece / 4 pieces | 253.1 / 350.9 ns per call |

## Attribution, row by row

| Row | Where the time goes | × Go |
|---|---|---|
| `word-frequency` | 72% `text.split` (400,000 pieces at 173 ns, plus 261 ns per call), 30% string-keyed map | 6.58 |
| `frequency-map` | ~90% `Map<int,int>` get+put: the row is 31.7 ns/word against a 28.6 ns probe pair, so value generation is a few ns | 1.93 |
| `unique-set` | one set insertion per element at 24.8 ns, cheaper than a get+put pair because there is no second lookup | 1.36 |
| `rune-frequency` | ~86% `Map<rune,int>` get+put at 35.7 ns; `rune` keys cost 7 ns more per pair than `int` keys | 3.48 |

## What these numbers rule out

- **Collection pressure.** Building the same split probe with 64 MiB and with
  1 GiB of heap gives 95.3 ms and 93.5 ms. The row is not GC-bound.
- **A stale or differently built runtime.** Recompiling the emitted
  `program.c` against the local `libmmay_native_runtime_release.a` reproduces
  the compiler's own binary within noise (91.8 ms median versus 95.4 ms), so the
  numbers are not an artifact of which archive the CLI links.
- **A map algorithm defect.** The raw64 map probe in C costs 25.8 ns per
  get+put; the same operation inside a Marimay program costs 28.6–31.7 ns. The
  map is operating near its own implementation floor, and the remaining gap to Go
  is per-operation constant work, not a wrong data structure.
- **The split scan itself.** The counting pass and the byte scan cover ~20 bytes
  per call, which is included in the 261 ns fixed cost and is not worth a
  single-pass rewrite.

## The open discrepancy that blocks mechanism selection

Inside a Marimay program one produced piece costs **173 ns**. In the C
replication of the emitted helper the marginal per-piece cost is **33 ns**, and
the isolated publication primitive is **68.5 ns**. The same work cannot be both
33 ns and 173 ns, so one of the three measurements is not measuring what it
claims, and the per-piece decomposition is not yet usable.

This is recorded as the first diagnostic step of the timebox, not as a result,
because the two prior rejections in this family were both caused by selecting a
mechanism from counters that did not locate the cost. Until the discrepancy is
resolved, no mechanism is selected and no production source is touched.

The bounded diagnostic: compile the emitted helper and the C scaffold under the
same allocator and the same object-liveness pattern, take allocation and slot
counters on both, and attribute the 105 ns difference to either publication
bookkeeping or per-call scaffolding. It ends in a number, not an opinion.

## Ranked hypotheses with pre-declared targets

Ordered by the measured share of the four rows, not by novelty. Each carries its
own target before timing, so a later "improvement" cannot be defined after the
fact.

1. **H1 — split scaffolding, `word-frequency` only.** `text.split` pays one
   cell-array `malloc`/`free`, one strided root frame and one array publication
   per call, and 173 ns per produced piece. Target: split from 95.4 ms to
   ≤ 65 ms with the map component unchanged, which moves the row from 6.58× to
   ≤ 4.7× Go. Blocked on the discrepancy above.
2. **H2 — string-key map path, `word-frequency`.** 98.9 ns per get+put for
   2–10 byte keys against 25.8 ns for raw64 keys in the same map. Target: −25% on
   the map component (39.6 → ≤ 30 ms). Lowest risk, smallest ceiling.
3. **H3 — `rune` key path, `rune-frequency`.** 7 ns per pair more than `int`
   keys on a row where the map is 86% of the time. Target: remove the difference
   entirely, which caps the gain at ~8% of the row and is therefore a follow-up,
   not a row-closing candidate.
4. **Not in scope, recorded for the decision line.** A combined count/upsert
   operation would halve the lookup count on all three collection rows, and #3
   already excludes it because it is a language API decision. If the timebox
   ends with the collection rows still open, this is the mechanism the debt entry
   must name.

## What this does not prove

Only `word-frequency` and `rune-frequency` are decomposed; `frequency-map` and
`unique-set` are inferred from one probe pair each and a corpus row. Every
number here is a local unisolated observation: load average was 5.5 on 8 CPUs
during the clean runs and 11.2 during one discarded rerun, and the discarded run
is kept in the raw record rather than deleted. No Tier 1 baseline, no Go/Rust
parity claim, no JIT statement and no CP42 closure follows from this document.

## Exit

On or before 2026-12-09 each of the four rows gets either a paired improvement
with its complete gate evidence or an accepted debt entry with a measured floor,
the rejected mechanism, the next hypothesis and a decision record. Nothing from
this timebox carries into alpha.5 without a new decision.
