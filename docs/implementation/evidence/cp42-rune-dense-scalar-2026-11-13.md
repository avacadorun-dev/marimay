# A rune is a dense scalar to the native emitter

Local macOS arm64 evidence, measured 2026-11-11 and committed 2026-11-13; sixth
candidate inside the #26/#3 timebox.
[Orders, samples, hashes and per-row pairs](cp42-rune-dense-scalar-2026-11-13.json).

This is the first candidate aimed at a collection row rather than at
`word-frequency`. It is not a corpus acceptance gate: Tier 1, the VM and Bun
columns and the two scalar-keyed collection rows remain unchanged and open.

## Mechanism

A `[]rune` array is stored as `MMAY_MANAGED_ARRAY_STORAGE_RAW32` with a
`sizeof(uint32_t)` stride - the same descriptor a `[]u32` array gets. The native
emitter's dense scalar element path, however, listed `i8/u8`, `i16/u16`,
`i32/u32/f32` and `i64/u64/f64` and not `MMAY_TYPE_RUNE`, so rune arrays fell
through to the cell path. Every rune element read then bound a generated view,
expanded it into a `MarimayManagedArrayView` and called the out-of-line
`mmay_managed_array_view_get`, which re-resolves the view pointer, zeroes a cell
and a length and returns a status the generated code branches on. Index reads and
`for` iteration both use that emitter, so both paid it per element.

`mmay_native_c_dense_scalar_stride` now returns the 4-byte stride for
`MMAY_TYPE_RUNE`. A rune element read becomes the same inline
`mmay_managed_array_generated_get_scalar` load that `[]u32` already used, behind
the same direct bounds check, and the element store becomes the same inline
store; the value conversion (`(uint32_t)cell.f64_bits`) is the one both paths
already emitted. No runtime file, descriptor, storage kind, layout or bytecode
changes: the storage was already dense RAW32 with stride 4, and the VM already
read it through `mmay_managed_array_view_get_raw64_fast`.

The same predicate also decides whether a scalar store loop may be fused into
batched pushes and whether a nested array may use the snapshot cache. Both were
already available to `[]u32`, which shares the storage, so rune arrays join
existing, differently measured paths rather than a new one.

## Correctness

| Check | Result |
|---|---|
| full Debug suite, serial | 99 of 100 passed |
| ASan+UBSan `managed-memory`, `resource`, `stateful` | 6/6 passed |
| ASan+UBSan `c_unit`, `cp33` by name | 20/20 passed |
| focused emitter test | passes with the change, fails without it |
| corpus matrices, all 32 rows | identical stdout, identical expected checksums |

The focused test
(`tests/c-unit/test_native_c_emitter_array_access.c`, `a4` case) hand-builds a
`[]rune` array with `ARRAY_GET` and asserts the emitted program contains
`mmay_managed_array_generated_get_scalar(` and no
`mmay_managed_array_view_get(`. With the stride clause stashed the same test
fails both assertions, so it guards this behaviour rather than describing it.

`mmay_macos_package` is the one failing test: it stops in
`test_real_pkgbuild_and_pkgutil_preserve_the_complete_prefix` with "compiler/runtime
Mach-O architecture does not match package". It fails the same way with the
emitter clause stashed, so it is a pre-existing packaging-environment mismatch,
not an emitter effect.

## Measurement

**Screening probes.** The same three probe sources, compiled by the previous and
the current release compiler with their own native-runtime archive, three
warmups and alternating fresh processes per side.

| Probe | Scale | Before ms | After ms | Delta |
|---|---|---:|---:|---:|
| bare iteration, no array body | 450,000,000 elements | 1792.2 | 560.8 | **−68.71%** |
| the same loop plus one `[]rune` push per element | 450,000 pushes | 8.509 | 7.389 | **−13.16%** |
| the same loop plus one `Map<rune,int>` get+put | 450,000 pairs | 20.226 | 18.402 | **−9.02%** |

Bare iteration falls from about 4.0 ns to 1.25 ns per element, which is where the
whole mechanism lives: the loop probe has no map and no allocation in its body,
so all of that 2.75 ns was the per-element runtime call. The push and map probes
carry the same iteration, so they move by the same absolute amount and a smaller
share.

**Corpus rows, paired.** The two matrices pin their own executables, so every row
was re-measured from both sides in one interleaved sequence, three warmups and
eleven alternating samples, fresh process per sample:

| Row | Before ms | After ms | Delta |
|---|---:|---:|---:|
| `rune-frequency` | 23.029 | 20.741 | **−9.94%** |
| 31 other rows | — | — | −5.36% … +3.72%, mixed signs |

Every row's stdout is byte-identical between the two binaries. The spread on the
untouched rows is code-layout and host noise: the mechanism only reaches element
access on `[]rune`, and no other corpus row iterates one.

The all-row matrix `perf26-c7-rune-dense-anchors` reads 19.98 ms against 6.59 ms
of Go, 3.03×. That ratio should not be compared with the previous matrix's 3.48×
directly: the whole host ran about 5% slower this hour, which also moved the Go
column from 6.26 to 6.59 ms. On the paired row delta the ratio becomes about
3.1× Go. Since the timebox opened the row has moved 23.20 → 20.74 ms.

## A rejected neighbour

Before this candidate, the last named map lever was measured: comparing stored
and query string keys inline for keys of at most sixteen bytes instead of
entering `_platform_memcmp`. Two fixed-width loads per side looked cheaper than
the call. The map probe came out **3.98% slower** against an A/A control of
−0.63%, so the change was reverted and nothing shipped. The platform routine
already compares those lengths with the same two-window trick, and the hand
version adds branches. The string map's key comparison is therefore not a
remaining lever.

## What this does not prove

No Tier 1 baseline, no VM or Bun column, and `rune-frequency` is still about
3.1× its Go column. `unique-set` and `frequency-map` did not move: neither
iterates a `[]rune`, and their own element types were already dense. The
mechanism says nothing about the per-call validated-handle boundary that the
remaining scalar-keyed cost sits in.

## Where the time goes now

`rune-frequency` remains mostly map work: 450,000 `Map<rune,int>` get+put pairs
at roughly 36 ns, one validated runtime call boundary per pair, with iteration
now at about 1.25 ns per element instead of 4.0. The next named hypothesis for
this row is not inside the emitter.
