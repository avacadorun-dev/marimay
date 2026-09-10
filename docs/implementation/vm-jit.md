# Optional VM machine-code tier

The C17 bootstrap includes an opt-in SLJIT adapter under
[D111](../design/decisions.md).
It consumes the verified VM image and original typed operands. Source programs,
bytecode bundles and the `mmay run` / `mmay test` interface stay unchanged.
The ordinary interpreter remains required. `MMAY_ENABLE_SLJIT` defaults to OFF
pending complete conformance and platform acceptance.

## Build

Supply an explicit local copy of the pinned BSD-2-Clause dependency:

```sh
git -c core.autocrlf=false clone https://github.com/zherczeg/sljit.git build/sljit-source
git -C build/sljit-source checkout --detach d6ec3d07dcdbf2256ccae2b84d0d8a2d019f392e
cmake -S . -B build/jit -DCMAKE_BUILD_TYPE=Release \
  -DMMAY_ENABLE_SLJIT=ON -DMMAY_SLJIT_SOURCE_DIR="$PWD/build/sljit-source"
cmake --build build/jit -j4
ctest --test-dir build/jit -j2 --output-on-failure
build/jit/mmay run examples/numeric_cp6.my
```

CMake requires Python 3 for the opt-in preparer. It never fetches dependencies.
The preparer verifies all 35 named upstream inputs, including the license,
against [the manifest](../../cmake/sljit-source.json), copies them into the build
directory and applies the recorded ARM64 INT64_MIN patch. It verifies the final
patched digest and installs Marimay's allocation hooks. The supplied checkout
is unchanged. Enabled installations include the upstream license.

Use `-DMMAY_ENABLE_SLJIT=OFF` for a dependency-free interpreter build. The
coordinator compiles out JIT state and calls in that configuration.
On macOS, `-DCMAKE_OSX_ARCHITECTURES=x86_64` builds the same adapter for x86-64;
Rosetta execution is functional evidence, not physical x86 performance evidence.

For instrumentation, add `-DMMAY_ENABLE_ADDRESS_SANITIZER=ON` and
`-DMMAY_ENABLE_UNDEFINED_SANITIZER=ON`. Both owned code and SLJIT are instrumented.
Linux CI runs the full enabled suite with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Local macOS runs disable
leak detection and cannot replace the Linux LeakSanitizer gate.

## Execution and ownership

The first tier admits bounded functions with a control-flow backedge and emits
nonallocating scalar, array and nominal-read segments. Its arithmetic generator
requires exact signed i64 operands. Historical I64-named wire instructions may
carry other integer widths; these retain ordinary typed execution. Calls,
allocation and unsupported instructions return to the existing VM owners.

VM registers remain the authoritative state. A side exit restores the original
source PC and executes the uncommitted instruction through ordinary dispatch.
Borrowed managed views expire at every return to the VM. Existing logical
frames, roots, heap/capability limits and panic reporting remain authoritative.

Finite frame fuel and every non-null shared counter are charged exactly. Only
D087's unlimited-frame/no-shared-counter mode omits unused segment accounting.
Unmetered code refuses entry with incompatible options. D109's composed array
shift retains its existing original-instruction accounting and source exits.

Compiler and staging allocations use the central allocator. The central
`base/code_memory.c` owner reserves at most 8 MiB of page-rounded code per
invocation, publishes it once with platform W^X/cache synchronization, and
releases every mapping at teardown. Unsupported hosts or denied executable
publication retain ordinary execution; actual allocation failures remain
observable. Published code is not serialized, shared between invocations or
patched in place.

The [architecture contract](compiler-architecture.md) names each owner. Focused
tests assert actual JIT admission, compare original dispatch across numeric
boundaries and precise fuel/traps, cover signed record and wide enum selectors,
and exhaust reachable allocation/publication failure indices with zero ledgers.
These checks do not establish Go/Rust parity or complete Tier 1 acceptance.

## Performance evidence

Use the unchanged external 32-workload corpus and the recorded 114 source/harness
hashes with `tools/performance/compare-vm-preparation.py`. Keep source loading,
checking, lowering, process startup, arguments and the 64 MiB managed-heap budget
in every observation. Compare pinned compiler snapshots with 3 warmups and 11
samples; do not time while builds or tests compete for CPU.

The historical [prototype](../../tools/performance/sljit-prototype/README.md)
remains pinned to its pre-integration commits. Current production owners are
built directly through CMake; rewriting the old adapter's hashes would destroy
the historical reproduction boundary.
