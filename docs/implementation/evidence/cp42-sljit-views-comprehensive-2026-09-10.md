# Catalogue and language benchmark

Generated: 2026-09-10T20:18:12+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.27 | 4.47 | 4.47 | 116.76 | 13.23 | 2.07× | 2.07× | — | measured |
| title-search-100k | 100,000 × 128 | 74.38 | 15.16 | 18.21 | 606.68 | 36.66 | 4.91× | 4.08× | — | measured |
| normalize-title | 10,000 × 1 | 57.90 | 9.01 | 11.11 | 184.15 | 17.68 | 6.43× | 5.21× | — | measured |
| word-frequency | 10,000 × 10 | 146.07 | 19.08 | 30.49 | 333.45 | 18.97 | 7.65× | 4.79× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 14.96 | 5.21 | 5.48 | 115.04 | 12.32 | 2.87× | 2.73× | — | measured |
| report-concat | 10,000 × 1 | 97.99 | 80.50 | 47.80 | 194.21 | 12.56 | 1.22× | 2.05× | — | measured |
| map-int | 4,096 × 32 | 7.63 | 5.22 | 6.01 | 90.14 | 13.76 | 1.46× | 1.27× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 23.22 | 7.49 | 9.89 | 115.56 | 15.35 | 3.10× | 2.35× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 31.60 | 6.66 | 10.56 | 127.30 | 15.48 | 4.74× | 2.99× | — | measured |
| set-string-churn | 4,096 × 8 | 47.96 | 10.13 | 17.36 | 145.72 | 17.95 | 4.74× | 2.76× | — | measured |
| catalog-match | 10,000 × 32 | 13.54 | 4.72 | 4.38 | 214.39 | 19.21 | 2.87× | 3.09× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.75 | 4.88 | 3.88 | 167.59 | 18.24 | 1.79× | 2.26× | — | measured |
| catalog-interface | 10,000 × 32 | 12.29 | 4.92 | 4.25 | 220.32 | 18.47 | 2.50× | 2.89× | — | measured |
| record-reference-copy | 1,024 × 128 | 7.03 | 4.56 | 3.34 | 148.37 | 14.73 | 1.54× | 2.11× | — | measured |
| record-create | 1,024 × 128 | 23.23 | 6.84 | 6.43 | 183.69 | 15.95 | 3.40× | 3.62× | — | measured |
| array-grow | 100,000 × 4 | 7.07 | 6.73 | 4.14 | 143.21 | 14.05 | 1.05× | 1.71× | — | measured |
| array-reserved | 100,000 × 4 | 6.98 | 5.39 | 4.33 | 142.99 | 12.91 | 1.30× | 1.61× | — | measured |
| bfs-int | 4,096 × 16 | 7.62 | 5.38 | 4.53 | 120.84 | 17.10 | 1.42× | 1.68× | — | measured |
| bfs-record | 4,096 × 16 | 9.60 | 5.98 | 4.82 | 132.54 | 17.39 | 1.60× | 1.99× | — | measured |
| catalog-pipeline | 10,000 × 16 | 38.08 | 8.18 | 10.46 | 391.60 | 23.26 | 4.66× | 3.64× | — | measured |
| parse-result | 10,000 × 8 | 19.89 | 6.04 | 6.10 | 108.71 | 13.50 | 3.29× | 3.26× | — | measured |
| insertion | fixed anchor | 49.87 | 29.28 | 28.73 | 229.13 | 81.29 | 1.70× | 1.74× | — | measured |
| heap-sort | fixed anchor | 32.82 | 39.29 | 22.38 | 344.28 | 93.65 | 0.84× | 1.47× | ≤ 1.3× | within target |
| stack | fixed anchor | 10.15 | 14.95 | 9.25 | 584.52 | 22.24 | 0.68× | 1.10× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.16 | 14.84 | 9.39 | 611.38 | 26.42 | 0.68× | 1.08× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 47.01 | 54.28 | 30.09 | 922.54 | 110.04 | 0.87× | 1.56× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.20 | 24.59 | 22.10 | 1374.13 | 48.92 | 1.23× | 1.37× | — | measured |
| bfs | fixed anchor | 67.15 | 63.90 | 24.22 | 2123.41 | 69.01 | 1.05× | 2.77× | — | measured |
| dijkstra | fixed anchor | 42.06 | 41.66 | 39.70 | 246.75 | 76.36 | 1.01× | 1.06× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 33.10 | 14.34 | 22.49 | 327.66 | 28.09 | 2.31× | 1.47× | — | measured |
| unique-set | fixed anchor | 27.65 | 16.40 | 19.74 | 240.85 | 22.27 | 1.69× | 1.40× | — | measured |
| rune-frequency | fixed anchor | 22.79 | 6.08 | 6.56 | 173.35 | 17.37 | 3.75× | 3.47× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.18 | 0.63 | 8.92–9.77 | 7% |
| title-search-10k | Go native | 0.22 | 0.46 | 4.13–5.24 | 15% |
| title-search-10k | Rust native | 0.19 | 0.29 | 4.11–5.13 | 13% |
| title-search-10k | Marimay VM (source run) | 0.20 | 0.73 | 115.57–117.11 | 2% |
| title-search-10k | Bun / TypeScript | 0.43 | 0.86 | 12.53–14.91 | 10% |
| title-search-100k | Marimay native | 0.80 | 1.32 | 73.22–78.92 | 3% |
| title-search-100k | Go native | 0.38 | 0.81 | 14.27–15.77 | 8% |
| title-search-100k | Rust native | 0.11 | 0.46 | 18.07–18.92 | 3% |
| title-search-100k | Marimay VM (source run) | 3.85 | 8.08 | 598.66–617.96 | 2% |
| title-search-100k | Bun / TypeScript | 0.21 | 0.43 | 36.31–37.27 | 2% |
| normalize-title | Marimay native | 0.13 | 0.32 | 57.38–58.15 | 2% |
| normalize-title | Go native | 0.21 | 0.32 | 8.52–9.68 | 7% |
| normalize-title | Rust native | 0.33 | 0.63 | 10.57–11.54 | 9% |
| normalize-title | Marimay VM (source run) | 0.95 | 1.69 | 183.14–185.53 | 2% |
| normalize-title | Bun / TypeScript | 0.16 | 0.36 | 17.18–17.93 | 3% |
| word-frequency | Marimay native | 0.10 | 0.76 | 145.06–146.52 | 2% |
| word-frequency | Go native | 0.28 | 0.61 | 18.45–19.57 | 4% |
| word-frequency | Rust native | 0.28 | 0.59 | 30.03–31.06 | 3% |
| word-frequency | Marimay VM (source run) | 0.92 | 1.85 | 332.23–335.38 | 2% |
| word-frequency | Bun / TypeScript | 0.14 | 0.29 | 18.50–20.98 | 2% |
| report-builder | Marimay native | 0.12 | 0.39 | 14.56–15.08 | 3% |
| report-builder | Go native | 0.26 | 0.63 | 4.85–5.78 | 15% |
| report-builder | Rust native | 0.21 | 0.44 | 4.98–5.87 | 12% |
| report-builder | Marimay VM (source run) | 0.41 | 1.06 | 114.06–116.45 | 2% |
| report-builder | Bun / TypeScript | 0.22 | 0.48 | 11.54–14.14 | 5% |
| report-concat | Marimay native | 0.65 | 1.79 | 96.94–100.31 | 2% |
| report-concat | Go native | 0.78 | 1.70 | 79.34–82.74 | 3% |
| report-concat | Rust native | 0.28 | 0.93 | 47.47–55.87 | 2% |
| report-concat | Marimay VM (source run) | 0.77 | 2.13 | 193.44–198.29 | 2% |
| report-concat | Bun / TypeScript | 0.17 | 0.77 | 12.31–18.16 | 6% |
| map-int | Marimay native | 0.14 | 0.39 | 7.44–8.83 | 6% |
| map-int | Go native | 0.37 | 1.09 | 4.84–6.83 | 21% |
| map-int | Rust native | 0.23 | 0.75 | 5.63–10.35 | 12% |
| map-int | Marimay VM (source run) | 0.38 | 0.78 | 89.40–90.85 | 2% |
| map-int | Bun / TypeScript | 0.36 | 1.00 | 12.78–16.81 | 8% |
| map-string | Marimay native | 0.23 | 0.46 | 22.87–23.69 | 3% |
| map-string | Go native | 0.14 | 0.41 | 7.14–7.73 | 6% |
| map-string | Rust native | 0.11 | 0.48 | 9.78–10.58 | 5% |
| map-string | Marimay VM (source run) | 0.44 | 0.84 | 115.11–116.67 | 2% |
| map-string | Bun / TypeScript | 0.31 | 0.66 | 14.79–16.02 | 6% |
| map-string-book | Marimay native | 0.28 | 0.59 | 31.02–32.05 | 3% |
| map-string-book | Go native | 0.13 | 0.38 | 6.50–6.96 | 6% |
| map-string-book | Rust native | 0.28 | 0.68 | 10.28–11.43 | 8% |
| map-string-book | Marimay VM (source run) | 0.17 | 0.46 | 125.96–128.57 | 2% |
| map-string-book | Bun / TypeScript | 0.17 | 0.42 | 14.87–16.22 | 3% |
| set-string-churn | Marimay native | 0.17 | 0.39 | 46.68–48.47 | 2% |
| set-string-churn | Go native | 0.22 | 0.46 | 9.67–10.40 | 6% |
| set-string-churn | Rust native | 0.37 | 0.83 | 16.94–17.93 | 6% |
| set-string-churn | Marimay VM (source run) | 0.65 | 1.16 | 144.54–149.74 | 2% |
| set-string-churn | Bun / TypeScript | 0.17 | 0.49 | 17.59–18.21 | 3% |
| catalog-match | Marimay native | 0.08 | 0.28 | 13.20–15.10 | 2% |
| catalog-match | Go native | 0.11 | 0.31 | 4.53–5.51 | 7% |
| catalog-match | Rust native | 0.20 | 0.64 | 3.83–4.77 | 15% |
| catalog-match | Marimay VM (source run) | 0.37 | 0.98 | 213.48–217.73 | 2% |
| catalog-match | Bun / TypeScript | 0.36 | 0.89 | 18.43–19.80 | 6% |
| catalog-dotcall | Marimay native | 0.21 | 0.34 | 8.52–9.02 | 7% |
| catalog-dotcall | Go native | 0.20 | 0.31 | 4.36–5.15 | 12% |
| catalog-dotcall | Rust native | 0.15 | 0.55 | 3.69–4.35 | 14% |
| catalog-dotcall | Marimay VM (source run) | 0.36 | 0.81 | 166.47–169.33 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.43 | 0.96 | 17.33–18.67 | 7% |
| catalog-interface | Marimay native | 0.21 | 0.43 | 12.05–12.73 | 5% |
| catalog-interface | Go native | 0.06 | 0.13 | 4.49–5.10 | 4% |
| catalog-interface | Rust native | 0.21 | 0.51 | 3.70–4.52 | 15% |
| catalog-interface | Marimay VM (source run) | 0.68 | 1.12 | 218.73–221.05 | 2% |
| catalog-interface | Bun / TypeScript | 0.36 | 0.86 | 17.93–19.74 | 6% |
| record-reference-copy | Marimay native | 0.08 | 0.22 | 6.69–7.19 | 4% |
| record-reference-copy | Go native | 0.18 | 0.28 | 4.35–5.45 | 12% |
| record-reference-copy | Rust native | 0.07 | 0.17 | 3.13–3.53 | 7% |
| record-reference-copy | Marimay VM (source run) | 0.32 | 0.71 | 147.00–149.02 | 2% |
| record-reference-copy | Bun / TypeScript | 0.40 | 0.74 | 13.99–15.19 | 8% |
| record-create | Marimay native | 0.26 | 0.47 | 22.81–24.04 | 3% |
| record-create | Go native | 0.10 | 0.24 | 6.66–7.30 | 5% |
| record-create | Rust native | 0.21 | 0.39 | 6.17–6.90 | 10% |
| record-create | Marimay VM (source run) | 0.46 | 1.38 | 181.87–185.66 | 2% |
| record-create | Bun / TypeScript | 0.16 | 0.34 | 15.38–16.26 | 3% |
| array-grow | Marimay native | 0.20 | 0.30 | 6.76–7.28 | 8% |
| array-grow | Go native | 0.07 | 0.27 | 6.45–7.10 | 4% |
| array-grow | Rust native | 0.14 | 0.68 | 4.00–4.83 | 16% |
| array-grow | Marimay VM (source run) | 0.36 | 1.15 | 142.86–265.72 | 2% |
| array-grow | Bun / TypeScript | 0.17 | 0.31 | 13.83–14.38 | 4% |
| array-reserved | Marimay native | 0.19 | 0.47 | 6.60–7.21 | 8% |
| array-reserved | Go native | 0.11 | 0.22 | 5.15–5.75 | 6% |
| array-reserved | Rust native | 0.40 | 0.80 | 3.88–4.79 | 28% |
| array-reserved | Marimay VM (source run) | 0.20 | 0.41 | 142.31–143.33 | 2% |
| array-reserved | Bun / TypeScript | 0.36 | 0.79 | 12.22–13.48 | 8% |
| bfs-int | Marimay native | 0.10 | 0.47 | 7.33–8.35 | 6% |
| bfs-int | Go native | 0.13 | 0.34 | 5.16–5.74 | 7% |
| bfs-int | Rust native | 0.15 | 0.31 | 4.33–5.10 | 10% |
| bfs-int | Marimay VM (source run) | 0.10 | 0.47 | 120.23–121.08 | 2% |
| bfs-int | Bun / TypeScript | 0.30 | 0.79 | 16.38–17.70 | 5% |
| bfs-record | Marimay native | 0.15 | 0.37 | 9.16–10.02 | 5% |
| bfs-record | Go native | 0.19 | 0.39 | 5.70–6.54 | 10% |
| bfs-record | Rust native | 0.26 | 0.59 | 4.56–5.33 | 16% |
| bfs-record | Marimay VM (source run) | 0.20 | 0.43 | 132.16–133.54 | 2% |
| bfs-record | Bun / TypeScript | 0.28 | 0.67 | 16.79–17.71 | 5% |
| catalog-pipeline | Marimay native | 0.17 | 0.38 | 37.59–38.60 | 2% |
| catalog-pipeline | Go native | 0.09 | 0.31 | 7.85–96.91 | 4% |
| catalog-pipeline | Rust native | 0.37 | 0.78 | 9.90–31.46 | 11% |
| catalog-pipeline | Marimay VM (source run) | 3.68 | 9.36 | 387.09–411.03 | 3% |
| catalog-pipeline | Bun / TypeScript | 0.22 | 0.44 | 22.60–25.07 | 3% |
| parse-result | Marimay native | 0.33 | 0.83 | 19.23–20.23 | 5% |
| parse-result | Go native | 0.17 | 0.49 | 5.65–6.50 | 9% |
| parse-result | Rust native | 0.30 | 0.62 | 5.80–6.69 | 15% |
| parse-result | Marimay VM (source run) | 0.89 | 1.96 | 107.61–110.33 | 2% |
| parse-result | Bun / TypeScript | 0.24 | 0.49 | 12.83–14.28 | 5% |
| insertion | Marimay native | 1.10 | 2.11 | 47.50–52.85 | 7% |
| insertion | Go native | 0.12 | 0.39 | 29.13–30.04 | 2% |
| insertion | Rust native | 0.13 | 0.38 | 28.43–29.19 | 2% |
| insertion | Marimay VM (source run) | 0.40 | 0.97 | 227.89–233.98 | 2% |
| insertion | Bun / TypeScript | 0.49 | 0.82 | 80.31–84.59 | 2% |
| heap-sort | Marimay native | 0.26 | 0.41 | 32.31–33.26 | 2% |
| heap-sort | Go native | 0.11 | 0.25 | 39.07–40.17 | 2% |
| heap-sort | Rust native | 0.22 | 0.61 | 21.95–23.06 | 3% |
| heap-sort | Marimay VM (source run) | 0.77 | 2.29 | 342.88–348.68 | 2% |
| heap-sort | Bun / TypeScript | 0.08 | 0.46 | 93.04–94.25 | 2% |
| stack | Marimay native | 0.18 | 0.42 | 9.71–10.57 | 5% |
| stack | Go native | 0.26 | 0.61 | 14.45–15.45 | 5% |
| stack | Rust native | 0.09 | 0.19 | 8.88–9.43 | 3% |
| stack | Marimay VM (source run) | 3.39 | 6.75 | 575.73–598.82 | 2% |
| stack | Bun / TypeScript | 0.35 | 0.70 | 21.73–23.34 | 5% |
| queue | Marimay native | 0.13 | 0.25 | 9.84–12.02 | 4% |
| queue | Go native | 0.33 | 0.71 | 14.43–15.30 | 7% |
| queue | Rust native | 0.29 | 0.59 | 8.85–9.70 | 9% |
| queue | Marimay VM (source run) | 1.86 | 5.93 | 607.72–740.50 | 2% |
| queue | Bun / TypeScript | 0.39 | 0.80 | 25.86–28.88 | 4% |
| min-heap | Marimay native | 0.14 | 0.49 | 46.40–47.66 | 2% |
| min-heap | Go native | 0.18 | 0.52 | 53.49–54.91 | 2% |
| min-heap | Rust native | 0.32 | 0.78 | 29.26–30.47 | 3% |
| min-heap | Marimay VM (source run) | 4.86 | 10.34 | 912.77–939.29 | 2% |
| min-heap | Bun / TypeScript | 0.73 | 1.12 | 109.02–121.00 | 2% |
| dsu | Marimay native | 0.36 | 0.88 | 29.78–31.43 | 4% |
| dsu | Go native | 0.18 | 0.38 | 24.26–24.95 | 2% |
| dsu | Rust native | 0.27 | 0.60 | 21.55–22.60 | 4% |
| dsu | Marimay VM (source run) | 10.91 | 24.56 | 1361.30–1524.56 | 2% |
| dsu | Bun / TypeScript | 0.32 | 0.60 | 48.29–50.34 | 2% |
| bfs | Marimay native | 0.27 | 0.65 | 66.84–68.44 | 2% |
| bfs | Go native | 0.52 | 2.22 | 61.44–124.73 | 3% |
| bfs | Rust native | 0.19 | 0.74 | 23.59–31.36 | 3% |
| bfs | Marimay VM (source run) | 3.25 | 7.44 | 2119.99–2142.88 | 2% |
| bfs | Bun / TypeScript | 0.31 | 0.69 | 68.55–73.83 | 2% |
| dijkstra | Marimay native | 0.26 | 0.57 | 41.49–42.55 | 2% |
| dijkstra | Go native | 0.23 | 0.52 | 41.02–42.31 | 2% |
| dijkstra | Rust native | 0.27 | 0.55 | 39.20–41.60 | 2% |
| dijkstra | Marimay VM (source run) | 0.72 | 1.55 | 245.62–251.77 | 2% |
| dijkstra | Bun / TypeScript | 0.33 | 0.84 | 75.63–78.64 | 2% |
| frequency-map | Marimay native | 0.14 | 0.32 | 32.63–34.16 | 2% |
| frequency-map | Go native | 0.17 | 0.38 | 13.95–14.54 | 4% |
| frequency-map | Rust native | 1.49 | 5.51 | 13.38–25.95 | 24% |
| frequency-map | Marimay VM (source run) | 5.99 | 11.89 | 319.45–334.18 | 5% |
| frequency-map | Bun / TypeScript | 0.16 | 0.35 | 27.81–29.83 | 2% |
| unique-set | Marimay native | 0.29 | 0.57 | 26.91–49.79 | 3% |
| unique-set | Go native | 0.07 | 0.13 | 16.14–121.55 | 2% |
| unique-set | Rust native | 0.21 | 0.60 | 19.26–21.42 | 3% |
| unique-set | Marimay VM (source run) | 1.67 | 3.53 | 238.50–250.43 | 2% |
| unique-set | Bun / TypeScript | 0.22 | 0.56 | 21.48–22.68 | 3% |
| rune-frequency | Marimay native | 0.25 | 0.53 | 22.38–24.11 | 3% |
| rune-frequency | Go native | 0.38 | 0.79 | 5.62–6.70 | 19% |
| rune-frequency | Rust native | 0.24 | 0.80 | 5.89–7.39 | 12% |
| rune-frequency | Marimay VM (source run) | 0.57 | 2.19 | 172.65–182.16 | 2% |
| rune-frequency | Bun / TypeScript | 0.27 | 0.62 | 16.75–19.18 | 5% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-views/comprehensive-current.md`
