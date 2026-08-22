# Checkpoint 24: dense raw scalar array storage

Date: 2026-08-22

Scope: internal managed-array backing representation and exact accounting; no
Ruda source syntax, type identity, MIR operation, portable bytecode row, public
capacity/stride observation, or VM/native semantic change.

## Decision and representation

Accepted D060 replaces the uniform `RudaManagedCell` backing stride with one
closed descriptor-selected storage tag. The cell remains the D059 transport
value and the representation for reference elements; raw payload storage is
byte-addressed and copies through aligned fixed-width locals with C17 `memcpy`.

| Element family | Storage tag | Exact stride | Alignment contract | Same 64-byte payload budget |
|---|---|---:|---:|---:|
| `bool`, `i8`, `u8` | `RAW8` | 1 | 1 | 64 elements |
| `i16`, `u16` | `RAW16` | 2 | 1 | 32 elements |
| `i32`, `u32`, binary32 payload | `RAW32` | 4 | 1 | 16 elements |
| `i64`, `u64`, `f64` | `RAW64` | 8 | 1 | 8 elements |
| managed references | `CELL` | `sizeof(RudaManagedCell)` | `_Alignof(RudaManagedCell)` | 4 elements on the measured 64-bit ABI |

On the Apple M2 test ABI a `RudaManagedCell` is 16 bytes. The focused budget
test derives the fixed object/slot base cost from an empty array, then proves
that `base + 64` admits exactly 64/32/16/8 raw elements and rejects the next
element. Thus raw64 doubles payload capacity relative to the former uniform
cell storage; raw8/raw16/raw32 improve it by 16x/8x/4x. Live-byte accounting
charges `capacity * descriptor.storage_stride`, and the CP19 exact resource
fixture changes its requested-byte observation from 344 to 320 accordingly in
both human and JSON VM/native paths.

The descriptor rejects invalid tag/stride/cell-class/reference combinations
before allocation or exact access. Growth uses checked byte arithmetic,
preflights the exact added bytes, zeroes the new byte range, and publishes the
new pointer/capacity/accounting only after a successful resize. Truncation and
pop clear removed bytes. Only the initialized prefix of `CELL` storage is
traced; raw payload bits are never interpreted as references.

`runtime/common/managed_array_storage.c` is the 540-line representation owner.
The 82-line owner-private `managed_array_storage_internal.h` contains only the
validated-callers-only inline byte-copy primitive. This avoids adding a second
out-of-line call inside the separately linked D059 exact-access ABI. The
existing growth policy is unchanged; `push` merely avoids re-resolving the
same array when its already validated capacity is sufficient.

## Bit, identity, tracing, and failure coverage

The new focused owners add the following direct coverage:

- `test_managed_array_storage.c` covers all 1/2/4/8-byte layouts at zero,
  signed boundaries, maximum values, alternating/high-bit patterns, binary64
  negative zero, signaling/quiet NaN payloads, odd length five, empty storage,
  alias-visible set/get, reserve, pop clearing, and exact private byte counts;
- corrupt tags, strides, raw/reference classes, and a descriptor corrupted
  after publication are rejected before access; no invalid constructor makes
  an allocation attempt;
- raw handle-like bits do not retain an object, a reference written outside
  the initialized prefix does not retain an object, and an initialized nested
  reference does retain it through collection;
- constructor failure is swept through every deterministic allocation index;
  resize failure preserves handle, descriptor, length, capacity, contents,
  live bytes, and mutation generation, followed by
  `live_allocations == 0 && live_bytes == 0` after teardown;
- `test_bytecode_vm_array_storage.c` encodes, decodes, verifies, and executes
  the existing generic-array table for every raw scalar kind, checking the VM
  descriptor stride and exact carried bits. F32 uses an empty typed array
  because the current bytecode has no F32-literal opcode; RAW32 payload bit
  preservation is covered directly by runtime and native scalar tests;
- `test_native_c_emitter_array_storage.c` checks every emitted scalar mapping,
  the canonical f64 descriptor, and `[]string` CELL fallback, then compiles and
  executes the generated program under strict C17. Existing checked/proven,
  mutation, panic/span, O0/O2, VM/native, and CLI differential suites run
  through the new representation unchanged.

No bytecode codec or opcode table changed. No formatter fixture or public
example changes are applicable because D060 introduces no source spelling.

## Performance record

The final D056 11-sample/3-warmup report was collected with the release runtime
on macOS arm64 Apple M2 using AppleClang 17. It is stored in the sibling corpus
as `benchmarks/results/cp24-probes-final-macos-arm64-apple-m2.json`, SHA-256
`87c817962cea3452b5b2db4cf11ee8e3e7f4f47176b094545a817c2ae1f71c66`,
and pushed by benchmark commit `1e7dbf7`.

| Probe | CP23 median ms | CP24 final median ms | Change | MAD / IQR ms | CP21 ceiling ms |
|---|---:|---:|---:|---:|---:|
| scalar-get | 79.210 | 88.738 | +12.03% | 0.571 / 1.604 | 106.652 |
| scalar-set | 84.423 | 104.079 | +23.28% | 1.043 / 2.233 | 139.852 |
| proven-bounds | 195.138 | 228.214 | +16.95% | 1.427 / 3.997 | 290.579 |
| unproven-bounds | 219.362 | 220.390 | +0.47% | 1.398 / 2.902 | 284.290 |
| growth | 87.774 | 97.371 | +10.93% | 1.357 / 3.473 | 97.716 |
| reserved-growth | 85.272 | 91.768 | +7.62% | 0.578 / 1.481 | 96.434 |
| density | 331.230 | 381.129 | +15.06% | 1.878 / 5.142 | 471.215 |
| nested-traversal | 168.893 | 192.577 | +14.02% | 1.009 / 2.908 | 233.059 |
| checked-loop | 309.733 | 311.934 | +0.71% | 1.902 / 4.588 | 341.839 |

The representation checkpoint does not claim a whole-matrix speedup relative
to the unusually fast CP23 local run. It does prove the exact storage-density
and same-budget capacity change, and every final probe remains below its
reviewed CP21 local ceiling. Growth has the narrowest remaining headroom at
0.35%; it is therefore recorded explicitly rather than rounded into a broad
performance claim.

The initial implementation exposed a real avoidable overhead: an extra
out-of-line storage call plus repeated descriptor validation on every access,
and a redundant mutable resolve on a push that already had capacity. The raw
attempt and its permitted cooldown retry are preserved as
`cp24-probes-macos-arm64-apple-m2.json` (SHA-256
`b6a897091a7f46a50b51c8b493ed74bb4a179c0a28c7add7b521a4078b8764d9`)
and `cp24-probes-retry-macos-arm64-apple-m2.json` (SHA-256
`d8d106acf631d90c298e9d451dd5ed4a3b937b243abbc3d2e9e5be0012eac880`).
They measured growth at 101.116 and 98.476 ms, both above the 97.716-ms
ceiling. After removing only that duplicate internal work, the final distinct
compiler artifact measured 97.371 ms; pre-optimization reports were not
discarded or substituted.

The checkpoint-21 paired validator intentionally requires a probe and
algorithm report from one compiler revision. CP24 changes only the compiler
and collected focused probes, so no incompatible paired-baseline pass is
claimed. This Apple M2 result is Tier 1 local evidence; the separate D057
five-platform public-alpha registry remains incomplete.

## Validation record

The final optimized tree passed on macOS arm64 with AppleClang 17:

| Validation | Result |
|---|---|
| `./tools/check-architecture` | passed; owner, dependency, registration, and line budgets clean |
| `./tools/check-c-memory-policy` | passed; central allocator boundary clean |
| `./tools/check-generated` | passed; bytecode, diagnostics, Unicode, and syntax current |
| doc/spec/example/syntax gates | passed; 75 syntax kinds and 116 grammar productions covered |
| debug C unit + install relocation | 2/2 passed, 305.27 s |
| release C unit + install relocation | 2/2 passed, 320.85 s |
| ASan C unit + install relocation | 2/2 passed, 390.23 s |
| ASan+UBSan C unit + install relocation | 2/2 passed, 337.63 s |
| focused post-optimization debug C unit | passed, 234.57 s |
| final D056 probes | 9/9 observations valid; 11 samples and 3 warmups each; all CP21 local ceilings passed |

This verifies checkpoint 24 on the local Tier 1 host and does not claim that
I2-I5, the public-alpha platform registry, or the complete language roadmap is
verified.
