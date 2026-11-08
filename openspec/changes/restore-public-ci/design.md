# Design

## Context

See [proposal.md](proposal.md) for the problem and requirement links.
The [compiler architecture](../../../docs/implementation/compiler-architecture.md)
assigns independent C17 owners to storage, parsing, VM execution, native
emission/builds, and platform adapters. The concurrent `main` update adds
OpenSpec and complementary portability tests; integration preserves both sets
of meaningful tests.

## Goals / Non-Goals

**Goals:** Preserve the declared source behavior and exact VM/native bytes while
making compiler, allocator, toolchain, and test orchestration contracts portable.
Retain strict warnings, sanitizers, deterministic failure injection, and narrow
ownership throughout the repair.

**Non-Goals:** New language features and product milestone qualification.
OpenSpec records this repair against the existing normative contracts.

## Decisions

1. **Centralize fundamental alignment.** Use one `MarimayMaxAlign` definition
   with the standard type where available and a statically checked MSVC C
   fallback. Arena, map, and JIT storage share it. Retain both mixed-allocation
   and pure-block growth tests, nonzero aligned map payloads, error cases,
   complete failure-index sweeps, and zero allocation/byte counters.
2. **Keep Windows host details in their owners.** Serialize simple arguments
   bare and quote complex arguments exactly; test round trips. Native builds
   select the matching CRT, sanitizer/debug metadata, and cache-local object
   output. Initialized const descriptors use matching forwards in the single
   generated translation unit. CLI/native entry points own binary standard
   streams, and fixtures explicitly choose their intended bytes.
3. **Preserve parser and formatter contracts.** Retain an already reached
   parser limit during recovery. Compare accepted optional separators in the
   AST oracles, including negative comparisons and required singleton tuple
   commas. D071 reconciles the existing grammar/formatter wording; the fuzz
   harness and source-language behavior remain the acceptance oracles.
4. **Own JIT entry-probe memory.** Reserve a readable zero prefix, account for
   the complete mapping, and protect/publish/free the actual mapping bounds.
   This supports instrumented indirect calls without disabling function
   sanitization. Exercise overflow, relocation, immutable bytes, and all
   allocator/platform failure indices.
5. **Keep test and security gates explicit.** Use the central MSVC runtime-check
   helper, full CTest suites after native preflight, complete submitted-history
   scans, and failed-test evidence capture. Seven individually bounded IPO
   configurations receive an aggregate budget of 240 seconds and flushed
   progress. A successful setup or a cancelled run is not passing conformance.
6. **Use OpenSpec for traceability.** Link accepted decisions, normative text,
   focused owners/tests, and evidence rather than copying the language spec.
   The change stays reviewable alongside the PR; roadmap verification continues
   to follow its existing evidence rules.

## Risks / Trade-offs

- Best-effort platform removal can leave a cache-local intermediate after an
  allocation or filesystem failure. Document that limit; require exact valid
  artifacts and zero allocator counters after teardown.
- A passing fuzz campaign does not prove absence of defects. Keep focused
  regressions, negative cases, and the unchanged sustained harnesses.
- Platform results are revision-scoped. Preserve successful results on an
  unchanged revision when retrying interrupted jobs, and verify an integrated
  revision again before claiming its matrix is green.

## Migration Plan

Merge the concurrent `main` changes, resolve overlapping owners, and run strict
local builds, focused tests, OpenSpec validation, and required repository gates.
Publish the integrated branch and require the full platform, sanitizer, JIT,
fuzz, and source-security checks for that head. Record exact final job links
and revision in the PR description. Any regression keeps its gate red and is
repaired or reverted through the same review process.
