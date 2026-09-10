# Shared SLJIT VM experiment

This experiment reuses the previous ARM64 prototype's region plan, VM bindings,
source-PC mapping and fuel accounting. A single C adapter delegates machine
instruction generation to [SLJIT](https://github.com/zherczeg/sljit), pinned at
`d6ec3d07dcdbf2256ccae2b84d0d8a2d019f392e` (BSD-2-Clause). No handwritten
ARM64 or x86-64 emitter is carried forward. These sources remain diagnostic
tooling, excluded from production CMake and ordinary `mmay run`.

## Reproduction

From this Marimay snapshot, with a matching baseline archive and clean SLJIT
checkout:

```sh
git clone https://github.com/zherczeg/sljit.git build/sljit-evaluation
git -C build/sljit-evaluation checkout d6ec3d07dcdbf2256ccae2b84d0d8a2d019f392e
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j4
python3 tools/performance/sljit-prototype/build.py \
  --sljit build/sljit-evaluation --unit --memory-test
build/sljit-prototype/test_memory
build/sljit-prototype/test_jit_ownership
mkdir -p build/sljit-prototype/suite-2
(cd build/sljit-prototype/suite-2 && ../mmay_c_unit ../mmay --suite-shard 2)
python3 tools/performance/sljit-prototype/check.py \
  --mmay build/sljit-prototype/mmay --benchmarks ../algorithm-benchmarks \
  --reference docs/implementation/evidence/cp42-vm-preparation-comprehensive-2026-09-09.json \
  --output build/sljit-prototype/all32-correctness.json
```

The builder verifies baseline coordinator/image, prototype and patch hashes;
patches copies under `build/`; records commands, archive/source/compiler hashes;
and copies generated configuration, native runtime and the SLJIT license.
The upstream checkout stays clean. `--arch arm64` / `--arch x86_64` select a
macOS target with a matching baseline archive. For x86-64, first configure the
baseline with `-DCMAKE_OSX_ARCHITECTURES=x86_64`. `--sanitize` instruments the
adapter, owners and SLJIT with ASan/UBSan and requires `--build` pointing at a
matching instrumented baseline. It does not suppress SLJIT diagnostics.

Use `tools/performance/compare-vm-preparation.py` with the immutable reference,
a pinned control and this compiler. It retains all 32 workloads, source-run
preparation, arguments, heap limits, checksums, 3 warmups and 11 samples.
`check.py` checks correctness only and makes no timing claim.

The first feasibility report describes the older experiment at commit
`0145177bd4c6366128a2039fd094ba1fede30f3a`. Reproduce its exact candidate from
that snapshot; current sources include the ownership and composed-loop changes.

## Owners and contracts

- `vm_jit.c` owns invocation-scoped metadata, code and bindings. Preparation
  failures destroy previously compiled functions. Unsupported executable memory
  falls back completely; a code-budget limit skips that function; allocation
  failure remains observable. The 8 MiB code limit counts page-rounded mappings.
- `vm_jit_plan.c` selects bounded supported segments without source/name-based
  specialization. `vm_jit_sljit.c` lowers them to the shared backend. Calls and
  unsupported instructions return to the ordinary VM with an exact source PC.
- `vm_jit_bind.c` validates borrowed array/record storage. Every return to the
  VM expires binding epochs. Within a segment, a managed COPY/read preserves
  its binding only when all provenance, slot and generation bits are identical.
- `vm_jit_loops.c` reuses D109's nonallocating shift-loop owner before generic
  segment charging. It commits the exact consumed prefix and returns to the VM
  after progress, expiring bindings. The existing finite frame/shared-fuel
  differential tests exercise this composition.
- `jit_memory.c` routes compiler arenas and temporary code buffers through the
  central allocator with aligned size headers. SLJIT emits into non-executable
  staging with relocations computed against the reserved executable address.
  Publication validates both staging and destination lengths, frees staging,
  and transfers the final mapping to its function owner.
- `code_memory.c` owns page-rounded reservation, one-way publication and release.
  Linux/macOS x86 use RW-to-RX protection; Windows uses VirtualAlloc/VirtualProtect;
  macOS ARM64 uses MAP_JIT and an allowlisted write callback, then cache flush.
  No generic RWX fallback exists. Unsupported hosts, including arm64e, use the
  interpreter. Unexpected failure to release an owned mapping aborts as an
  ownership violation; it cannot falsely clear allocation ledgers.
- `sljitConfigPre.h` installs the hooks. Its executable-offset configuration is
  required for final-address relocation even with the upstream executable
  allocator disabled. Library cache/protection hooks affect staging only; the
  platform owner publishes and flushes the actual mapping. Published code is
  immutable; live patching and upstream code-free APIs are outside this contract.

`test_memory.c` checks relocated calls/branches, INT64_MIN immediate operations,
rejected writes to published memory, page/staging limits, alignment, all reachable
allocation/platform failure indices, and zero allocator/mapping counters after
teardown. `test_jit_ownership.c` adds two-function preparation, failures after a
prior publication, exact source/fuel exit, and complete teardown. Neither is a
substitute for Linux LeakSanitizer or full JIT semantic/platform conformance.

## Findings retained from the earlier prototype

`MarimayBytecodeOperand` stores signed and unsigned members separately. Reading
a record selector from the wrong member selected field zero: Dijkstra read
`Edge.to` as `Edge.weight`. The copied VM adapter fixes record selectors; enum
helpers retain full-width selectors from the original instruction. A production
image fix must preserve both nonzero record fields and wide enum selectors.
Foreign working files in the main checkout are neither modified nor staged.

ASan/UBSan exposed signed negation of INT64_MIN in SLJIT's ARM64 immediate
selection. `sljit-arm64-minimum.patch` avoids two such negations and uses the
existing register-operand fallback. It applies only to the copied pinned
upstream source. The focused memory test exercises both comparison/subtraction
and addition with this boundary; no sanitizer suppression was added.

## Remaining production gates

1. Complete focused JIT differential coverage for every admitted operation,
   reference alias/rebinding, forced GC, finite frame/shared-fuel boundaries,
   trap spans and unsupported/code-budget fallback.
2. Adopt the dependency, memory owner and execution-tier contracts in the
   production architecture, specification and build/distribution path.
3. Remove ordinary call/activation costs while retaining logical depth, exact
   roots and shared limits; retain accepted composed-loop performance.
4. Validate supported Tier 1 hosts, Linux LSan and the unchanged complete
   performance matrix. ARM64 and Rosetta observations do not establish Linux,
   Windows or physical x86-64 performance. Go/Rust parity remains open.
