# One allocation per enumerated value

Local macOS arm64 evidence, 2026-11-11; second candidate inside the #26/#3
timebox. [Orders, samples and executable hashes](cp42-enum-single-allocation-2026-11-11.json).

This corrects the cost decomposition published in the
[separator-search record](cp42-split-separator-search-2026-11-11.md) and delivers
a second paired improvement for `word-frequency` (#3). It is not a corpus
acceptance gate: Tier 1, the VM and Bun columns and the three other collection
rows remain unchanged and open.

## Correction to the previous decomposition

The previous record closed with "per-call scaffolding ~265 ns, four pieces
published 299 ns" for a 606 ns call, derived from a ladder over a C harness. That
ladder was measured without a garbage collection: the harness allocated nine
hundred thousand objects per probe and the heap collects at one million, so the
sweep never ran in it. The real probe collects continuously.

A `sample` profile of the real 5,000,000-call split probe attributes the call
differently:

| Symbol, inclusive | Share of the program |
|---|---:|
| `mmay_managed_string_join` (four per call) | 53.2% |
| — of which `collection_finish` (sweeping dead objects) | 22.7% |
| — of which `publication_prepare` | 9.9% |
| `mmay_managed_enum_new` (**one** per call) | 16.1% |
| `mmay_managed_array_construct` (one per call) | 10.6% |
| `_xzm_xzone_malloc_tiny` + `_xzm_free_main` | 24.2% |

The call performs **ten** heap allocations, not "one `malloc`/`free` pair plus a
root frame": four piece strings, the array object, the array storage, the enum
value, the enum payload, a temporary root vector for the enum, and the staging
block for the pieces. One fifth of the call is the collector releasing those
objects one by one, and another quarter is the host allocator. Per-object cost,
not per-call scaffolding, is the dominant term — which is why the four earlier
candidates that removed copies or shared bodies moved the row by less than 5%.

## Mechanism

`MarimayManagedRecord` already owns exactly one allocation: the fields live in
the tail of the object. `MarimayManagedString` does the same for its bytes. Only
`MarimayManagedEnum` was split across three allocations, and it is the object
`split` produces on every call, because `split` returns
`Result<[]string, string>`.

Two changes, both inside `runtime/common/managed_enum.c` and
`runtime/common/managed_internal.c`:

- the payload is placed in the tail of the enum allocation;
  `mmay_managed_enum_dispose` releases the single block;
- the temporary root vector moves to the stack for up to eight reference
  payloads, exactly the pattern `managed_record.c` already used.

The struct layout in `runtime/include/mmay/runtime/generated_slots.h` is
unchanged — the contract for compiler-generated C is untouched, only what the
`payload` pointer points at. `mmay_managed_enum_payload_bytes` is still the
authority for both the accounting total and the release size, so
`mmay_managed_slot_object_bytes` reports the same number as before and the
publication budget sees no change. Enumerated payloads are read-only in the
runtime API, so there is no second owner of the tail, and every failure path
between allocation and publication disposes through the same single-size call.
Allocation-failure indices shift for enums (three allocations become one); the
deterministic failure suites pass unchanged.

## Correctness

| Check | Result |
|---|---|
| full Debug suite, serial | 99 of 100 passed |
| ASan+UBSan `managed-memory`, `resource`, `stateful` | 6/6 passed |
| ASan+UBSan `c-unit`, `runtime`, `cp33` | 6/6 passed |

`mmay_macos_package` fails on `pkgbuild` in this environment; it reproduces with
this change stashed, it exercises packaging rather than the managed heap, and it
is tracked as a local environment limitation. Enums are covered by the
differential VM/native suites, which run the same typed MIR through both engines
and are part of the Debug suite above.

## Measurement

**Screening, paired probe.** The same probe source, built once by the pre-change
release compiler and once by the patched one; each compiler resolved its own
native-runtime archive from its own build tree, so both sides link their own
runtime. Three warmups per side, then eleven samples per side with the side order
alternating every round, fresh process per sample. The probe runs the corpus
`wordFrequency` line set through `split` 5,000,000 times.

| Side | Median ms | Min | Max | ns per call |
|---|---:|---:|---:|---:|
| pre-change | 2616.349 | 2594.945 | 2678.667 | 523.3 |
| patched | 2381.526 | 2365.626 | 2433.467 | 476.3 |

**−8.98%**, reproducing an earlier pair of the same two binaries at −8.89%. Two
A/A controls bound host drift on this probe: the pre-change binary against itself
moved +0.02% and −0.43%, the patched binary against itself −0.36% and +0.18%.

The mechanism is visible in the profile, not only in the totals: the
`mmay_managed_enum_new` share falls from 16.1% to 6.8%, the host allocator shares
from 24.2% to 19.8%, and the samples collected for the same work fall from 1530
to 1421.

Dividing the delta by the two removed allocations (~47 ns per call) calibrates the per-object cost
that the next candidate must attack: **about 24 ns per heap allocation and its
sweep**, consistent across the four strings, the array pair, the enum and the
staging block.

## What this does not prove

No Tier 1 baseline, no VM or Bun column, no parity claim, no CP42 or CLI0
closure. The probe is a proxy for the corpus row and is reported as such; the
corpus matrices for this candidate are recorded separately. Eight allocations
per split call remain, and the row is still far from its 2.0× target.

## Where the time goes now

At 476 ns per call and eight allocations left, roughly 190 ns is still object
allocation and sweep, ~120 ns is the four string publications' own work, and the
rest is the two scans, the array publication and the loop. The next candidate is
therefore the object cost itself — a cache of released object blocks inside the
heap — rather than more geometry: collapsing the array's two allocations and the
staging block would remove at most three of the eight and ~70 ns.
