# CP42: ordinary Result mappers and native root lifetime

D100 replaces the special inline mapper body with ordinary D098 callable
preparation and invocation. Receiver and mapper expression each run once, in
that order, before variant selection. Only Err invokes the resulting callback.
Stored, returned, indexed, field and selected values compose with generic
wrappers; annotated block closures own their return, try, defer and panic scope.
D094's narrowly contextual expression closures and enum constructors remain.
No new wire opcode or runtime API is introduced.

## Root correctness

The forced-GC engine fixture reproduced a native failure hidden by ordinary
process runs: unioning all COPY instructions across a function treated distinct
branch assignments as one stable root. An Err holding a returned closure lost
its actual root to an unexecuted Ok-branch register; another mapper allocation
then collected the still-live value. Generated native code trapped at both -O0
and -O2, whereas MIR and VM produced the expected result. Saved local artifacts
are in `build/d100-isolate/`; the checked-in fixture is
`tests/c-unit/test_mapper_value_engines.c`.

The native root owner now proves activation-wide stability with saturating
definition counts and linear CFG elimination. Registers defined more than once
or in/after a cycle are conservatively kept independent. Stable exact identities
and immutable enum ownership may retain another root. Mutable container
projections keep their own roots: a callee or alias may replace their children.
This corrects D063 lifetime implementation without changing source semantics.

`test_native_c_emitter_roots.c` additionally reproduces branch joins, rebinding,
loop iterations and record/array child replacement through callees. Both focused
programs now pass MIR/VM/native differential, forced collection and reachable
allocation-failure sweeps. All complete allocator-backed teardowns assert both
zero live allocations and zero live bytes. The old/new reproducibility logs are
`build/d100-roots-before.log` and `build/d100-roots-after.log`.

## Verification and consumers

Ten public tests cover callback storage, evaluation order, generic composition,
mutable capture, identity, lexical shadowing, void errors and contextual try.
The process fixture checks formatter, checker, VM/native tests, native Debug and
Release, factory/callback panics, exact cleanup order and callback test-failure
source ranges. Checker tests sweep valid/invalid calls and resource ceilings.
Frontend and formatter seeds were replayed; no new fuzz campaign is claimed.

Notes prepares its collection-growth adapter once before validating all IDs.
Config reuses one typed output adapter for write and flush. Notes passes 16
source tests on both engines, 36 process cases across VM/native Debug/Release
and 21 closed-pipe checks. Config passes seven source tests, 32 process cases
and 23 filesystem cases across those engines. Outputs, files, expected failures
and atomic publication assertions are unchanged. Only allowlisted source files
were snapshotted; no private Notes database was read or copied. Existing external
repository changes remain uncommitted. The exact new consumer delta is retained
in `build/d100-consumer.patch` and the machine-readable evidence packet.

The [machine-readable packet](cp42-mapper-values-2026-09-08.json) records source,
binary and log hashes, commands, consumer deltas and raw measurements. Release
initially passed 80/81 in 372.49 s: the native shard's only failure was an obsolete
synthetic-function count (two separate mapping functions each own an adapter).
After correcting that expectation to ten, the complete native shard passed in
218.93 s; all 81 Release tests therefore have successful final results.

macOS ASan/UBSan initially passed 79/80 in 786.04 s, with exactly the same
count assertion and no other native-shard failures. The rebuilt, sanitized
generic differential fixture passed separately in 3.23 s. The complete sanitizer
suite was not repeated after that test-only correction; this is explicitly not
an unqualified 80/80 sanitizer-suite claim. Architecture, memory policy, roadmap
and the remaining documentation/generated-artifact gates pass. CP42 and full
Tier 1 acceptance remain open.

The unchanged synthetic dataset has 6,098 notes and 1,047,786 bytes. A rotating
native Release sample uses one warmup and six measured runs per phase, with
exact stdout and file-byte checks. It compares the retained D099 artifact with
D100 before/after hoisting the reusable adapter. D099 → final D100 medians are
144.56 → 144.05 ms for show-last and 236.21 → 236.20 ms for remove-last; median
peak RSS is effectively unchanged (about 61.2/70.9 MiB). Unhoisted D100 gives
144.80/238.53 ms. These small differences do not establish a speedup and do not
replace named CP42 performance acceptance. No concurrent test workload ran
during measurement.

## Explicit remaining limits

A mapper declared to return `never` currently gives I0001: common enum transport
does not yet support `Result<T, never>`. D100 records the common-layout removal
gate. Diverging mapper expressions, and inhabited-result callbacks whose bodies
panic, work with ordinary evaluation and cleanup. The nonfatal escaping mutable
capture warning remains the separate D098 callback conformance gap. Neither
limitation is represented as complete by this slice.

No new idea from Vladimirov's book was used: this repair follows the local MIR,
GC reproducer and existing D063 contract. Earlier book-derived ideas and exact
pages remain in the research journal.
