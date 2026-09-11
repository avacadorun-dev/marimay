# Catalogue and language benchmark

Generated: 2026-09-12T00:52:54+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 7.46 | 4.35 | 4.30 | 56.43 | 12.54 | 1.72× | 1.74× | — | measured |
| title-search-100k | 100,000 × 128 | 59.89 | 14.77 | 17.67 | 166.00 | 35.29 | 4.05× | 3.39× | — | measured |
| normalize-title | 10,000 × 1 | 56.38 | 8.58 | 10.91 | 129.70 | 17.21 | 6.57× | 5.17× | — | measured |
| word-frequency | 10,000 × 10 | 141.91 | 18.77 | 30.02 | 255.12 | 18.22 | 7.56× | 4.73× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 14.19 | 4.95 | 5.27 | 80.13 | 11.50 | 2.87× | 2.69× | — | measured |
| report-concat | 10,000 × 1 | 96.33 | 80.54 | 47.41 | 165.09 | 11.96 | 1.20× | 2.03× | — | measured |
| map-int | 4,096 × 32 | 7.42 | 5.08 | 5.84 | 66.55 | 13.34 | 1.46× | 1.27× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 22.38 | 7.17 | 9.95 | 84.31 | 14.88 | 3.12× | 2.25× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 30.51 | 6.30 | 10.19 | 89.91 | 14.80 | 4.84× | 2.99× | — | measured |
| set-string-churn | 4,096 × 8 | 46.03 | 9.88 | 16.89 | 108.88 | 17.02 | 4.66× | 2.73× | — | measured |
| catalog-match | 10,000 × 32 | 13.16 | 4.60 | 3.90 | 120.55 | 18.50 | 2.86× | 3.37× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.45 | 4.58 | 3.69 | 100.58 | 17.38 | 1.84× | 2.29× | — | measured |
| catalog-interface | 10,000 × 32 | 11.96 | 4.60 | 3.78 | 125.35 | 18.27 | 2.60× | 3.17× | — | measured |
| record-reference-copy | 1,024 × 128 | 6.82 | 4.31 | 3.20 | 83.64 | 13.90 | 1.58× | 2.13× | — | measured |
| record-create | 1,024 × 128 | 23.42 | 6.65 | 6.32 | 114.34 | 15.61 | 3.52× | 3.71× | — | measured |
| array-grow | 100,000 × 4 | 6.75 | 6.46 | 4.19 | 82.15 | 13.94 | 1.04× | 1.61× | — | measured |
| array-reserved | 100,000 × 4 | 6.81 | 5.10 | 4.18 | 81.53 | 12.23 | 1.34× | 1.63× | — | measured |
| bfs-int | 4,096 × 16 | 7.56 | 5.29 | 4.46 | 72.20 | 16.52 | 1.43× | 1.69× | — | measured |
| bfs-record | 4,096 × 16 | 9.39 | 6.02 | 4.90 | 78.97 | 17.01 | 1.56× | 1.92× | — | measured |
| catalog-pipeline | 10,000 × 16 | 36.07 | 7.85 | 10.10 | 204.95 | 22.93 | 4.59× | 3.57× | — | measured |
| parse-result | 10,000 × 8 | 19.25 | 5.81 | 6.01 | 74.87 | 12.77 | 3.31× | 3.21× | — | measured |
| insertion | fixed anchor | 50.88 | 29.27 | 28.52 | 208.25 | 80.87 | 1.74× | 1.78× | — | measured |
| heap-sort | fixed anchor | 32.29 | 38.88 | 22.17 | 242.34 | 91.78 | 0.83× | 1.46× | ≤ 1.3× | within target |
| stack | fixed anchor | 9.89 | 14.67 | 8.89 | 261.65 | 21.74 | 0.67× | 1.11× | ≤ 1.3× | within target |
| queue | fixed anchor | 9.85 | 14.40 | 9.22 | 254.66 | 25.44 | 0.68× | 1.07× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 46.28 | 53.56 | 29.08 | 520.17 | 108.23 | 0.86× | 1.59× | ≤ 1.3× | within target |
| dsu | fixed anchor | 29.64 | 24.22 | 21.62 | 773.31 | 48.25 | 1.22× | 1.37× | — | measured |
| bfs | fixed anchor | 65.87 | 62.84 | 23.91 | 659.70 | 68.50 | 1.05× | 2.76× | — | measured |
| dijkstra | fixed anchor | 41.39 | 40.65 | 38.82 | 199.54 | 74.76 | 1.02× | 1.07× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 32.34 | 13.93 | 16.54 | 194.85 | 27.08 | 2.32× | 1.96× | — | noisy; repeat |
| unique-set | fixed anchor | 26.87 | 15.84 | 18.45 | 132.92 | 21.17 | 1.70× | 1.46× | — | measured |
| rune-frequency | fixed anchor | 21.98 | 5.90 | 6.07 | 106.58 | 16.18 | 3.73× | 3.62× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.09 | 0.22 | 7.12–8.84 | 4% |
| title-search-10k | Go native | 0.18 | 0.32 | 3.99–7.13 | 13% |
| title-search-10k | Rust native | 0.21 | 0.43 | 3.93–7.57 | 15% |
| title-search-10k | Marimay VM (source run) | 0.85 | 1.62 | 55.28–75.12 | 5% |
| title-search-10k | Bun / TypeScript | 0.27 | 0.69 | 11.90–17.27 | 6% |
| title-search-100k | Marimay native | 2.95 | 6.89 | 55.47–68.37 | 15% |
| title-search-100k | Go native | 0.44 | 0.97 | 13.88–17.87 | 9% |
| title-search-100k | Rust native | 0.25 | 0.88 | 17.25–19.07 | 5% |
| title-search-100k | Marimay VM (source run) | 5.75 | 14.52 | 158.48–178.32 | 10% |
| title-search-100k | Bun / TypeScript | 0.69 | 1.89 | 34.34–38.56 | 6% |
| normalize-title | Marimay native | 0.58 | 1.32 | 55.47–66.73 | 3% |
| normalize-title | Go native | 0.17 | 0.31 | 8.16–19.40 | 6% |
| normalize-title | Rust native | 0.32 | 0.59 | 10.32–25.80 | 9% |
| normalize-title | Marimay VM (source run) | 1.49 | 3.17 | 127.05–138.88 | 3% |
| normalize-title | Bun / TypeScript | 0.53 | 1.07 | 16.17–19.06 | 9% |
| word-frequency | Marimay native | 1.49 | 5.10 | 140.41–156.74 | 4% |
| word-frequency | Go native | 0.46 | 0.94 | 18.07–21.62 | 7% |
| word-frequency | Rust native | 0.36 | 0.76 | 29.23–33.79 | 4% |
| word-frequency | Marimay VM (source run) | 2.14 | 7.96 | 252.52–271.88 | 3% |
| word-frequency | Bun / TypeScript | 0.38 | 0.65 | 17.52–19.12 | 6% |
| report-builder | Marimay native | 0.17 | 0.50 | 13.92–16.11 | 4% |
| report-builder | Go native | 0.22 | 0.49 | 4.72–7.11 | 13% |
| report-builder | Rust native | 0.33 | 0.50 | 4.79–5.72 | 19% |
| report-builder | Marimay VM (source run) | 0.43 | 1.16 | 78.88–84.67 | 2% |
| report-builder | Bun / TypeScript | 0.17 | 0.37 | 11.17–12.55 | 5% |
| report-concat | Marimay native | 1.36 | 2.58 | 94.30–112.93 | 4% |
| report-concat | Go native | 1.41 | 4.39 | 76.88–89.97 | 5% |
| report-concat | Rust native | 0.99 | 2.23 | 45.52–51.58 | 6% |
| report-concat | Marimay VM (source run) | 4.47 | 5.62 | 158.29–174.49 | 8% |
| report-concat | Bun / TypeScript | 0.27 | 0.57 | 11.31–13.13 | 7% |
| map-int | Marimay native | 0.05 | 0.39 | 7.08–7.80 | 5% |
| map-int | Go native | 0.07 | 0.24 | 4.80–5.78 | 5% |
| map-int | Rust native | 0.17 | 0.40 | 5.65–7.12 | 9% |
| map-int | Marimay VM (source run) | 0.66 | 2.34 | 65.31–70.48 | 4% |
| map-int | Bun / TypeScript | 0.60 | 1.15 | 12.09–14.16 | 14% |
| map-string | Marimay native | 0.41 | 1.14 | 21.81–27.37 | 5% |
| map-string | Go native | 0.14 | 0.71 | 6.89–9.25 | 10% |
| map-string | Rust native | 0.26 | 0.56 | 9.44–12.24 | 8% |
| map-string | Marimay VM (source run) | 1.04 | 2.10 | 82.27–87.90 | 4% |
| map-string | Bun / TypeScript | 0.32 | 0.66 | 13.98–15.84 | 6% |
| map-string-book | Marimay native | 0.49 | 1.16 | 29.72–33.00 | 5% |
| map-string-book | Go native | 0.26 | 0.47 | 5.91–7.08 | 12% |
| map-string-book | Rust native | 0.16 | 0.48 | 9.96–11.03 | 5% |
| map-string-book | Marimay VM (source run) | 0.57 | 2.30 | 88.67–92.36 | 3% |
| map-string-book | Bun / TypeScript | 0.44 | 0.97 | 14.20–16.72 | 9% |
| set-string-churn | Marimay native | 0.82 | 1.69 | 44.56–48.69 | 5% |
| set-string-churn | Go native | 0.17 | 0.42 | 9.35–10.42 | 5% |
| set-string-churn | Rust native | 0.24 | 0.66 | 16.48–18.02 | 4% |
| set-string-churn | Marimay VM (source run) | 1.66 | 3.80 | 106.84–112.84 | 5% |
| set-string-churn | Bun / TypeScript | 0.55 | 1.09 | 16.15–18.40 | 10% |
| catalog-match | Marimay native | 0.28 | 0.98 | 12.83–14.06 | 7% |
| catalog-match | Go native | 0.14 | 0.29 | 4.36–4.81 | 9% |
| catalog-match | Rust native | 0.17 | 0.37 | 3.58–4.88 | 13% |
| catalog-match | Marimay VM (source run) | 1.00 | 2.50 | 118.92–124.06 | 2% |
| catalog-match | Bun / TypeScript | 0.24 | 0.70 | 17.45–19.17 | 4% |
| catalog-dotcall | Marimay native | 0.18 | 0.37 | 8.24–9.05 | 6% |
| catalog-dotcall | Go native | 0.10 | 0.24 | 4.28–4.70 | 6% |
| catalog-dotcall | Rust native | 0.10 | 0.22 | 3.51–3.99 | 8% |
| catalog-dotcall | Marimay VM (source run) | 1.11 | 2.75 | 98.94–103.72 | 3% |
| catalog-dotcall | Bun / TypeScript | 0.61 | 1.24 | 16.66–18.33 | 11% |
| catalog-interface | Marimay native | 0.13 | 0.22 | 11.58–12.29 | 3% |
| catalog-interface | Go native | 0.27 | 0.47 | 4.34–5.06 | 17% |
| catalog-interface | Rust native | 0.31 | 0.62 | 3.47–4.80 | 25% |
| catalog-interface | Marimay VM (source run) | 2.26 | 4.53 | 122.70–129.41 | 5% |
| catalog-interface | Bun / TypeScript | 0.68 | 1.54 | 17.30–21.22 | 11% |
| record-reference-copy | Marimay native | 0.31 | 0.68 | 6.49–7.49 | 14% |
| record-reference-copy | Go native | 0.15 | 0.32 | 4.11–4.62 | 10% |
| record-reference-copy | Rust native | 0.05 | 0.15 | 3.12–3.41 | 5% |
| record-reference-copy | Marimay VM (source run) | 0.70 | 1.55 | 82.35–85.79 | 3% |
| record-reference-copy | Bun / TypeScript | 0.45 | 0.92 | 13.32–14.65 | 10% |
| record-create | Marimay native | 0.28 | 0.72 | 22.65–24.71 | 4% |
| record-create | Go native | 0.20 | 0.41 | 6.29–6.97 | 9% |
| record-create | Rust native | 0.30 | 0.73 | 5.94–7.03 | 14% |
| record-create | Marimay VM (source run) | 1.59 | 3.33 | 111.70–116.88 | 4% |
| record-create | Bun / TypeScript | 0.29 | 0.66 | 14.77–15.92 | 6% |
| array-grow | Marimay native | 0.18 | 0.46 | 6.44–7.22 | 8% |
| array-grow | Go native | 0.12 | 0.22 | 6.03–6.67 | 5% |
| array-grow | Rust native | 0.13 | 0.27 | 3.92–4.93 | 9% |
| array-grow | Marimay VM (source run) | 0.79 | 1.64 | 80.31–87.44 | 3% |
| array-grow | Bun / TypeScript | 0.40 | 0.81 | 12.93–15.11 | 9% |
| array-reserved | Marimay native | 0.21 | 0.39 | 6.46–7.35 | 9% |
| array-reserved | Go native | 0.12 | 0.28 | 4.82–5.47 | 7% |
| array-reserved | Rust native | 0.13 | 0.27 | 3.92–4.70 | 9% |
| array-reserved | Marimay VM (source run) | 1.28 | 2.18 | 79.97–86.18 | 5% |
| array-reserved | Bun / TypeScript | 0.27 | 0.84 | 11.96–13.28 | 7% |
| bfs-int | Marimay native | 0.24 | 0.52 | 7.05–8.15 | 10% |
| bfs-int | Go native | 0.15 | 0.31 | 4.94–6.48 | 9% |
| bfs-int | Rust native | 0.14 | 0.33 | 4.23–5.42 | 10% |
| bfs-int | Marimay VM (source run) | 0.85 | 2.06 | 70.78–74.13 | 4% |
| bfs-int | Bun / TypeScript | 0.55 | 1.33 | 15.39–17.45 | 10% |
| bfs-record | Marimay native | 0.26 | 0.68 | 8.85–12.25 | 8% |
| bfs-record | Go native | 0.29 | 0.50 | 5.36–6.59 | 14% |
| bfs-record | Rust native | 0.31 | 0.66 | 4.28–5.98 | 19% |
| bfs-record | Marimay VM (source run) | 1.35 | 3.71 | 77.38–83.49 | 5% |
| bfs-record | Bun / TypeScript | 0.50 | 1.17 | 15.78–18.11 | 9% |
| catalog-pipeline | Marimay native | 0.54 | 1.39 | 35.21–44.62 | 5% |
| catalog-pipeline | Go native | 0.32 | 0.78 | 7.43–11.35 | 12% |
| catalog-pipeline | Rust native | 0.22 | 0.34 | 9.67–12.19 | 7% |
| catalog-pipeline | Marimay VM (source run) | 2.62 | 7.06 | 201.80–214.01 | 4% |
| catalog-pipeline | Bun / TypeScript | 0.67 | 1.26 | 21.64–24.65 | 9% |
| parse-result | Marimay native | 0.25 | 0.52 | 18.57–19.68 | 4% |
| parse-result | Go native | 0.12 | 0.28 | 5.53–6.38 | 6% |
| parse-result | Rust native | 0.30 | 0.74 | 5.62–6.54 | 15% |
| parse-result | Marimay VM (source run) | 1.38 | 3.56 | 73.35–78.35 | 6% |
| parse-result | Bun / TypeScript | 0.43 | 0.77 | 12.00–14.31 | 10% |
| insertion | Marimay native | 2.81 | 3.73 | 46.70–56.53 | 17% |
| insertion | Go native | 0.58 | 1.31 | 28.33–30.13 | 6% |
| insertion | Rust native | 0.46 | 1.02 | 27.83–29.52 | 5% |
| insertion | Marimay VM (source run) | 1.09 | 4.91 | 205.47–240.62 | 2% |
| insertion | Bun / TypeScript | 1.39 | 1.97 | 78.85–85.63 | 5% |
| heap-sort | Marimay native | 0.40 | 0.93 | 31.58–35.94 | 4% |
| heap-sort | Go native | 0.51 | 1.06 | 38.21–40.98 | 4% |
| heap-sort | Rust native | 0.31 | 0.76 | 21.42–22.75 | 4% |
| heap-sort | Marimay VM (source run) | 1.91 | 5.17 | 238.19–247.64 | 2% |
| heap-sort | Bun / TypeScript | 0.84 | 1.82 | 89.74–92.98 | 3% |
| stack | Marimay native | 0.17 | 0.49 | 9.44–10.73 | 5% |
| stack | Go native | 0.36 | 0.74 | 14.06–15.32 | 7% |
| stack | Rust native | 0.21 | 0.63 | 8.55–9.51 | 7% |
| stack | Marimay VM (source run) | 3.81 | 6.61 | 256.85–273.69 | 4% |
| stack | Bun / TypeScript | 0.33 | 0.88 | 20.66–23.63 | 5% |
| queue | Marimay native | 0.29 | 0.59 | 9.54–10.38 | 9% |
| queue | Go native | 0.36 | 0.64 | 13.96–15.03 | 7% |
| queue | Rust native | 0.27 | 0.66 | 8.53–9.50 | 9% |
| queue | Marimay VM (source run) | 1.86 | 12.24 | 250.50–280.10 | 5% |
| queue | Bun / TypeScript | 0.50 | 1.14 | 24.56–27.21 | 6% |
| min-heap | Marimay native | 0.88 | 2.11 | 44.73–51.53 | 6% |
| min-heap | Go native | 0.78 | 3.63 | 52.35–58.41 | 7% |
| min-heap | Rust native | 0.54 | 0.98 | 28.38–33.04 | 6% |
| min-heap | Marimay VM (source run) | 3.59 | 8.56 | 507.64–532.07 | 2% |
| min-heap | Bun / TypeScript | 1.40 | 3.40 | 104.49–116.13 | 4% |
| dsu | Marimay native | 0.40 | 1.11 | 29.01–31.14 | 4% |
| dsu | Go native | 0.17 | 0.40 | 23.65–25.71 | 2% |
| dsu | Rust native | 0.49 | 0.88 | 21.04–23.20 | 7% |
| dsu | Marimay VM (source run) | 7.68 | 16.24 | 761.60–815.70 | 3% |
| dsu | Bun / TypeScript | 0.88 | 1.94 | 47.00–53.93 | 5% |
| bfs | Marimay native | 0.81 | 3.64 | 64.43–121.15 | 6% |
| bfs | Go native | 1.58 | 3.33 | 60.31–65.75 | 8% |
| bfs | Rust native | 0.32 | 0.77 | 23.08–25.06 | 4% |
| bfs | Marimay VM (source run) | 2.95 | 8.39 | 647.85–673.71 | 2% |
| bfs | Bun / TypeScript | 0.79 | 1.73 | 66.43–79.90 | 3% |
| dijkstra | Marimay native | 0.47 | 0.86 | 40.68–49.02 | 3% |
| dijkstra | Go native | 0.34 | 1.00 | 40.13–44.38 | 2% |
| dijkstra | Rust native | 0.22 | 0.62 | 38.35–41.91 | 2% |
| dijkstra | Marimay VM (source run) | 0.72 | 1.48 | 198.60–236.32 | 2% |
| dijkstra | Bun / TypeScript | 0.70 | 1.53 | 73.73–89.05 | 3% |
| frequency-map | Marimay native | 0.20 | 1.07 | 31.85–43.40 | 3% |
| frequency-map | Go native | 0.36 | 0.86 | 13.53–23.18 | 8% |
| frequency-map | Rust native | 3.44 | 9.98 | 13.09–25.79 | 62% |
| frequency-map | Marimay VM (source run) | 0.93 | 3.98 | 193.92–239.14 | 2% |
| frequency-map | Bun / TypeScript | 0.46 | 1.24 | 26.47–36.01 | 5% |
| unique-set | Marimay native | 0.23 | 0.48 | 26.41–30.18 | 3% |
| unique-set | Go native | 0.24 | 0.72 | 15.53–17.74 | 5% |
| unique-set | Rust native | 0.35 | 1.24 | 18.06–20.37 | 7% |
| unique-set | Marimay VM (source run) | 2.36 | 4.71 | 130.20–149.38 | 5% |
| unique-set | Bun / TypeScript | 0.30 | 0.77 | 20.11–27.21 | 4% |
| rune-frequency | Marimay native | 0.17 | 0.44 | 21.72–23.00 | 2% |
| rune-frequency | Go native | 0.44 | 1.12 | 5.30–7.17 | 22% |
| rune-frequency | Rust native | 0.19 | 0.79 | 5.65–6.92 | 13% |
| rune-frequency | Marimay VM (source run) | 0.91 | 1.99 | 104.91–120.50 | 3% |
| rune-frequency | Bun / TypeScript | 0.25 | 0.59 | 15.75–21.06 | 5% |

## Coverage and representations

- **title-search-10k**: Linear exact UTF-8 title search; 75% hits and 25% misses.
- **title-search-100k**: Same queries over 100,000 unique runtime-built titles.
- **normalize-title**: ASCII trim, owned scalar pieces, explicit ASCII/Russian folding table, builder, equality.
- **word-frequency**: Four-token Russian/ASCII lines; owned token split and map counts; all keys checked.
- **report-builder**: Build 10k formatted rows using a growing string builder; checksum every UTF-8 byte.
- **report-concat**: Same rows with a fresh immutable concatenation each step (quadratic copy volume).
- **map-int**: 4096 integer keys, get then put; deterministic permutation and full weighted histogram.
- **map-string**: Same map trace with dynamic Russian/ASCII title keys; fresh equal keys for final lookups.
- **map-string-book**: Dynamic string keys to Book references; construct once, lookup and read title/state/serial.
- **set-string-churn**: Sliding live window; remove old string, check absence, insert fresh key; verify final membership.
- **catalog-match**: BookState enum + exhaustive match; Go tagged struct + switch, Rust enum + match.
- **catalog-dotcall**: Concrete Book label call; no per-call result string allocation.
- **catalog-interface**: Materialized interface array; method-table call and UTF-8 label length.
- **record-reference-copy**: Retain aliases in a growing array; compare every identity and read fields.
- **record-create**: Retain fresh Book objects per batch; same fields, distinct identity; titles/states shared.
- **array-grow**: Append integers from empty capacity, then checksum all values.
- **array-reserved**: Same append trace with one initial reserve.
- **bfs-int**: Contiguous integer IDs, growing integer queue, distance array, degree 16.
- **bfs-record**: Same edges/distances; Node records and reference queue; edges remain integer IDs.
- **catalog-pipeline**: Combined title search, title frequency map, and BookState match; 25% missing queries.
- **parse-result**: Format/parse decimal tokens, 25% invalid suffixes; propagate Result/error then match.
- **insertion**: 50 × 2,500 generated integers
- **heap-sort**: 5 × 100,000 generated integers
- **stack**: 1,000,000 push + pop
- **queue**: 1,000,000 enqueue + dequeue
- **min-heap**: 500,000 insert + remove-min
- **dsu**: 100,000 nodes; 5,000,000 unions
- **bfs**: 300 × 20,000 nodes, degree 4
- **dijkstra**: 80 × 800 nodes, O(V²)
- **frequency-map**: 1,000,000 integers; 4,096-key histogram
- **unique-set**: 1,000,000 integers; distinct-value count
- **rune-frequency**: 450,000 Unicode scalars; 12-key histogram

## Interpretation limits

- No general Unicode lower/casefold API: normalize-title measures a shared corpus folding helper only.
- No I/O, concurrency, JSON or HTTP claims.
- Allocation workloads prescribe escaping objects; allocation counts, GC pauses and peak RSS are not measured.
- Map/set implementations retain host ordering, hashing, growth and GC policies. Cross-language equality is observable results, not identical runtime layouts.
- Go uses *Book and Rust Rc<Book> to preserve Marimay reference identity; Rust Rc costs do not generalize to all Rust representations.
- BFS has fixed degree 16 and contiguous IDs; it is not an O(V²)-edge dense graph and record edges are IDs, not object pointers.
- Bun uses JavaScript numbers for bounded exact integers, UTF-16 strings with UTF-8 byte checksums, Map/Set and reference objects; storage and copying are runtime-controlled.
- Bun report-builder uses an array of chunks plus join; concatenation may use ropes. Split and scalar strings are independently retainable values, without a forced physical copy in JavaScript.
- TypeScript interfaces are erased; catalog-interface measures a materialized structural-interface array and an ordinary method call, not a guaranteed vtable dispatch.
- JavaScript has no reserve-only Array API: array-reserved uses new Array(n) plus indexed writes; array-grow uses push. These are equivalent values, not identical allocation/append traces.
- The per-workload Go targets are exploratory thresholds, not reviewed release budgets. No average score or general ‘Go parity’ verdict is computed.
- Short cases can be dominated by process startup and validation; repeat with larger documented inputs before attributing a difference to dispatch, GC or a particular primitive.
- Interface results include array construction/boxing and compiler devirtualization opportunities; they are not isolated virtual-call latency.

## Toolchains

- python: `3.9.6`
- marimay: `Marimay compiler 0.1.0-alpha.1`
- go: `go version go1.26.4 darwin/arm64`
- rust: `rustc 1.96.0 (ac68faa20 2026-05-25)`
- bun: `1.4.2`

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-string-views-v4/comprehensive-current.md`
