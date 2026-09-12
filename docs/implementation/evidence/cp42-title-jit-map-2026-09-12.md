# Accepted title JIT: mapped machine-code costs

Status: **diagnosed locally**, on `c4acd2efc9a5656cee8156d01ffbf0121f13d348`. Production is
unchanged from accepted string publication `613b703`. The
[accepted all32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
remains current; title-family, original all32 parity, complete Tier1 and Linux
LeakSanitizer remain open. JIT remains opt-in.

[Protocol and compact results](cp42-title-jit-map-2026-09-12-summary.json),
[reproducible sources, exact code bytes, maps and profiles](cp42-title-jit-map-2026-09-12-raw.json.xz).
The archive stores each file with its encoding and SHA-256. Compression and all
decoded file identities were verified before publication. No instrumented
duration is performance acceptance evidence.

## Capture and attribution

The previous profiles left JIT addresses anonymous. This diagnosis hooks only
successful publication and pre-disposal in an ignored copy of `vm_jit.c`.
The accepted emitter, runtime and safety checks are unchanged. Existing entry
offsets supply every original image instruction boundary; no extra machine-code
labels or runtime probes enter the search loop. Each publication captures exact
code bytes, address/size, iteration, function symbol, operations, operands,
source spans and time. Each release is paired with that active address.

Two independent process trials for each original title size run in each of two
variants: sampling and separate allocator-ledger checking. Each repeats the
original main 64 times, including preparation and teardown, preserving arguments,
stdout and the 64MiB execution heap. All **512 original results** agree.
Every one of **256 ledger returns** has zero central-allocator live allocations
and live bytes. All **24,064 code publications/releases** pair correctly, with
47 selected functions per iteration. Captured code totals 64,744,480 bytes.
This pairing is lifecycle evidence, not a claim that all external resources or
process RSS return to zero. Diagnostic stdio buffers are outside these ledgers.

macOS `sample` captures one second at 1ms intervals, with `-mayDie`. Reused
addresses are mapped only when **every containing lifetime** agrees on the
function symbol and complete original operation identity. Otherwise an address
would remain ambiguous. All **434 anonymous leaf observations** map uniquely;
there are zero ambiguous or unmapped observations in these four profiles.
The sampled four-byte instruction itself is identical across every containing
lifetime. Wrapped Mach-O text was byte-verified against captured code before
`otool` disassembly. Raw artifacts retain both wrapping and disassembly commands.

The call-tree parser subtracts direct child counts to obtain leaf observations;
it separately verifies their sum against thread totals. Anonymous counts >=5
independently agree with the sampler's collapsed top-of-stack summary. Caller
return addresses are not reinterpreted as leaf instruction PCs, and inclusive
helper time is not added again to its caller's leaf time.

## Measured ownership

| Original size / trial | All thread samples | Anonymous JIT leaves, all mapped | string_equal | array_require_index | i64_less + branch_bool | array_get_in_bounds | C string-load bridge leaves |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10k-0 | 826 | 18 | 6 | 6 | 2 | 4 | 15 |
| 10k-1 | 852 | 38 | 14 | 10 | 9 | 4 | 37 |
| 100k-0 | 843 | 180 | 80 | 36 | 43 | 16 | 138 |
| 100k-1 | 842 | 198 | 86 | 46 | 39 | 20 | 152 |

The dominant original function is symbol `a97fab987fdc9664d94fdb37dfa88596`,
function index 48. Its spans 452–459 and 471–494 match the original loop bound
and `values[index] == target` expression in catalogue `core/text.my`.
The byte-span match and source hash are retained in the summary.

- PC55/56, offsets `0x76c`–`0x788`: load index and limit, compare, materialize
  the boolean in a VM register, reload that same boolean, then branch.
- PC57, offsets `0x78c`–`0x8f4`: bind the array, check the original unsigned
  index/length predicate, publish the complete array reference and invalidate
  the destination view if its identity changed.
- PC58 starts at `0x8f8`: bind that same canonical array again, then obtain the
  selected string through the accepted checked C projection. The successful
  projection publishes the complete reference and validated byte binding.
- PC59 starts at `0xa3c`: bind both strings, compare their lengths and call
  `memcmp` when appropriate; save the resulting boolean, followed immediately
  by a VM-register reload for PC60's branch at `0xc88`.

These offsets describe the archived `code-35.bin` blob in each sampling
profile, with function index 48; blob ID and function ID are distinct.
The largest individual JIT leaf, offset `0xc5c`, lies just after the conditional
length branch. This identifies the comparison area, **not** proof that the
constant move there is expensive, a measured branch-misprediction rate, or a
hardware-cache explanation. Ordinary PC sampling can have attribution skid.

Source preparation remains separately visible. The first 10k profile contains
399 `_dyld_start` leaves out of 826 total observations. The map writer itself
also adds open/write/close work during compilation, visible in the profiles.
These startup and diagnostic costs are preserved; no favorable replacement
sample is substituted. This diagnosis locates unchanged emitted instructions,
not their uncontaminated end-to-end timing. The earlier accepted repeated-entry
profiles independently showed the C bridge at 142/842 and 151/842 leaves, compared
with 138/843 and 152/842 here.

## Changed next action

The [rejected publication-marker candidate](cp42-string-array-projection-v3-2026-09-12-rejected.md)
removed over 98% of invariant header stores without a useful VM speedup. That
rejection remains intact. The next VM candidate should instead avoid repeated
work that the mapped instruction sequence exposes:

1. Remember a successfully checked view during emission of one uninterrupted,
   nonallocating segment. A second use of the same unchanged canonical register
   can use that proof. A successful checked string projection may establish
   the destination proof. Every overwrite, independent entry, cold boundary,
   source call/return and relevant mutation must invalidate the appropriate
   facts. A miss still executes the complete existing checked binding owner.
2. Forward a just-produced boolean to an immediately adjacent branch when
   the emitter proves that the same scratch value survives and the branch is
   not an independent entry. Preserve the complete existing VM register store;
   only its redundant immediate reload may disappear.

These are emission-time facts, not live object caches across safepoints. They
must retain all logical register materialization, child checks, original failure
PCs and exact finite/shared fuel. New focused actual-JIT/cold tests must cover
overwrites, aliases, branch joins, call/return, heap epochs, malformed references,
allocation-failure cleanup and rejected proofs. Declare a fresh VM-only SMART
and fixed early/full32 gates before implementation and timing. Native production
remains unchanged by this proposed step; its parity work remains required.

No such optimization is implemented or accepted by this diagnostic record.

Protocol began 2026-09-12T09:55:18.150666+00:00; due 2026-09-12T10:30:18.150666+00:00. Evidence prepared
2026-09-12T10:05:26.831710+00:00, deadline met: **True**. Actual commit/push time
is recorded separately in the stage publication record.
