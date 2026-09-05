# Empty text ranges: native undefined behavior

This corrects the existing D076 text contract; it introduces no new source
syntax, conversion, ownership rule or error type.

## Failure and correction

The Linux x86_64 ASan/UBSan/LSan job for main `01aec63`
([run 176, job 1431](http://ci.example:3000/Marimay/marimay/actions/runs/176/jobs/1431))
reported `applying zero offset to null pointer` in the native `trimAscii`
function while executing `mmay_cli_frequency`. VM returned the expected
`{"tea":2,"":1}` while native exited 1 under UBSan. Inspection of generated C
confirmed an unconditional `value.data + start`, with NULL-backed empty
managed string storage and `start == 0`.

The same calculation occurred in prefix dropping (used by substring) and
split-result construction in native, VM and MIR-oracle text owners. All nine
range-construction sites now use the allocation-free, shared
`mmay_text_range_start` helper. Its caller must already have validated the
range; zero offset returns the original pointer without C pointer arithmetic.
It neither invents storage nor changes counted-string or boundary semantics.

Other pointer arithmetic in these focused text owners was inspected: UTF-8
decoding and separator comparison require remaining nonempty bytes, while
join copies are guarded by a nonzero copy length.

## Regression coverage

- The CP33 MIR fixture now also transforms a managed empty value through
  trim, prefix take/drop, split/join, rune/grapheme copies and UTF-8 conversion.
  Exact output and array lengths are checked through MIR, bytecode round-trip,
  VM and generated C.
- Emission assertions require the safe helper at all three native range
  sites. Base tests include NULL/zero and nonempty zero/interior/end offsets.
- MIR execution, bytecode execution/lowering and native emission replay every
  allocation index measured on an unfailing baseline, rather than stopping at
  the first successful fault-injected execution. Every full teardown checks
  both live allocation and byte counters.
- `test_native_text_failures.c` compiles the real generated program at `-O0`
  and `-O2`, then replays each allocation index with collection before
  allocation and verifies full zero-counter teardown for empty and nonempty
  text.
- Public tests cover repeated trimming, all six ASCII whitespace bytes,
  substring of empty text, an empty suffix, leading/trailing split parts,
  all-empty join and an empty UTF-8 round trip. The existing seven-test
  `cp33-text-bytes` module and its formatter check are now explicitly
  registered with CTest; previously the checkpoint documented manual commands
  but CTest registered only the separate binary-I/O process fixture.

## Verification scope

Local platform: macOS arm64, AppleClang 21.0.0. The new public cases and empty
MIR fixture also passed with the old local sanitizer build: this host did not
diagnose the Linux-confirmed NULL-plus-zero error. Local sanitizer success is
therefore not presented as proof of the Linux fix.

After correction, the final all-index CP33 suite passes with ASan/UBSan and
`UBSAN_OPTIONS=halt_on_error=1` in 28.45 seconds. The public module passes all
seven tests with that compiler and `--engine both`.

The focused integration command below passes 4/4 groups in Release (70.93
seconds) and ASan/UBSan with `UBSAN_OPTIONS=halt_on_error=1` (222.22 seconds),
including the frequency workload that exposed the Linux error:

```sh
ctest --test-dir build/release -R '^(mmay_cp33_unit|mmay_cp33_process_text_bytes|mmay_cli_frequency|mmay_cli_notes_model)$' --output-on-failure --parallel 3
env UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/asan-ubsan -R '^(mmay_cp33_unit|mmay_cp33_process_text_bytes|mmay_cli_frequency|mmay_cli_notes_model)$' --output-on-failure --parallel 3
```

The final baseline-sized allocation replay was added after these integration
runs began; its final sanitizer result is the separate 28.45-second CP33 run
above. This distinction prevents attributing the newer sweep to an earlier
binary.

The full local Release suite, including both this correction and D084, passes
60/60 groups in 758.33 seconds:

```sh
ctest --test-dir build/release -C Release --output-on-failure --parallel 4
```

Its CLI notes process suite passes in 239.38 seconds, but that does not resolve
the historical Linux timeout. The two new public-text CTest registrations were
added while this full run was already active and are verified separately. They
pass 2/2 under ASan/UBSan in 122.37 seconds and Release in 24.05 seconds:

```sh
ctest --test-dir build/release -R '^(mmay_cp33_text_bytes|mmay_format_cp33-text-bytes)$' --output-on-failure --parallel 2
env UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/asan-ubsan -R '^(mmay_cp33_text_bytes|mmay_format_cp33-text-bytes)$' --output-on-failure --parallel 2
```

Architecture, C memory policy, generated artifacts, roadmap and whitespace
checks pass. No checkpoint or alpha acceptance state is advanced.

The referenced historical Linux job finished its test step with 51/53 groups
passing: frequency failed as described, and CLI notes timed out at 1500.11
seconds. The timeout needs separate investigation; neither this correction nor
local results close that failure or the remaining Tier 1 acceptance gates.
