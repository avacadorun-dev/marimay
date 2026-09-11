# Catalogue and language benchmark

Generated: 2026-09-11T16:55:22+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.35 | 4.35 | 4.69 | 93.28 | 13.50 | 2.15× | 1.99× | — | measured |
| title-search-100k | 100,000 × 128 | 81.90 | 15.48 | 18.62 | 515.59 | 37.52 | 5.29× | 4.40× | — | measured |
| normalize-title | 10,000 × 1 | 58.80 | 9.01 | 11.35 | 142.67 | 17.86 | 6.53× | 5.18× | — | measured |
| word-frequency | 10,000 × 10 | 146.77 | 19.42 | 30.93 | 272.80 | 19.18 | 7.56× | 4.75× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 15.37 | 5.42 | 5.57 | 83.51 | 12.51 | 2.83× | 2.76× | — | measured |
| report-concat | 10,000 × 1 | 107.34 | 86.56 | 49.28 | 173.62 | 12.74 | 1.24× | 2.18× | — | measured |
| map-int | 4,096 × 32 | 7.82 | 5.33 | 6.19 | 68.64 | 14.37 | 1.47× | 1.26× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 23.65 | 7.69 | 10.17 | 90.14 | 15.90 | 3.08× | 2.32× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 31.97 | 6.92 | 10.72 | 94.13 | 15.75 | 4.62× | 2.98× | — | measured |
| set-string-churn | 4,096 × 8 | 48.67 | 10.16 | 17.83 | 114.31 | 18.69 | 4.79× | 2.73× | — | measured |
| catalog-match | 10,000 × 32 | 14.10 | 5.00 | 4.33 | 124.53 | 19.80 | 2.82× | 3.25× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 9.01 | 4.85 | 3.87 | 104.50 | 18.61 | 1.86× | 2.33× | — | measured |
| catalog-interface | 10,000 × 32 | 12.53 | 4.89 | 4.31 | 128.06 | 19.08 | 2.56× | 2.91× | — | measured |
| record-reference-copy | 1,024 × 128 | 7.16 | 4.75 | 3.43 | 88.29 | 14.99 | 1.51× | 2.09× | — | measured |
| record-create | 1,024 × 128 | 23.49 | 7.12 | 6.80 | 119.75 | 15.98 | 3.30× | 3.45× | — | measured |
| array-grow | 100,000 × 4 | 7.17 | 6.76 | 4.51 | 92.57 | 14.56 | 1.06× | 1.59× | — | measured |
| array-reserved | 100,000 × 4 | 7.02 | 5.36 | 4.51 | 92.36 | 13.16 | 1.31× | 1.56× | — | measured |
| bfs-int | 4,096 × 16 | 7.85 | 5.66 | 4.81 | 77.51 | 17.35 | 1.39× | 1.63× | — | measured |
| bfs-record | 4,096 × 16 | 9.91 | 6.23 | 5.04 | 84.12 | 17.71 | 1.59× | 1.96× | — | measured |
| catalog-pipeline | 10,000 × 16 | 38.19 | 8.15 | 10.27 | 226.69 | 23.28 | 4.69× | 3.72× | — | measured |
| parse-result | 10,000 × 8 | 19.59 | 6.09 | 6.08 | 78.72 | 13.34 | 3.22× | 3.22× | — | measured |
| insertion | fixed anchor | 49.18 | 29.51 | 28.72 | 214.07 | 81.69 | 1.67× | 1.71× | — | measured |
| heap-sort | fixed anchor | 32.71 | 39.54 | 22.49 | 269.01 | 94.80 | 0.83× | 1.45× | ≤ 1.3× | within target |
| stack | fixed anchor | 10.32 | 15.17 | 9.07 | 325.58 | 22.66 | 0.68× | 1.14× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.30 | 15.17 | 9.35 | 327.09 | 26.92 | 0.68× | 1.10× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 48.20 | 55.25 | 30.33 | 591.34 | 112.16 | 0.87× | 1.59× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.61 | 25.18 | 22.06 | 859.92 | 49.56 | 1.22× | 1.39× | — | measured |
| bfs | fixed anchor | 67.60 | 63.41 | 24.28 | 971.30 | 69.33 | 1.07× | 2.78× | — | measured |
| dijkstra | fixed anchor | 42.38 | 41.31 | 39.64 | 213.68 | 76.69 | 1.03× | 1.07× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 34.32 | 14.71 | 20.87 | 223.70 | 29.24 | 2.33× | 1.64× | — | measured |
| unique-set | fixed anchor | 27.95 | 16.71 | 20.14 | 156.98 | 22.36 | 1.67× | 1.39× | — | measured |
| rune-frequency | fixed anchor | 22.80 | 6.11 | 6.50 | 121.08 | 17.64 | 3.73× | 3.51× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.17 | 0.36 | 9.11–9.69 | 6% |
| title-search-10k | Go native | 0.13 | 0.32 | 4.14–5.13 | 9% |
| title-search-10k | Rust native | 0.23 | 0.52 | 4.20–4.97 | 14% |
| title-search-10k | Marimay VM (source run) | 0.49 | 0.91 | 91.56–94.06 | 2% |
| title-search-10k | Bun / TypeScript | 0.38 | 0.50 | 12.80–14.09 | 8% |
| title-search-100k | Marimay native | 4.02 | 9.05 | 77.53–90.79 | 15% |
| title-search-100k | Go native | 0.26 | 0.56 | 14.93–16.06 | 5% |
| title-search-100k | Rust native | 0.21 | 0.40 | 17.92–18.99 | 3% |
| title-search-100k | Marimay VM (source run) | 5.57 | 8.85 | 502.97–529.89 | 3% |
| title-search-100k | Bun / TypeScript | 0.37 | 1.07 | 36.04–39.48 | 3% |
| normalize-title | Marimay native | 0.52 | 1.15 | 57.22–63.02 | 3% |
| normalize-title | Go native | 0.19 | 0.46 | 8.38–10.22 | 6% |
| normalize-title | Rust native | 0.23 | 0.50 | 10.64–13.09 | 6% |
| normalize-title | Marimay VM (source run) | 0.55 | 2.84 | 138.82–143.67 | 2% |
| normalize-title | Bun / TypeScript | 0.27 | 0.62 | 17.46–19.95 | 5% |
| word-frequency | Marimay native | 0.56 | 1.24 | 144.87–157.60 | 2% |
| word-frequency | Go native | 0.62 | 1.19 | 18.43–23.23 | 10% |
| word-frequency | Rust native | 0.51 | 1.15 | 29.98–33.94 | 5% |
| word-frequency | Marimay VM (source run) | 0.82 | 1.44 | 267.85–286.34 | 2% |
| word-frequency | Bun / TypeScript | 0.20 | 0.58 | 18.49–19.82 | 3% |
| report-builder | Marimay native | 0.24 | 0.50 | 14.65–17.31 | 5% |
| report-builder | Go native | 0.28 | 0.47 | 4.78–6.07 | 15% |
| report-builder | Rust native | 0.30 | 0.52 | 5.03–6.02 | 16% |
| report-builder | Marimay VM (source run) | 0.81 | 2.36 | 82.52–87.03 | 3% |
| report-builder | Bun / TypeScript | 0.23 | 0.46 | 12.28–15.13 | 6% |
| report-concat | Marimay native | 0.63 | 1.47 | 104.65–114.84 | 2% |
| report-concat | Go native | 1.48 | 3.67 | 84.86–104.17 | 5% |
| report-concat | Rust native | 0.71 | 1.60 | 47.55–55.51 | 4% |
| report-concat | Marimay VM (source run) | 0.76 | 2.45 | 170.91–205.59 | 2% |
| report-concat | Bun / TypeScript | 0.11 | 0.30 | 12.34–16.56 | 3% |
| map-int | Marimay native | 0.23 | 0.47 | 7.41–8.24 | 9% |
| map-int | Go native | 0.16 | 0.76 | 5.03–6.51 | 14% |
| map-int | Rust native | 0.15 | 0.44 | 5.71–6.41 | 7% |
| map-int | Marimay VM (source run) | 0.17 | 0.99 | 68.07–69.84 | 2% |
| map-int | Bun / TypeScript | 0.27 | 0.95 | 12.96–14.81 | 7% |
| map-string | Marimay native | 0.26 | 0.63 | 23.16–24.42 | 3% |
| map-string | Go native | 0.11 | 0.18 | 7.30–8.06 | 4% |
| map-string | Rust native | 0.30 | 0.67 | 9.74–12.32 | 9% |
| map-string | Marimay VM (source run) | 0.51 | 1.14 | 89.15–95.42 | 2% |
| map-string | Bun / TypeScript | 0.18 | 0.62 | 15.50–16.53 | 4% |
| map-string-book | Marimay native | 0.42 | 0.73 | 31.05–37.14 | 4% |
| map-string-book | Go native | 0.23 | 0.37 | 6.28–9.37 | 10% |
| map-string-book | Rust native | 0.16 | 0.44 | 10.18–12.74 | 4% |
| map-string-book | Marimay VM (source run) | 0.51 | 1.05 | 93.11–96.05 | 2% |
| map-string-book | Bun / TypeScript | 0.24 | 0.69 | 15.31–16.39 | 5% |
| set-string-churn | Marimay native | 0.40 | 0.97 | 47.78–49.64 | 2% |
| set-string-churn | Go native | 0.29 | 0.73 | 9.83–10.90 | 8% |
| set-string-churn | Rust native | 0.25 | 0.74 | 16.94–20.19 | 4% |
| set-string-churn | Marimay VM (source run) | 0.60 | 3.57 | 113.06–122.75 | 3% |
| set-string-churn | Bun / TypeScript | 0.33 | 0.73 | 18.23–20.67 | 5% |
| catalog-match | Marimay native | 0.13 | 0.62 | 13.39–15.03 | 4% |
| catalog-match | Go native | 0.07 | 0.33 | 4.59–5.85 | 7% |
| catalog-match | Rust native | 0.42 | 0.51 | 3.69–5.22 | 29% |
| catalog-match | Marimay VM (source run) | 0.42 | 2.08 | 123.83–126.26 | 2% |
| catalog-match | Bun / TypeScript | 0.38 | 0.96 | 18.98–21.28 | 6% |
| catalog-dotcall | Marimay native | 0.21 | 0.42 | 8.40–9.39 | 7% |
| catalog-dotcall | Go native | 0.19 | 0.45 | 4.36–5.12 | 12% |
| catalog-dotcall | Rust native | 0.17 | 0.75 | 3.61–4.64 | 19% |
| catalog-dotcall | Marimay VM (source run) | 0.42 | 0.91 | 102.56–106.97 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.30 | 0.67 | 18.25–19.93 | 5% |
| catalog-interface | Marimay native | 0.33 | 0.71 | 11.98–13.36 | 8% |
| catalog-interface | Go native | 0.17 | 0.40 | 4.46–5.79 | 11% |
| catalog-interface | Rust native | 0.33 | 0.54 | 3.78–4.69 | 23% |
| catalog-interface | Marimay VM (source run) | 0.91 | 1.94 | 126.25–131.09 | 2% |
| catalog-interface | Bun / TypeScript | 0.50 | 0.63 | 17.72–21.61 | 8% |
| record-reference-copy | Marimay native | 0.29 | 0.66 | 6.70–8.18 | 12% |
| record-reference-copy | Go native | 0.23 | 0.44 | 4.42–5.64 | 15% |
| record-reference-copy | Rust native | 0.05 | 0.14 | 3.22–3.96 | 4% |
| record-reference-copy | Marimay VM (source run) | 0.76 | 1.49 | 85.84–96.91 | 3% |
| record-reference-copy | Bun / TypeScript | 0.33 | 0.64 | 14.24–18.41 | 7% |
| record-create | Marimay native | 0.39 | 0.73 | 22.52–25.09 | 5% |
| record-create | Go native | 0.28 | 0.59 | 6.69–8.15 | 12% |
| record-create | Rust native | 0.26 | 0.56 | 6.22–7.23 | 11% |
| record-create | Marimay VM (source run) | 2.77 | 6.25 | 116.87–131.50 | 7% |
| record-create | Bun / TypeScript | 0.29 | 0.73 | 15.45–17.14 | 5% |
| array-grow | Marimay native | 0.16 | 0.26 | 6.86–7.38 | 7% |
| array-grow | Go native | 0.13 | 0.27 | 6.36–6.99 | 6% |
| array-grow | Rust native | 0.28 | 0.61 | 4.08–4.93 | 19% |
| array-grow | Marimay VM (source run) | 0.43 | 1.11 | 90.15–98.07 | 2% |
| array-grow | Bun / TypeScript | 0.29 | 0.65 | 13.95–15.32 | 6% |
| array-reserved | Marimay native | 0.09 | 0.33 | 6.60–7.21 | 5% |
| array-reserved | Go native | 0.25 | 0.77 | 5.08–6.40 | 14% |
| array-reserved | Rust native | 0.18 | 0.51 | 3.89–5.01 | 12% |
| array-reserved | Marimay VM (source run) | 0.61 | 1.37 | 90.20–96.15 | 2% |
| array-reserved | Bun / TypeScript | 0.22 | 0.42 | 12.83–14.02 | 5% |
| bfs-int | Marimay native | 0.34 | 0.78 | 7.43–8.50 | 13% |
| bfs-int | Go native | 0.17 | 0.43 | 5.02–7.15 | 9% |
| bfs-int | Rust native | 0.26 | 0.46 | 4.25–5.07 | 16% |
| bfs-int | Marimay VM (source run) | 0.55 | 1.07 | 76.16–83.83 | 2% |
| bfs-int | Bun / TypeScript | 0.45 | 1.02 | 16.48–19.31 | 8% |
| bfs-record | Marimay native | 0.18 | 0.37 | 9.25–10.33 | 5% |
| bfs-record | Go native | 0.28 | 0.58 | 5.91–7.08 | 14% |
| bfs-record | Rust native | 0.18 | 0.38 | 4.60–5.97 | 11% |
| bfs-record | Marimay VM (source run) | 1.10 | 2.30 | 82.60–85.85 | 4% |
| bfs-record | Bun / TypeScript | 0.22 | 0.57 | 17.13–19.35 | 4% |
| catalog-pipeline | Marimay native | 0.19 | 0.55 | 37.69–38.89 | 2% |
| catalog-pipeline | Go native | 0.25 | 0.54 | 7.73–8.59 | 9% |
| catalog-pipeline | Rust native | 0.28 | 0.75 | 9.93–11.15 | 8% |
| catalog-pipeline | Marimay VM (source run) | 1.74 | 3.55 | 222.90–233.40 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.40 | 0.85 | 22.48–24.41 | 5% |
| parse-result | Marimay native | 0.08 | 0.17 | 19.30–20.33 | 2% |
| parse-result | Go native | 0.17 | 0.53 | 5.79–6.51 | 9% |
| parse-result | Rust native | 0.17 | 0.67 | 5.89–6.74 | 11% |
| parse-result | Marimay VM (source run) | 0.36 | 0.84 | 77.66–80.12 | 2% |
| parse-result | Bun / TypeScript | 0.37 | 0.48 | 12.80–14.06 | 8% |
| insertion | Marimay native | 0.73 | 1.55 | 47.35–51.24 | 4% |
| insertion | Go native | 0.32 | 0.71 | 29.18–31.11 | 3% |
| insertion | Rust native | 0.38 | 0.79 | 28.22–30.21 | 4% |
| insertion | Marimay VM (source run) | 2.04 | 4.03 | 210.46–219.86 | 3% |
| insertion | Bun / TypeScript | 0.46 | 2.01 | 81.09–87.10 | 2% |
| heap-sort | Marimay native | 0.38 | 0.76 | 32.33–33.73 | 3% |
| heap-sort | Go native | 0.20 | 0.51 | 39.12–43.46 | 2% |
| heap-sort | Rust native | 0.27 | 0.69 | 22.07–25.10 | 4% |
| heap-sort | Marimay VM (source run) | 1.14 | 2.67 | 264.18–286.92 | 2% |
| heap-sort | Bun / TypeScript | 0.80 | 1.69 | 92.04–106.33 | 3% |
| stack | Marimay native | 0.15 | 0.28 | 9.94–11.08 | 4% |
| stack | Go native | 0.25 | 0.65 | 14.58–15.68 | 5% |
| stack | Rust native | 0.12 | 0.20 | 8.89–9.34 | 4% |
| stack | Marimay VM (source run) | 0.92 | 2.22 | 317.69–337.74 | 2% |
| stack | Bun / TypeScript | 0.23 | 0.47 | 22.00–25.88 | 3% |
| queue | Marimay native | 0.18 | 0.39 | 9.92–10.65 | 5% |
| queue | Go native | 0.28 | 0.57 | 14.70–15.64 | 5% |
| queue | Rust native | 0.20 | 0.43 | 8.88–9.72 | 6% |
| queue | Marimay VM (source run) | 1.04 | 2.13 | 319.98–339.89 | 2% |
| queue | Bun / TypeScript | 0.37 | 0.73 | 26.03–27.53 | 4% |
| min-heap | Marimay native | 0.47 | 1.04 | 47.05–49.18 | 3% |
| min-heap | Go native | 0.27 | 1.08 | 53.50–56.67 | 2% |
| min-heap | Rust native | 0.16 | 0.44 | 29.44–30.83 | 2% |
| min-heap | Marimay VM (source run) | 4.10 | 8.89 | 579.04–753.63 | 2% |
| min-heap | Bun / TypeScript | 0.76 | 1.70 | 111.15–114.46 | 2% |
| dsu | Marimay native | 0.50 | 1.14 | 29.65–33.18 | 5% |
| dsu | Go native | 0.32 | 0.64 | 24.43–26.77 | 4% |
| dsu | Rust native | 0.27 | 0.58 | 21.62–22.98 | 4% |
| dsu | Marimay VM (source run) | 3.89 | 9.06 | 840.20–893.30 | 2% |
| dsu | Bun / TypeScript | 0.61 | 1.27 | 47.88–54.75 | 4% |
| bfs | Marimay native | 0.67 | 1.39 | 66.41–70.83 | 3% |
| bfs | Go native | 0.25 | 1.37 | 60.88–75.69 | 2% |
| bfs | Rust native | 0.11 | 0.34 | 23.77–26.23 | 2% |
| bfs | Marimay VM (source run) | 5.25 | 8.37 | 955.35–1005.47 | 2% |
| bfs | Bun / TypeScript | 0.78 | 1.69 | 67.84–73.66 | 3% |
| dijkstra | Marimay native | 0.09 | 0.28 | 41.24–42.77 | 2% |
| dijkstra | Go native | 0.21 | 0.60 | 40.45–42.27 | 2% |
| dijkstra | Rust native | 0.31 | 0.56 | 39.17–40.98 | 2% |
| dijkstra | Marimay VM (source run) | 0.80 | 1.87 | 210.53–223.86 | 2% |
| dijkstra | Bun / TypeScript | 0.22 | 0.62 | 75.31–77.29 | 2% |
| frequency-map | Marimay native | 0.55 | 0.92 | 33.14–79.76 | 5% |
| frequency-map | Go native | 0.35 | 0.74 | 14.08–112.95 | 7% |
| frequency-map | Rust native | 1.41 | 5.57 | 14.34–22.46 | 27% |
| frequency-map | Marimay VM (source run) | 0.89 | 1.79 | 219.67–236.69 | 2% |
| frequency-map | Bun / TypeScript | 0.49 | 1.22 | 28.01–30.48 | 5% |
| unique-set | Marimay native | 0.28 | 0.80 | 27.19–28.74 | 3% |
| unique-set | Go native | 0.16 | 0.50 | 15.95–18.09 | 3% |
| unique-set | Rust native | 0.48 | 2.29 | 19.11–22.43 | 11% |
| unique-set | Marimay VM (source run) | 0.94 | 1.95 | 153.54–161.39 | 2% |
| unique-set | Bun / TypeScript | 0.18 | 0.56 | 21.73–22.69 | 3% |
| rune-frequency | Marimay native | 0.27 | 0.35 | 22.36–24.65 | 3% |
| rune-frequency | Go native | 0.49 | 0.83 | 5.62–7.30 | 24% |
| rune-frequency | Rust native | 0.23 | 0.50 | 5.90–8.43 | 11% |
| rune-frequency | Marimay VM (source run) | 0.65 | 1.86 | 117.94–123.80 | 2% |
| rune-frequency | Bun / TypeScript | 0.49 | 0.97 | 16.35–18.41 | 8% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/current-accepted-snapshot-2026-09-11/comprehensive-current.md`
