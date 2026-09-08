# CP42 nonfatal mutable-capture diagnostics

Scope: local macOS arm64 implementation evidence, 2026-09-09, parent `666911d`.
[Raw commands, outputs and source hashes](cp42-escape-warning-2026-09-09.json)
identify the candidate. D103 closes the diagnostic removal gap from D098;
it does not close full CP42 or certify unobserved Tier 1 hosts.

T0020 reports a possible escape once per closure source span. Warnings retain
checked functions and continue subsequent files/packages, check/run/build/test
and native artifact creation. Errors still prevent successful compilation.
The existing human/JSON renderer receives the actual severity. Diagnostics
share the existing budget; resource and allocation failures remain failures.

The checker owns lexical capture facts, flow-insensitive aliases and
field-sensitive containment. A monotone scope worklist covers returns, outer
storage, arguments, arrays, enums, records, maps and cyclic objects. Ordinary
calls can retain arguments and return aliases to retained storage. Direct
function-value invocation does not retain the callable itself; captured values
may be exposed by its body. Branch/reassignment union, array element merging
and lack of interprocedural no-retention summaries are deliberate conservative
limits recorded in D103. No claim of exact runtime escape detection is made.
Boxing, managed-cell identity/lifetime, GC roots and backend semantics do not
change; the analysis is not retained in published MIR and is disposed on all exits.

Focused tests cover warning and no-warning cases, exact code/severity/byte span,
shadowing, generic source deduplication, local aliases, map traversal and cyclic
records. Every reachable checker allocation-failure index is swept, including
multi-file/multi-package continuation; complete teardown checks live_allocations
and live_bytes. A work-budget sweep verifies defensive limits. Public source
runs three tests with each engine and both, with 31 process observations across
human/JSON, VM, native Debug/Release, precise warning prefixes, cleanup/panic,
continued imported declarations and warning plus a real type error. Existing
function-value, mapper and array-search fixtures preserve complete runtime
output expectations while separately checking compiler warning counts/prefixes.

The full Release invocation passed 84/84 in 288.02 s, including the native
shard in 270.97 s. While that invocation was running, a focused audit added
the missing alias edge across collection generation checks and two regression
cases (map traversal and a cyclic record). The final checker/public recheck
passed 2/2 in 10.21 s. Thus the full run is broad regression evidence alongside
final focused coverage, not a claim that every full-run process used the final
binary. Exact commands, raw outputs and final source hashes are retained.
The final focused macOS ASan/UBSan checks passed 2/2 in 28.34 s and cover
checker ownership and the public process fixture. Linux LeakSanitizer and other required Tier 1 hosts remain
CI/host obligations. No release gate or budget is waived. Next: delimited and
source-lint, then complete portfolio CP42 → CP41 → CP39 → CP40/N0 acceptance.

Vladimirov: printed 62–63 / PDF 63–64 supplied fixed-point iteration principles;
printed 65–68 / PDF 66–69 supplied monotone finite data-flow and precision limits.
The graph and warning rule are a Marimay adaptation, not a book escape algorithm.
[Application journal](../../research/vladimirov-optimizing-compilers-2026-09-07.md)
records the concrete mapping. This is diagnostic work, not a performance claim.
