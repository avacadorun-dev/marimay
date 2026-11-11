# One separator search: the split scanner was the cost

Local macOS arm64 evidence, 2026-11-11; candidate inside the #26/#3 timebox.
[Raw matrices, orders, samples and executable hashes](cp42-split-separator-search-2026-11-11.json).

This closes the diagnostic that blocked mechanism selection in the
[timebox start record](cp42-perf-timebox-2026-11-11.md), corrects one number in
that record, and delivers a paired improvement for `word-frequency` (#3). It is
not a corpus acceptance gate: Tier 1, the VM and Bun columns and the other three
collection rows remain unchanged and open.

## Correction to the start record

The start record reported 173.3 ns per produced split piece from a one-piece and
a four-piece probe. That slope was contaminated. The probes compared a 10-byte
one-piece line with a 21-byte four-piece line, so every extra piece also bought
11 extra scanned bytes, and the slope charged the scan to the piece.

An equal-length control isolates the two effects: a seven-byte one-piece line and
a seven-byte four-piece line (`a a a a`) differ only in piece count.

| Probe, 100,000 calls | Median ms | ns per call |
|---|---:|---:|
| one piece, 7 bytes | 44.779 | 447.8 |
| four pieces, 7 bytes | 67.213 | 672.1 |

| Derived quantity | Corrected value |
|---|---:|
| marginal cost per produced piece | **74.8 ns** |
| fixed cost per call at the same line length | 373.0 ns |
| cost per additional scanned line byte | **19.6 ns** |

Moving eight bytes into the same four-piece line costs 15.7 ms per 100,000
calls, so a scanned byte costs ~19.6 ns, split across the counting pass and the
building pass — about 9.8 ns for each `memcmp` call. For the 21-byte,
four-piece line that the corpus actually splits, 412 ns of the 942 ns per call
was scanning, 299 ns was publishing four pieces and the remaining ~230 ns was
the per-call scaffolding. The scanner, not the allocator, was the largest single
cost, which is why every earlier candidate that removed allocations moved the row
by less than 5%.

## Mechanism

Four hand-written loops scanned text one byte at a time and called `memcmp` for
every position: the counting pass in `bootstrap/src/base/text.c`, the native
helper emitted by `bootstrap/src/native/c_emitter_text.c`, the VM in
`bootstrap/src/bytecode/vm_text.c` and the MIR interpreter in
`bootstrap/src/mir/execution_text.c`.

One owner now performs the search, `mmay_text_find_separator` in
`bootstrap/src/base/text.c`. A one-byte separator uses `memchr`; a longer one
compares the first byte directly and calls `memcmp` only on a candidate. The
counting pass, both engines and the emitted native helper call it, and
`mmay_text_contains` reuses it instead of keeping a fifth copy. The helper is
total: invalid input, an empty separator, a separator longer than the text or an
exhausted range all report the text length, and a match can never start at the
text length, so the two cases stay distinguishable without a second status
channel. No caller gained a failure path, no allocation changed, and D055
accounting, failure atomicity and piece boundaries are untouched.

Beyond the speed, generated native code lost the scan loop: the emitter now
writes one call instead of a loop, so the three engines cannot drift apart on
boundary behaviour.

## Correctness

| Check | Result |
|---|---|
| `mmay_cp33_unit` (new boundary cases) | passed |
| `mmay_cp33_text_bytes` (`test --engine both`) | passed |
| `mmay_cp33_process_text_bytes`, `mmay_format_cp33-text-bytes` | passed |
| focused `text`/`string`/`bytecode`/`mir` tests | 9/9 passed |
| ASan+UBSan build of the same tests | passed |
| full Debug suite, serial | 99 of 100 passed |

The new unit cases pin every boundary the four loops had: leading, trailing and
consecutive separators, a multi-byte separator, a match at the last possible
position, a one-byte separator that must not match inside a multi-byte one, and
every rejected input. `mmay_macos_package` fails on `pkgbuild` in this
environment; the same failure reproduces with the change stashed, and the test
compiles no Marimay code.

## Measurement

Two levels, in this order. Both use the unchanged corpus and the release
compiler; neither touches Tier 1 budgets.

**Screening, paired probes.** The same probe sources, built once by the pre-patch
compiler and once by the patched one, timed alternately in fresh processes, five
warmups and 21 samples:

| Probe | Pre-patch ms | Patched ms | Delta |
|---|---:|---:|---:|
| `split` (100,000 calls, 4 pieces) | 94.845 | 61.493 | **−35.2%** |
| `full` (corpus `wordFrequency` body) | 132.694 | 98.420 | **−25.8%** |
| `map` (400,000 string get+put, no split) | 39.232 | 38.939 | −0.7% |

The map component, which the change does not touch, stays flat, so the effect is
where the mechanism says it is.

**Corpus, 32 workloads, native column.** Three warmups and eleven samples per
cell, Marimay native, Go and Rust in the same run, four matrices: pre-patch,
patched, patched again as an A/A control, and pre-patch again to control for
host drift.

| Matrix | `word-frequency` median ms |
|---|---:|
| pre-patch, first | 130.70 |
| patched, first | 95.47 |
| patched, A/A control | 94.87 |
| pre-patch, repeat | 132.97 |

| Pair | Median delta | P90 delta |
|---|---:|---:|
| pre-patch → patched | **−27.0%** | −27.3% |
| pre-patch repeat → patched | **−28.2%** | −38.4% |
| patched → A/A control (noise floor) | −0.2% (range −3.2..+4.6%) | — |
| pre-patch → pre-patch repeat (drift) | +4.8% (range −1.0..+9.1%) | — |

`word-frequency` is the only corpus workload that calls the changed primitive;
`values.contains` in the collection workloads is array containment, not text
search. Everything else therefore moves with the host, and the controls say how
much: this host drifts up to 4.8% between two runs of one binary, and up to 9.1%
for a single row. Under that envelope no row regresses — the largest positive in
the pre-patch → patched pair is `queue` +3.2%, and `queue` moves −2.8% between
two runs of the same patched binary. Several other rows show −5% to −15% in the
same pair and also appear in the drift set (`report-concat` −10.6% against +5.1%
drift, `set-string-churn` −6.5%, `bfs` −3.9%); they are not attributed, because
they cannot call the changed function.

Against its own contemporaneous Go column the row went from 6.68× to **5.06×**
(95.47 ms against 18.85 ms). The recorded Go target for this row is 2.0×, so the
row improves and stays open.

## What this does not prove

No Tier 1 baseline, no VM or Bun column, no parity claim, no CP42 or CLI0
closure. The three #26 rows measured −0.7%, +0.4% and −0.5%, which is the noise
floor, not a result: they were not touched and they still need their own exit.
Host isolation remains unverified, samples are local-only, and the drift
controls above are the only reason the `word-frequency` number can be trusted.

## Where the time goes now

For the same 21-byte four-piece line, 606 ns per call remain against the 942 ns
before the change:

| Component | Before | After |
|---|---:|---:|
| scan | 412 ns | ~40 ns |
| four pieces published | 299 ns | 299 ns |
| per-call scaffolding (cell array malloc/free, strided root frame, array publication) | ~230 ns | ~265 ns |

The ranking changed. Per-call scaffolding and per-piece publication now carry
93% of the remaining split cost, and the string-key map carries 39% of the row.
The next candidate is the scaffolding — one `malloc`/`free` pair, one root frame
and one array publication per call, none of which the split result needs if the
pieces are written into an array that is already rooted. The map is second and
separate.
