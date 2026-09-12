# Catalogue and language benchmark

Generated: 2026-09-12T14:22:41+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 6.62 | 4.22 | 4.36 | 56.19 | 12.95 | 1.57× | 1.52× | — | measured |
| title-search-100k | 100,000 × 128 | 46.37 | 15.24 | 18.10 | 148.97 | 36.72 | 3.04× | 2.56× | — | measured |
| normalize-title | 10,000 × 1 | 56.81 | 8.63 | 10.87 | 128.17 | 17.48 | 6.58× | 5.22× | — | measured |
| word-frequency | 10,000 × 10 | 141.44 | 18.84 | 30.02 | 254.48 | 18.51 | 7.51× | 4.71× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 12.02 | 5.20 | 5.26 | 78.43 | 11.93 | 2.31× | 2.28× | — | measured |
| report-concat | 10,000 × 1 | 64.70 | 80.78 | 47.39 | 130.19 | 12.03 | 0.80× | 1.37× | — | measured |
| map-int | 4,096 × 32 | 7.69 | 5.25 | 6.18 | 67.78 | 13.83 | 1.47× | 1.25× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 21.18 | 7.43 | 9.95 | 83.86 | 14.99 | 2.85× | 2.13× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 25.91 | 6.64 | 10.70 | 86.65 | 15.20 | 3.90× | 2.42× | — | measured |
| set-string-churn | 4,096 × 8 | 35.22 | 10.22 | 17.32 | 98.10 | 17.92 | 3.45× | 2.03× | — | measured |
| catalog-match | 10,000 × 32 | 13.75 | 5.04 | 4.20 | 123.50 | 19.38 | 2.73× | 3.27× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.92 | 4.73 | 3.87 | 102.92 | 18.45 | 1.89× | 2.30× | — | measured |
| catalog-interface | 10,000 × 32 | 12.53 | 5.04 | 3.91 | 125.87 | 18.57 | 2.49× | 3.20× | — | measured |
| record-reference-copy | 1,024 × 128 | 7.00 | 4.64 | 3.40 | 84.76 | 14.09 | 1.51× | 2.06× | — | measured |
| record-create | 1,024 × 128 | 24.02 | 7.00 | 6.57 | 115.19 | 15.84 | 3.43× | 3.65× | — | measured |
| array-grow | 100,000 × 4 | 7.06 | 6.62 | 4.52 | 83.46 | 13.94 | 1.07× | 1.56× | — | measured |
| array-reserved | 100,000 × 4 | 7.10 | 5.18 | 4.19 | 82.79 | 13.09 | 1.37× | 1.69× | — | measured |
| bfs-int | 4,096 × 16 | 7.65 | 5.40 | 4.66 | 72.66 | 16.70 | 1.42× | 1.64× | — | measured |
| bfs-record | 4,096 × 16 | 9.39 | 5.89 | 4.85 | 80.46 | 17.01 | 1.59× | 1.93× | — | measured |
| catalog-pipeline | 10,000 × 16 | 36.08 | 8.07 | 10.44 | 206.24 | 23.00 | 4.47× | 3.46× | — | measured |
| parse-result | 10,000 × 8 | 18.35 | 5.89 | 6.19 | 72.90 | 13.42 | 3.11× | 2.96× | — | measured |
| insertion | fixed anchor | 52.16 | 29.31 | 28.83 | 211.63 | 81.76 | 1.78× | 1.81× | — | measured |
| heap-sort | fixed anchor | 32.89 | 39.39 | 22.49 | 245.14 | 93.46 | 0.84× | 1.46× | ≤ 1.3× | within target |
| stack | fixed anchor | 10.13 | 14.90 | 9.29 | 264.85 | 22.63 | 0.68× | 1.09× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.17 | 14.94 | 9.19 | 259.70 | 26.12 | 0.68× | 1.11× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 46.38 | 53.63 | 30.08 | 526.91 | 110.29 | 0.86× | 1.54× | ≤ 1.3× | within target |
| dsu | fixed anchor | 29.89 | 24.52 | 21.78 | 779.74 | 47.73 | 1.22× | 1.37× | — | measured |
| bfs | fixed anchor | 66.35 | 63.39 | 24.02 | 661.60 | 68.91 | 1.05× | 2.76× | — | measured |
| dijkstra | fixed anchor | 41.87 | 41.13 | 39.33 | 201.31 | 75.89 | 1.02× | 1.06× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 32.55 | 13.98 | 23.99 | 201.17 | 27.67 | 2.33× | 1.36× | — | measured |
| unique-set | fixed anchor | 27.43 | 16.50 | 19.58 | 132.99 | 21.75 | 1.66× | 1.40× | — | measured |
| rune-frequency | fixed anchor | 22.36 | 6.09 | 6.23 | 109.48 | 17.23 | 3.67× | 3.59× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.18 | 0.68 | 6.21–17.48 | 10% |
| title-search-10k | Go native | 0.20 | 0.36 | 3.97–9.83 | 14% |
| title-search-10k | Rust native | 0.11 | 0.37 | 4.01–4.97 | 9% |
| title-search-10k | Marimay VM (source run) | 0.78 | 1.65 | 55.01–58.02 | 4% |
| title-search-10k | Bun / TypeScript | 0.41 | 0.83 | 12.33–14.25 | 9% |
| title-search-100k | Marimay native | 1.43 | 4.40 | 43.64–50.73 | 9% |
| title-search-100k | Go native | 0.38 | 0.48 | 14.65–15.67 | 8% |
| title-search-100k | Rust native | 0.25 | 0.91 | 17.82–19.16 | 5% |
| title-search-100k | Marimay VM (source run) | 3.48 | 6.98 | 141.41–156.07 | 7% |
| title-search-100k | Bun / TypeScript | 0.77 | 1.59 | 34.66–38.72 | 6% |
| normalize-title | Marimay native | 0.68 | 1.67 | 55.51–57.65 | 4% |
| normalize-title | Go native | 0.35 | 0.60 | 8.20–9.13 | 12% |
| normalize-title | Rust native | 0.18 | 0.48 | 10.41–11.57 | 5% |
| normalize-title | Marimay VM (source run) | 1.24 | 3.19 | 126.55–133.45 | 3% |
| normalize-title | Bun / TypeScript | 0.49 | 1.80 | 16.38–37.49 | 10% |
| word-frequency | Marimay native | 1.54 | 2.72 | 138.88–143.94 | 3% |
| word-frequency | Go native | 0.30 | 0.75 | 18.25–19.55 | 5% |
| word-frequency | Rust native | 0.21 | 0.55 | 29.67–31.55 | 2% |
| word-frequency | Marimay VM (source run) | 2.99 | 5.76 | 248.21–310.58 | 4% |
| word-frequency | Bun / TypeScript | 0.36 | 0.78 | 17.87–19.13 | 6% |
| report-builder | Marimay native | 0.21 | 0.46 | 11.70–13.03 | 5% |
| report-builder | Go native | 0.13 | 0.33 | 4.66–5.56 | 8% |
| report-builder | Rust native | 0.18 | 0.66 | 5.06–6.09 | 12% |
| report-builder | Marimay VM (source run) | 0.54 | 1.77 | 77.03–82.98 | 2% |
| report-builder | Bun / TypeScript | 0.29 | 0.86 | 11.14–12.98 | 7% |
| report-concat | Marimay native | 1.55 | 2.27 | 61.97–70.59 | 7% |
| report-concat | Go native | 2.15 | 4.56 | 78.46–143.07 | 8% |
| report-concat | Rust native | 0.91 | 2.09 | 46.18–54.06 | 6% |
| report-concat | Marimay VM (source run) | 1.00 | 2.01 | 128.23–145.71 | 2% |
| report-concat | Bun / TypeScript | 0.05 | 0.35 | 11.89–12.60 | 3% |
| map-int | Marimay native | 0.26 | 0.56 | 7.34–8.27 | 10% |
| map-int | Go native | 0.29 | 0.63 | 4.77–6.12 | 17% |
| map-int | Rust native | 0.15 | 0.41 | 5.65–6.58 | 7% |
| map-int | Marimay VM (source run) | 1.00 | 2.43 | 66.34–71.81 | 4% |
| map-int | Bun / TypeScript | 0.39 | 1.20 | 12.75–17.38 | 9% |
| map-string | Marimay native | 0.34 | 0.95 | 20.77–22.47 | 5% |
| map-string | Go native | 0.21 | 0.47 | 6.90–8.00 | 8% |
| map-string | Rust native | 0.30 | 0.82 | 9.52–11.65 | 9% |
| map-string | Marimay VM (source run) | 1.27 | 2.78 | 81.88–88.89 | 5% |
| map-string | Bun / TypeScript | 0.45 | 0.79 | 14.24–16.57 | 9% |
| map-string-book | Marimay native | 0.46 | 0.99 | 25.31–27.35 | 5% |
| map-string-book | Go native | 0.28 | 0.56 | 6.04–7.27 | 13% |
| map-string-book | Rust native | 0.36 | 0.73 | 10.01–11.47 | 10% |
| map-string-book | Marimay VM (source run) | 0.85 | 2.90 | 84.32–101.63 | 3% |
| map-string-book | Bun / TypeScript | 0.32 | 0.75 | 14.81–16.20 | 6% |
| set-string-churn | Marimay native | 0.51 | 1.36 | 34.32–36.94 | 4% |
| set-string-churn | Go native | 0.43 | 0.89 | 9.61–12.87 | 13% |
| set-string-churn | Rust native | 0.37 | 1.10 | 16.85–33.21 | 6% |
| set-string-churn | Marimay VM (source run) | 1.72 | 4.51 | 96.18–112.62 | 5% |
| set-string-churn | Bun / TypeScript | 0.49 | 1.02 | 17.16–18.88 | 8% |
| catalog-match | Marimay native | 0.35 | 0.88 | 13.06–15.09 | 8% |
| catalog-match | Go native | 0.34 | 0.79 | 4.57–19.78 | 20% |
| catalog-match | Rust native | 0.30 | 0.71 | 3.74–7.39 | 21% |
| catalog-match | Marimay VM (source run) | 2.22 | 5.16 | 119.61–145.34 | 5% |
| catalog-match | Bun / TypeScript | 0.85 | 1.74 | 18.42–21.22 | 13% |
| catalog-dotcall | Marimay native | 0.31 | 0.65 | 8.54–9.89 | 11% |
| catalog-dotcall | Go native | 0.19 | 0.52 | 4.53–5.32 | 12% |
| catalog-dotcall | Rust native | 0.28 | 0.67 | 3.54–4.42 | 22% |
| catalog-dotcall | Marimay VM (source run) | 1.34 | 2.86 | 100.17–110.84 | 4% |
| catalog-dotcall | Bun / TypeScript | 0.38 | 0.84 | 17.25–22.62 | 6% |
| catalog-interface | Marimay native | 0.12 | 0.24 | 12.21–12.97 | 3% |
| catalog-interface | Go native | 0.16 | 0.25 | 4.53–5.23 | 9% |
| catalog-interface | Rust native | 0.27 | 0.80 | 3.65–4.58 | 20% |
| catalog-interface | Marimay VM (source run) | 1.57 | 3.20 | 123.98–192.68 | 4% |
| catalog-interface | Bun / TypeScript | 0.45 | 0.64 | 17.61–20.30 | 7% |
| record-reference-copy | Marimay native | 0.24 | 0.63 | 6.63–7.49 | 10% |
| record-reference-copy | Go native | 0.11 | 0.28 | 4.41–5.55 | 7% |
| record-reference-copy | Rust native | 0.13 | 0.28 | 3.14–3.89 | 12% |
| record-reference-copy | Marimay VM (source run) | 0.63 | 1.53 | 83.34–87.39 | 2% |
| record-reference-copy | Bun / TypeScript | 0.26 | 0.94 | 13.83–15.36 | 7% |
| record-create | Marimay native | 0.39 | 0.64 | 23.48–25.14 | 5% |
| record-create | Go native | 0.08 | 0.50 | 6.89–7.57 | 7% |
| record-create | Rust native | 0.18 | 0.38 | 6.18–7.99 | 8% |
| record-create | Marimay VM (source run) | 0.85 | 2.22 | 113.19–171.34 | 2% |
| record-create | Bun / TypeScript | 0.42 | 0.86 | 15.24–16.44 | 8% |
| array-grow | Marimay native | 0.12 | 0.26 | 6.79–7.38 | 5% |
| array-grow | Go native | 0.12 | 0.25 | 6.36–6.78 | 5% |
| array-grow | Rust native | 0.14 | 0.43 | 4.13–4.75 | 9% |
| array-grow | Marimay VM (source run) | 0.75 | 1.62 | 81.22–86.57 | 3% |
| array-grow | Bun / TypeScript | 0.35 | 0.69 | 13.57–15.42 | 7% |
| array-reserved | Marimay native | 0.23 | 0.62 | 6.57–7.63 | 10% |
| array-reserved | Go native | 0.16 | 0.35 | 4.96–5.65 | 9% |
| array-reserved | Rust native | 0.13 | 0.39 | 3.86–5.00 | 9% |
| array-reserved | Marimay VM (source run) | 0.87 | 1.82 | 81.43–84.50 | 3% |
| array-reserved | Bun / TypeScript | 0.25 | 1.02 | 12.19–13.52 | 8% |
| bfs-int | Marimay native | 0.14 | 0.35 | 7.32–7.99 | 6% |
| bfs-int | Go native | 0.14 | 0.45 | 5.11–5.99 | 8% |
| bfs-int | Rust native | 0.27 | 0.58 | 4.29–5.21 | 17% |
| bfs-int | Marimay VM (source run) | 0.86 | 1.93 | 71.80–74.67 | 4% |
| bfs-int | Bun / TypeScript | 0.40 | 0.90 | 16.07–18.53 | 7% |
| bfs-record | Marimay native | 0.10 | 0.39 | 9.18–10.19 | 4% |
| bfs-record | Go native | 0.22 | 0.52 | 5.54–6.31 | 11% |
| bfs-record | Rust native | 0.24 | 0.54 | 4.58–18.02 | 15% |
| bfs-record | Marimay VM (source run) | 0.96 | 2.31 | 78.43–81.42 | 4% |
| bfs-record | Bun / TypeScript | 0.44 | 1.17 | 16.56–18.05 | 8% |
| catalog-pipeline | Marimay native | 0.45 | 0.75 | 35.34–37.48 | 4% |
| catalog-pipeline | Go native | 0.12 | 0.25 | 7.65–8.45 | 5% |
| catalog-pipeline | Rust native | 0.43 | 0.94 | 9.75–10.95 | 12% |
| catalog-pipeline | Marimay VM (source run) | 1.51 | 3.11 | 202.75–224.69 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.33 | 0.62 | 22.43–23.95 | 4% |
| parse-result | Marimay native | 0.39 | 1.04 | 17.84–19.56 | 6% |
| parse-result | Go native | 0.14 | 0.47 | 5.60–6.44 | 8% |
| parse-result | Rust native | 0.30 | 0.40 | 5.77–6.69 | 15% |
| parse-result | Marimay VM (source run) | 0.47 | 2.90 | 72.02–87.23 | 4% |
| parse-result | Bun / TypeScript | 0.61 | 0.98 | 12.39–14.85 | 14% |
| insertion | Marimay native | 0.61 | 1.35 | 50.19–52.87 | 3% |
| insertion | Go native | 0.51 | 0.91 | 28.64–30.04 | 5% |
| insertion | Rust native | 0.58 | 1.25 | 27.90–29.71 | 6% |
| insertion | Marimay VM (source run) | 2.53 | 5.31 | 206.89–227.58 | 4% |
| insertion | Bun / TypeScript | 1.36 | 3.05 | 79.42–99.01 | 5% |
| heap-sort | Marimay native | 0.25 | 0.54 | 31.99–34.00 | 2% |
| heap-sort | Go native | 0.30 | 0.57 | 38.51–40.29 | 2% |
| heap-sort | Rust native | 0.29 | 0.61 | 21.59–23.10 | 4% |
| heap-sort | Marimay VM (source run) | 2.04 | 5.48 | 241.18–263.13 | 3% |
| heap-sort | Bun / TypeScript | 1.75 | 3.72 | 91.05–107.84 | 6% |
| stack | Marimay native | 0.33 | 0.79 | 9.62–11.01 | 10% |
| stack | Go native | 0.43 | 0.90 | 14.12–15.99 | 9% |
| stack | Rust native | 0.10 | 0.19 | 8.67–9.60 | 3% |
| stack | Marimay VM (source run) | 1.18 | 3.16 | 262.33–272.18 | 2% |
| stack | Bun / TypeScript | 0.38 | 0.97 | 21.54–23.43 | 5% |
| queue | Marimay native | 0.31 | 0.50 | 9.76–10.90 | 9% |
| queue | Go native | 0.62 | 1.36 | 13.96–15.83 | 13% |
| queue | Rust native | 0.31 | 0.60 | 8.79–9.91 | 10% |
| queue | Marimay VM (source run) | 2.24 | 4.65 | 255.62–307.62 | 3% |
| queue | Bun / TypeScript | 0.37 | 0.81 | 24.92–27.57 | 4% |
| min-heap | Marimay native | 0.29 | 1.25 | 45.55–54.84 | 3% |
| min-heap | Go native | 0.48 | 2.68 | 53.07–56.62 | 5% |
| min-heap | Rust native | 0.77 | 1.37 | 28.49–31.34 | 8% |
| min-heap | Marimay VM (source run) | 8.95 | 20.70 | 513.39–555.63 | 5% |
| min-heap | Bun / TypeScript | 1.79 | 3.73 | 105.40–122.05 | 5% |
| dsu | Marimay native | 0.32 | 0.93 | 29.54–91.52 | 3% |
| dsu | Go native | 0.22 | 0.46 | 23.94–25.60 | 3% |
| dsu | Rust native | 0.44 | 0.98 | 21.31–22.68 | 6% |
| dsu | Marimay VM (source run) | 8.01 | 12.26 | 769.15–800.22 | 3% |
| dsu | Bun / TypeScript | 0.72 | 1.45 | 46.62–61.27 | 5% |
| bfs | Marimay native | 0.94 | 1.93 | 65.08–68.60 | 4% |
| bfs | Go native | 0.90 | 2.13 | 61.07–65.17 | 4% |
| bfs | Rust native | 0.29 | 1.00 | 23.46–25.20 | 4% |
| bfs | Marimay VM (source run) | 6.13 | 26.40 | 653.10–722.10 | 4% |
| bfs | Bun / TypeScript | 0.90 | 2.06 | 67.09–69.83 | 4% |
| dijkstra | Marimay native | 0.48 | 1.04 | 41.28–42.85 | 3% |
| dijkstra | Go native | 0.46 | 0.81 | 40.46–42.38 | 3% |
| dijkstra | Rust native | 0.32 | 0.67 | 38.87–40.79 | 2% |
| dijkstra | Marimay VM (source run) | 0.98 | 4.61 | 199.55–222.24 | 2% |
| dijkstra | Bun / TypeScript | 0.58 | 1.40 | 74.28–77.39 | 2% |
| frequency-map | Marimay native | 0.13 | 0.91 | 32.18–34.26 | 3% |
| frequency-map | Go native | 0.15 | 0.34 | 13.75–14.97 | 3% |
| frequency-map | Rust native | 0.53 | 1.43 | 15.17–24.72 | 7% |
| frequency-map | Marimay VM (source run) | 0.90 | 3.64 | 195.39–204.82 | 2% |
| frequency-map | Bun / TypeScript | 0.46 | 1.10 | 26.97–28.52 | 5% |
| unique-set | Marimay native | 0.35 | 1.03 | 26.59–45.61 | 4% |
| unique-set | Go native | 0.19 | 0.86 | 15.75–16.76 | 5% |
| unique-set | Rust native | 0.46 | 0.85 | 18.70–20.23 | 7% |
| unique-set | Marimay VM (source run) | 0.27 | 1.55 | 130.80–135.49 | 2% |
| unique-set | Bun / TypeScript | 0.32 | 0.95 | 20.53–22.94 | 4% |
| rune-frequency | Marimay native | 0.36 | 0.59 | 21.75–23.32 | 5% |
| rune-frequency | Go native | 0.27 | 0.66 | 5.65–6.93 | 13% |
| rune-frequency | Rust native | 0.27 | 0.62 | 5.77–7.04 | 13% |
| rune-frequency | Marimay VM (source run) | 1.34 | 3.14 | 105.93–130.40 | 4% |
| rune-frequency | Bun / TypeScript | 0.39 | 0.98 | 16.45–19.42 | 7% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/string-publication-v2/comprehensive-current.md`
