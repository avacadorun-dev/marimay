# A memo that survives the read-modify-write pair

Local macOS arm64 evidence, 2026-11-11; fifth candidate inside the #26/#3
timebox. [Orders, samples and executable hashes](cp42-string-key-lookup-memo-2026-11-11.json).

This closes the gap the [previous record](cp42-string-key-lookup-2026-11-11.md)
left open: after collapsing the duplicate checks, one get+put pair still did the
whole lookup twice, because `get` and `put` are two calls and each resolved the
key, hashed it and probed the index. It is not a corpus acceptance gate: Tier 1,
the VM and Bun columns and the three other collection rows remain unchanged and
open.

## Mechanism

The corpus idiom is read-modify-write: `counts.get(word)` followed by
`counts.put(word, count + 1)` on the same key. The map already carried
`MarimayManagedMapLookup`, a memo of the last successful scalar-key lookup, but
it was unusable for reference keys: it compared a 64-bit scalar, and strings are
equal by content, not by handle.

The string path now fills the same memo and reads it back:

- the memo keeps the entry index, the bucket, the hash, the mutation generation,
  the seeds and — in the scalar slot the raw64 path uses for its key — the **key
  length**; it holds no reference and no copied bytes;
- a hit requires an unchanged generation and unchanged seeds, a matching key
  length, an active entry whose stored hash still matches, and finally a full
  content comparison of the stored key with the query key;
- a miss is not an error. It falls through to the unchanged probe, which re-runs
  every check, so the memo can only avoid repeating an answer, never change one.

Lifetime needs no extra rule: `put` on a found entry, `put` that inserts,
`remove`, `clear`, growth, tombstone rehash and the storage rebuild all bump
`mutation_generation`, and the generation never wraps because the mutation path
refuses to run at `UINT64_MAX`. A stale memo is therefore rejected by one scalar
compare. Because the memo holds no reference it is not a root either, so it can
neither retain nor resurrect a collected object.

## Correctness

| Check | Result |
|---|---|
| full Debug suite, serial | 99 of 100 passed |
| ASan+UBSan `managed-memory`, `resource`, `stateful` | 6/6 passed |
| ASan+UBSan `c-unit`, `cp33` by name | 20/20 passed |
| corpus matrices, all 32 rows | identical stdout and checksums |

`mmay_macos_package` fails on `pkgbuild` in this environment; it reproduces with
the change stashed and it exercises packaging rather than the managed heap.

The two-heap differential map/set fuzzers are the load-bearing check here: they
walk two heaps through the same randomized action sequence and compare every
observable result, so a memo that outlived its generation would diverge. The
corpus matrices compare every row's checksum against the previous candidate's
matrix and find no difference.

## Measurement

**Screening, paired probe.** The same map probe source built by the previous and
the current release compiler, each linking its own native-runtime archive, three
warmups and eleven alternating samples per side, fresh process per sample.

| Side | Median ms | ns per get+put pair |
|---|---:|---:|
| previous candidate | 33.413 | 83.5 |
| this candidate | 30.050 | 75.1 |

**−10.06%** on the map probe.

**Corpus rows, paired** between the two matrices' executables:

| Row | Previous ms | This ms | Delta | Checksums |
|---|---:|---:|---:|---|
| `word-frequency` | 88.177 | 85.731 | **−2.77%** | 2925090 = 2925090 |
| `map-string` | 17.570 | 16.970 | **−3.41%** | 268505088 = 268505088 |
| `map-string-book` (record keys) | 22.540 | 22.480 | −0.28% | equal |
| `set-string-churn` (set: no get before put) | 32.170 | 32.550 | +1.18% | equal |
| `unique-set` (int keys) | 21.700 | 22.050 | +1.63% | equal |
| `rune-frequency` (rune keys) | 21.100 | 21.270 | +0.80% | equal |

An A/A control on `word-frequency` moved +0.32%. The rows the mechanism cannot
reach move within ±1.6%, which bounds code-layout noise for this pair; the two
string-keyed map rows move beyond it.

The row gain is smaller than the probe gain because the map is 41% of the row:
−10.06% of that share is −4.1%, and part of the difference is that the probe
skips the per-round key construction the row pays for.

Cumulatively, `word-frequency` has moved from 130.70 ms at the timebox start to
85.73 ms, **−34.4%**, and from 6.68× to about 4.3× its Go column.

## What this does not prove

No Tier 1 baseline, no VM or Bun column, and the 2.0× target is still unmet. The
gain depends on an adjacency the language does not require: a `get` followed by a
`put` on the same key. `set-string-churn` inserts without reading and gains
nothing; a workload that alternates distinct keys would pay only the cost of the
generation compare, which is why the row shows −2.77% rather than −10%.

## Where the time goes now

The map is still about 38% of the row and a get+put pair about 83 ns:

| Component | ns per operation |
|---|---:|
| `mmay_hash_seeded` over the key bytes | ~9 |
| bucket group scan | ~6 |
| stored-key resolve plus length/`memcmp` compare | ~8 |
| memo check and entry/output plumbing, `Option`/`Result`, loop | ~19 |

The two remaining mechanisms are the short-key comparison that still enters
`memcmp` and the probe's group scan; neither is implied by this record.
