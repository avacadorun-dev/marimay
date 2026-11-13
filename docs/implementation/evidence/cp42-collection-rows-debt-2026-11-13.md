# The two scalar-keyed collection rows exit as accepted debt

Local macOS arm64 evidence, 2026-11-13.
[Probe sources, medians and row pairs](cp42-collection-rows-debt-2026-11-13.json).

`#26` had two exits left after the [rune stride clause](cp42-rune-dense-scalar-2026-11-13.md)
and the earlier `word-frequency` candidates: `frequency-map` and `unique-set`.
Both are scalar-keyed, both sit on the same generic out-of-line collection call,
and this record measures their floor, lists what has already been ruled out and
names the next hypothesis. It is the accepted-debt exit the issue allows, and it
is [D127](../../design/decisions.md): both rows leave CP42 scope.

## Probe shape, and a correction to the screening rule

The first probe family put generation and the collection loop in `main`. Against
the row's own binary it measured 38.63 ms for work the corpus does in 30.15 ms,
28% high, with no allocation or collection difference to explain it.

The corpus keeps the loop in a function that receives the collection as a
parameter (`intFrequencies(values)`), which is the emitter's prewarmed
substitution path: the element access is emitted against a view bound by the
caller rather than rebound per element. Splitting the probes the same way -
`generate()`, `iterate(values)`, `frequencies(values)`, `unique(values)` - brought
them to 29.48 ms and 21.98 ms against rows at 30.26 ms and 22.16 ms.

The screening rule therefore gains a clause: a probe must match the row's
**call shape** - in particular whether the collection crosses a function boundary
before the loop - not only its volume and collection schedule. An in-main probe
overstated this row by 28%, which is larger than any effect the timebox has
measured on it.

## Floors at the corpus size (1,000,000 elements)

Three warmups and eleven fresh-process samples per probe, medians:

| Probe | Work | Median ms |
|---|---|---:|
| `generate` | 1,000,000 `push` into `[]int` | 4.93 |
| `generate` + `iterate` | plus one `for` pass, no collection | 5.60 |
| `generate` + `frequencies` | plus 1,000,000 `Map<int,int>` get+put | 29.48 |
| `generate` + `unique` | plus 1,000,000 `Set<int>` add | 21.98 |

| Derived quantity | Value |
|---|---:|
| per `int`-keyed get+put pair | 23.9 ns |
| per `Set<int>` insertion | 16.4 ns |
| map share of the `frequency-map` row | 81% |
| set share of the `unique-set` row | 74% |
| iteration over 1,000,000 `int` elements | 0.67 ms, about 0.67 ns each |

The rows themselves are `frequency-map` 30.26 ms and `unique-set` 22.16 ms; the
same-hour matrix puts them at 2.00× and 1.37× their Go columns.

## Where a pair goes

Top-of-stack shares from the shaped `frequencies` probe, about 700 samples:
`mmay_managed_map_probe` 21%, the `get` and `put` frames together 39%,
`mmay_managed_map_lookup_resolved` 13.5%, `mmay_hash_seeded_word` 6%,
`mmay_managed_invalid_reference` 2.6%, and the generated loop 16.5%. There is no
single frame above a fifth of the time: the cost is spread across the generic
call boundary, the validated handle resolution, the hash, the probe and the
status plumbing.

## Already ruled out by measurement

| Mechanism | Why it cannot move these rows |
|---|---|
| Last-lookup memo | already used by scalar keys before this timebox; it covers the `put` half of the pair |
| Dense element access | `[]int` already took the inline scalar path; only `[]rune` was missing it |
| Released-object pool | these rows allocate the array once and grow the map a handful of times, so candidate 3's pooling measured nothing here |
| Inline short-key comparison | string keys only; the scalar path has no byte comparison |
| Container shape | the map is at its own floor: an isolated C replication of `get`+`put` at the same cardinality measures 25.8 ns per pair |

## Next hypothesis, ranked

1. **Fused collection update.** The idiom `match m.get(k) { Some(v) => f(v),
   None => d }` followed by `m.put(k, ...)` is two runtime calls, two handle
   resolutions and two key validations. A lowering that recognises the adjacent
   pair and enters one runtime call would save one resolution, one validation and
   one status round trip - on the order of 8-10 ns of the 23.9 ns pair, so up to
   a third of the row. It is a MIR-level pass with a new runtime entry point, so
   it needs its own decision and differential coverage.
2. **A bound map view**, mirroring the array view: resolve the map once and reuse
   it for repeated accesses on the same variable. Smaller ceiling (2-4 ns per
   pair) and the same backend cost.

Neither is a timebox candidate: both are backend and ABI projects, and the issue
excludes anything that needs a heap rewrite inside the bounded slot.

## What this does not prove

No Tier 1 baseline and no VM column. The floors are local medians, not a
regression gate. `word-frequency` remains above its 2.0× Go target and stays with
#3.
