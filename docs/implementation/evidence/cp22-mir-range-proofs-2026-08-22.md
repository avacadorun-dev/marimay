# Checkpoint 22: verified MIR range-proof capabilities

Date: 2026-08-22
Scope: internal MIR/bytecode/native optimization foundation; no source syntax
change.

## Delivered contract

- `ARRAY_REQUIRE_INDEX` remains the sole observable D045/G4002 bounds barrier.
- `ARRAY_GET_IN_BOUNDS` and `ARRAY_SET_IN_BOUNDS` consume its matching
  verifier capability, so a compound indexed assignment does not re-check the
  same index after the barrier. A MIR-only assumption additionally removes the
  barrier for literal lengths and exact `index < array.length()` true edges
  whose separate `index >= 0` must-fact reaches the CFG fixed point;
  it is serialized as a checked bytecode barrier and introduces no
  source-visible rule or cached array cell.
- The MIR and bytecode verifiers independently reject a missing or forged
  carrier. Calls, interface calls, stores, and length/capacity mutations clear
  all range capabilities.
- The optimizer prepares every allocation before rewriting and rolls back on a
  post-rewrite verification failure. Resource exhaustion returns checked MIR;
  verifier exhaustion rejects the artifact.
- VM, MIR oracle, and generated C each route in-bounds operations to the
  explicit runtime trust-boundary helpers. No raw cell pointer is cached.

## Focused coverage

`test_mir_range.c` covers transactional rewrite of the get/store pair,
checked-versus-rewritten MIR and VM execution, literal and source-level nested
affine-loop assumptions, a forged negative true edge, hostile-bytecode proof
rejection, generated-C execution at both `-O0` and `-O2`, direct and interface
call invalidation, every array mutation and block-boundary invalidation, and
deterministic pass/verifier/codec allocation sweeps with zero live allocator
counters after teardown. Existing checker, imported-array, opcode-registry,
VM, native, and CLI suites exercise the canonical instructions through their
adjacent owners.

## Local validation

The following completed locally on macOS arm64:

```text
cmake --build --preset debug --parallel 4
ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
cmake --build --preset release --parallel 4
ctest --preset release -R '^ruda_c_unit$' --output-on-failure
cmake --build --preset asan-ubsan --parallel 4
ctest --preset asan-ubsan -R '^ruda_c_unit$' --output-on-failure
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-generated
./tools/check-doc-links
./tools/check-spec-terms
```

The generated native code contains one checked `ARRAY_REQUIRE_INDEX` barrier
per source indexing operation unless a literal or true-edge range fact proves
it unnecessary, and replaces the following matching get/store with the
in-bounds helper. A generated array acceptance program has seven barriers and
seven in-bounds helpers; the `proven-bounds` generated program has five dynamic
barriers and seven in-bounds loads.

## Pushed CI and Linux performance evidence

Ruda commit `1d728451612f5ad4d6cfb78a5785b2e57605af26` is published on
`origin/main`. Gitea Actions [run #86](http://ci.example:3000/OS/Ruda/actions/runs/86)
completed 10/10 jobs: documentation/architecture, Linux x86_64 GCC and Clang
Debug/Release, ASan+UBSan+LSan, TSan, and bytecode, formatter, and frontend
fuzzing.

The sibling benchmark [run #87](http://ci.example:3000/OS/ruda-algotithms/actions/runs/87)
used that exact release compiler in paired 11-sample/3-warmup probe and
Ruda/Go/Rust algorithm reports. Attempt 1 began at load average 14.78 on the
four-CPU runner and transiently exceeded five old probe budgets. D056 therefore
required one complete cool-down retry with unchanged compiler, runtime,
protocol, manifests, source tree, resource budget, and sample schedule. The
[retry job](http://ci.example:3000/OS/ruda-algotithms/actions/runs/87/jobs/695)
passed every previous reviewed budget; none of the five exceedances reproduced.
Attempt 1 remains preserved by sibling commit `2bf4f73395b84eac123ccf14d45733f3d5b6ab75`.
The reviewed retry and both merge parents are published on sibling `origin/main`
at `a05a1ff`.

| Probe | CP21 median ms | CP22 retry median ms | Change |
|---|---:|---:|---:|
| `scalar-get` | 167.897 | 170.798 | +1.73% |
| `scalar-set` | 229.951 | 167.280 | -27.25% |
| `proven-bounds` | 424.440 | 345.294 | -18.65% |
| `unproven-bounds` | 392.307 | 403.390 | +2.83% |
| `growth` | 140.729 | 142.287 | +1.11% |
| `reserved-growth` | 140.747 | 142.273 | +1.08% |
| `density` | 694.539 | 577.399 | -16.87% |
| `nested-traversal` | 383.643 | 313.333 | -18.33% |
| `checked-loop` | 475.486 | 478.304 | +0.59% |

The faithful algorithm corpus also remained within every old budget:
insertion sort improved 13.72%, heap sort 7.81%, stack 1.31%, min-heap 12.43%,
and BFS 1.37%; queue and DSU moved by less than 1.5%, while Dijkstra was 6.80%
slower but remained below its reviewed 10% limit. The maximum retained Ruda
noise fraction was 4.93%; the maximum in the complete Ruda/Go/Rust matrix was
17.03%, below the D056 30% rejection threshold. All new Ruda budgets retain
the 10% floor rather than widening a threshold for this optimization.

The retry probe JSON SHA-256 is
`819b702bc3f3c5ca382d9f957f7ed812c7b99c67afe53aaae2970c10ed704caf`;
the paired algorithm JSON SHA-256 is
`0bccbbfdb24ca6d5fd5f405183b021deea667d0952505f094ebb6a069cb4f7dd`;
the reviewed budget SHA-256 is
`0fb2f24863cf7a4c4f5483ed7c245db9d7ec594965a3b624aea84939502d4df9`.
The supplemental Apple M2 report remains useful corroboration:
`proven-bounds` improved from 264.163 ms to 207.477 ms, or 21.46%.

Checkpoint 22 is therefore verified under D057's Linux checkpoint gate. The
separate public-alpha registry still requires Linux arm64, macOS x86_64, and
Windows x86_64 baselines and later post-optimization revalidation; that
deferred release gate is not claimed here and does not block checkpoint 23.
