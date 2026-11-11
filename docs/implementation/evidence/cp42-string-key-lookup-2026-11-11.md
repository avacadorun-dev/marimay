# One validation and one resolution per string-keyed lookup

Local macOS arm64 evidence, 2026-11-11; fourth candidate inside the #26/#3
timebox. [Orders, samples and executable hashes](cp42-string-key-lookup-2026-11-11.json).

This attacks the other half of the `word-frequency` row. The
[separator-search record](cp42-split-separator-search-2026-11-11.md) closed the
split side; the map probe is now 41% of the row, and the policy record for the
[released-object pool](cp42-released-object-pool-2026-11-11.md) established that
sweep-based levers cannot reach these rows at all. It is not a corpus acceptance
gate: Tier 1, the VM and Bun columns and the three other collection rows remain
unchanged and open.

## Measured decomposition before the change

The probe family was rebuilt in one session with the tree's own pre-change
release compiler, three warmups and eleven samples in one alternating session, so
the three probes below share a host state:

| Probe | Work | Median ms | Min ms |
|---|---|---:|---:|
| `split` | 100,000 `text.split` calls, 400,000 strings | 57.720 | 55.124 |
| `map` | 400,000 `Map<string,int>` get+put pairs | 39.714 | 38.616 |
| `full` | the corpus `wordFrequency` body | 95.871 | 91.723 |

`split` + `map` = 97.4 ms against 95.9 ms measured, so the two costs are still
additive within 1.6%; the map is **41% of the row** and one get+put pair costs
**99 ns**. `full` also lands on the corpus row itself, which the same compiler
measured at 96.61 ms in the paired matrix below.

## Mechanism

One string-keyed lookup resolved the key handle four times and validated it
twice. Per `get`, in order: `mmay_managed_map_resolve` for the receiver,
`mmay_managed_validate_field` (which resolves the key slot), then
`mmay_managed_map_value_hash` (which resolves it again), then
`mmay_managed_map_value_equal` for the probe's hash candidate, which resolved
both the stored key and the query key. Validation, hashing and comparison
together took 930 of the profile's 3,352 samples (27.7%), and two of the three
resolution passes inside those frames repeated a check an earlier frame had
already done.

The string key now has one path through the lookup:

- `mmay_managed_map_string_key_slot` runs the same checks in the same order as
  validation followed by hashing — descriptor shape, invalid handle, resolve,
  object kind, non-null descriptor, immutability — and returns the resolved
  slot;
- the hash reads that slot directly (`mmay_managed_map_string_hash_slot`), with
  the same `mmay_hash_seeded` call over the same bytes, so bucket placement is
  bit-identical;
- `mmay_managed_map_find_reference` takes the resolved slot and only resolves
  the stored key of a hash candidate, comparing through
  `mmay_managed_map_string_slot_equal`.

Every other key shape — raw64, enum, record — keeps the generic path unchanged,
and `put` keeps its post-growth re-probe, now with the same resolved slot. No
allocation, no new field in `MarimayManagedMap`, no change to the layout reserved
for compiler-generated C, and no change to the status codes a bad caller sees;
the only difference is that the same checks run once instead of twice.

## Correctness

| Check | Result |
|---|---|
| full Debug suite, serial | 99 of 100 passed |
| ASan+UBSan `managed-memory`, `resource`, `stateful` | 6/6 passed |
| ASan+UBSan `c-unit`, `runtime`, `cp33` | 5/5 passed |

`mmay_macos_package` fails on `pkgbuild` in this environment; it reproduces with
this change stashed, it exercises packaging rather than the managed heap, and it
is tracked as a local environment limitation.

The runtime suite includes the two-heap differential map/set fuzzers and the
deterministic allocation-failure sweeps, which is where a changed check order or
a stale resolved slot would surface. The change allocates nothing, so no
allocation-failure index moves.

## Measurement

**Screening, paired probe.** The same map probe source, built once by the
pre-change release compiler and once by the patched one; each side links the
native-runtime archive of its own build tree. Three warmups per side, eleven
samples per side, side order alternating every round, fresh process per sample.

| Side | Median ms | Min | Max | ns per get+put pair |
|---|---:|---:|---:|---:|
| pre-change | 39.728 | 39.380 | — | 99.3 |
| patched | 34.790 | 34.429 | — | 87.0 |

**−12.43%** on the map probe. An A/A control on the same probe (the pre-change
binary against a copy of itself) moved **+1.15%**, an order of magnitude smaller.

**Corpus rows, paired.** Both rows were timed from the executables the two
matrices produced, before and after, interleaved with the same protocol.

| Row | Pre-change ms | Patched ms | Delta | Checksums |
|---|---:|---:|---:|---|
| `word-frequency` | 93.994 | 88.433 | **−5.92%** | 2925090 = 2925090 |
| `map-string` | 18.930 | 17.770 | **−6.16%** | 268505088 = 268505088 |
| `map-string-book` (record keys) | 22.600 | 22.460 | −0.62% | equal |
| `frequency-map` (int keys) | 29.830 | 30.230 | +1.33% | equal |
| `unique-set` (int keys) | 21.790 | 22.010 | +1.00% | equal |
| `rune-frequency` (rune keys) | 21.220 | 21.130 | −0.42% | equal |

An A/A control on `word-frequency` moved +0.18%. The scalar-key rows that the
change does not touch move within ±1.4%, which bounds code-layout noise for this
pair of binaries; the two string-keyed rows move six times that.

The two full corpus matrices behind these executables agree: `word-frequency`
96.61 → 91.08 ms and `map-string` 19.28 → 17.54 ms.

## What this does not prove

No Tier 1 baseline, no VM or Bun column, and no claim that the row meets its
2.0× target: `word-frequency` is at 4.4× Go after this change. Record-keyed maps
(`map-string-book`) gained nothing, as expected, because they keep the generic
path. `set-string-churn` did not move outside noise.

## Where the time goes now

A get+put pair still costs 87 ns, that is ~43 ns per operation, for a key of 2
to 10 UTF-8 bytes in a 16-bucket map:

| Component | ns per operation |
|---|---:|
| `mmay_hash_seeded` over the key bytes | ~9 |
| bucket group scan (`mmay_managed_map_probe`) | ~6 |
| stored-key resolve plus length/`memcmp` compare | ~8 |
| entry/output plumbing, `Option`/`Result` construction, loop | ~20 |

The remaining structural waste is that `get` and `put` on the same key each
recompute all of it, and that the comparison still calls `memcmp` for keys of at
most ten bytes. Both are separate mechanisms and separate candidates; neither is
implied by this one.
