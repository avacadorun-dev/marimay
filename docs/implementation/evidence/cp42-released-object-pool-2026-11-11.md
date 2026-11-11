# A released-object pool the corpus cannot use

Local macOS arm64 evidence, 2026-11-11; third candidate of the #26/#3 timebox,
measured and **not adopted**. [Probe pairs and corpus medians](cp42-released-object-pool-2026-11-11.json).

This is a rejection with the mechanism intact. It also corrects how the timebox
screens candidates: the 5,000,000-call probe used by the first two candidates
forces about forty-five collections, and the corpus rows force none, so a
per-object saving measured on that probe overstates the row by roughly seven
times.

## What was built

The split profile of the second candidate put 11.5% of the program inside the
host `free`, 8.3% inside the host `malloc` and 22.1% inside the collector's
sweep. Every managed object was a `malloc`/`free` round trip, including the ones
the sweep released in bulk. The heap gained per-size-class free lists of released
object blocks: classes are 16-byte multiples up to 256 bytes, the class of a block
is a pure function of the logical object size, and everything is released by
`mmay_managed_heap_dispose`. Object constructors already zero their headers, so
reused blocks carry no state; the array, map and set paths keep their own
resizable storage out of the pool.

Three variants were measured, because the retention policy turned out to decide
the outcome:

1. a fixed 1 MiB cap on retained bytes;
2. a cap of `max_live_bytes`, the D055 budget;
3. variant 2 plus a fast path that skips the class arithmetic while the pool is
   empty.

## Measurement — screening probe

The 5,000,000-call split probe of the previous record, three warmups and eleven
alternating samples per side, fresh process per sample.

| Variant | Median ms | Delta |
|---|---:|---:|
| enum-only (base) | 2545.5 | — |
| 1 MiB cap | 2621.1 | **+2.97%** |
| `max_live_bytes` cap | 2508.2 | **−20.32%** |
| `max_live_bytes` cap + empty fast path | 2617.5 → 1993.7 | **−23.83%** |

The 1 MiB cap is the instructive one. A single collection releases about nine
hundred thousand objects at once, so the cap is exhausted in the first two
percent of the burst and the pool then contributes a branch and a class
computation for nothing. A pool is only worth its lookup if it can hold the whole
dead set, which is why the second variant uses the budget rather than a byte
count.

## Measurement — the corpus row

The unchanged 32-workload corpus, native column, anchors subset, three warmups
and eleven samples per cell. `word-frequency` is `size = 10000`, `rounds = 10`.

| Matrix, compiler | `word-frequency` median ms | Its Go column | Ratio |
|---|---:|---:|---:|
| `dee131c`, first | 97.96 | 19.99 | 4.90× |
| `dee131c`, repeat | 99.24 | 19.58 | 5.07× |
| enum only (`3edb72c`), first | 97.07 | 19.92 | 4.87× |
| enum only, repeat | 97.65 | 20.36 | 4.80× |
| enum only + 1 MiB-cap pool | 101.95 | 21.22 | 4.81× |
| enum only + budget-cap pool + fast path | 96.07 | 19.78 | 4.86× |

Two facts follow. First, the pool as first built costs the row 4.7%, and the fast
path removes that cost entirely. Second, with the fast path the row is 96.07 ms
against 97.36 ms for enum-only — inside the 1.3% spread between the two
`dee131c` matrices.

## Why the pool cannot help this row

`word-frequency` allocates roughly seven objects per call over a hundred thousand
calls, about seven hundred thousand in total, against a `max_objects` threshold
of one million. **No collection runs.** Nothing is ever released, so the pool is
never filled, and the only thing it can do is add cost to the allocation path.
The probe, by contrast, runs fifty times more calls, crosses the threshold
forty-five times, and spends its steady state popping blocks that the previous
sweep pushed.

That is the screening error this record exists to prevent: the probe reproduces a
call's *shape* but not the corpus row's *allocation volume and collection
schedule*. Removing two of ten allocations on the probe measured −8.98%; the same
two allocations on the row measure −1.3%.

## Correction to the second candidate

The [single-allocation enum record](cp42-enum-single-allocation-2026-11-11.md)
reported −8.98% on the probe and calibrated the remaining cost at 24 ns per heap
allocation. The probe number stands; the transfer to the row does not. Against the
two `dee131c` matrices the row moved 98.60 → 97.36 ms, **−1.3%**, which is the
same size as the 1.3% spread between those two matrices. The change is still
correct, still removes two allocations from every `Result` in every program, and
still stands on its own gates; it is simply worth far less to this row than the
probe implied, because most of the row's per-object cost is the host allocator,
not the sweep.

## What remains true

The sweep really is removable. On a workload that does collect, retaining
released blocks is worth 23.8%, and the mechanism is 200 lines with no new
failure path. The pool is therefore recorded as available evidence, not as a
rejected mechanism: adopting it for the alpha line needs a decision of its own,
because it changes the memory policy — retained blocks would be capped at the
D055 budget, so managed-object memory could reach twice the configured value —
and the benefit is not on the two rows this timebox exists to move.

## Where the row's time goes now

`word-frequency` costs 97 ms for 100,000 calls, about 970 ns per call, and it
never collects. The split work of the previous candidates is about 300 ns of that
under a non-collecting schedule; four `Map<string, int>` get+put pairs are about
390 ns at the recorded ~98 ns per pair; the remainder is the loop, the aggregation
and the final key sweep. Allocation levers are therefore capped at the host
allocator's price, and the next candidate should be the string-keyed map, not the
object model.

## What this does not prove

No Tier 1 baseline, no VM or Bun column, no parity claim, no CP42 or CLI0
closure, and no claim that the pool is wrong — only that it does not pay for
itself on the rows this timebox exists to move, and that a 1 MiB retention cap is
worse than no pool at all. Host isolation remains unverified; every number above
is a local median with the drift controls named next to it.
