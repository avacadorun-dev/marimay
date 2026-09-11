# Catalogue and language benchmark

Generated: 2026-09-11T13:16:34+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.28 | 4.41 | 4.36 | 91.44 | 12.77 | 2.10× | 2.13× | — | measured |
| title-search-100k | 100,000 × 128 | 76.69 | 15.21 | 18.18 | 504.50 | 36.37 | 5.04× | 4.22× | — | measured |
| normalize-title | 10,000 × 1 | 57.52 | 8.66 | 10.88 | 140.32 | 17.16 | 6.64× | 5.29× | — | measured |
| word-frequency | 10,000 × 10 | 145.60 | 19.04 | 30.49 | 268.86 | 18.40 | 7.65× | 4.77× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 14.85 | 4.93 | 5.44 | 83.16 | 11.86 | 3.01× | 2.73× | — | measured |
| report-concat | 10,000 × 1 | 97.53 | 80.98 | 47.75 | 162.52 | 12.17 | 1.20× | 2.04× | — | measured |
| map-int | 4,096 × 32 | 7.63 | 5.16 | 5.73 | 67.43 | 13.43 | 1.48× | 1.33× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 23.32 | 7.37 | 10.18 | 88.80 | 14.93 | 3.16× | 2.29× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 31.37 | 6.49 | 10.48 | 92.68 | 15.16 | 4.83× | 2.99× | — | measured |
| set-string-churn | 4,096 × 8 | 47.56 | 10.27 | 17.66 | 112.21 | 17.42 | 4.63× | 2.69× | — | measured |
| catalog-match | 10,000 × 32 | 13.57 | 4.76 | 4.19 | 124.09 | 18.72 | 2.85× | 3.24× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.79 | 4.73 | 3.80 | 103.54 | 17.66 | 1.86× | 2.31× | — | measured |
| catalog-interface | 10,000 × 32 | 12.34 | 4.95 | 4.08 | 127.26 | 18.31 | 2.49× | 3.02× | — | measured |
| record-reference-copy | 1,024 × 128 | 6.88 | 4.54 | 3.36 | 88.04 | 14.19 | 1.51× | 2.05× | — | measured |
| record-create | 1,024 × 128 | 22.93 | 6.84 | 6.23 | 116.84 | 15.52 | 3.35× | 3.68× | — | measured |
| array-grow | 100,000 × 4 | 6.91 | 6.50 | 4.47 | 91.84 | 13.71 | 1.06× | 1.55× | — | measured |
| array-reserved | 100,000 × 4 | 6.90 | 5.30 | 4.53 | 91.77 | 12.64 | 1.30× | 1.52× | — | noisy; repeat |
| bfs-int | 4,096 × 16 | 7.57 | 5.40 | 4.69 | 79.57 | 16.54 | 1.40× | 1.62× | — | measured |
| bfs-record | 4,096 × 16 | 9.55 | 5.94 | 4.83 | 85.87 | 16.98 | 1.61× | 1.98× | — | measured |
| catalog-pipeline | 10,000 × 16 | 38.05 | 8.25 | 10.35 | 229.78 | 22.83 | 4.61× | 3.68× | — | measured |
| parse-result | 10,000 × 8 | 19.51 | 5.91 | 6.22 | 78.06 | 13.04 | 3.30× | 3.14× | — | measured |
| insertion | fixed anchor | 50.44 | 29.65 | 29.00 | 212.98 | 81.06 | 1.70× | 1.74× | — | measured |
| heap-sort | fixed anchor | 32.79 | 39.73 | 22.63 | 280.62 | 94.12 | 0.83× | 1.45× | ≤ 1.3× | within target |
| stack | fixed anchor | 9.89 | 14.62 | 9.02 | 320.82 | 21.74 | 0.68× | 1.10× | ≤ 1.3× | within target |
| queue | fixed anchor | 9.99 | 14.71 | 9.07 | 325.17 | 25.70 | 0.68× | 1.10× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 47.21 | 54.30 | 29.93 | 639.76 | 109.68 | 0.87× | 1.58× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.09 | 24.46 | 22.23 | 971.85 | 48.38 | 1.23× | 1.35× | — | measured |
| bfs | fixed anchor | 66.44 | 62.97 | 24.14 | 1143.29 | 68.53 | 1.06× | 2.75× | — | measured |
| dijkstra | fixed anchor | 42.08 | 41.55 | 39.71 | 216.82 | 76.04 | 1.01× | 1.06× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 32.96 | 14.18 | 19.92 | 229.99 | 27.66 | 2.32× | 1.65× | — | noisy; repeat |
| unique-set | fixed anchor | 27.47 | 16.26 | 19.43 | 168.99 | 21.78 | 1.69× | 1.41× | — | measured |
| rune-frequency | fixed anchor | 22.42 | 5.96 | 6.24 | 124.47 | 16.75 | 3.76× | 3.59× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.10 | 0.34 | 8.98–9.58 | 4% |
| title-search-10k | Go native | 0.10 | 0.22 | 4.09–4.75 | 7% |
| title-search-10k | Rust native | 0.21 | 0.48 | 4.15–4.93 | 14% |
| title-search-10k | Marimay VM (source run) | 0.17 | 0.79 | 91.19–93.15 | 2% |
| title-search-10k | Bun / TypeScript | 0.24 | 0.47 | 12.23–13.16 | 6% |
| title-search-100k | Marimay native | 1.93 | 4.17 | 74.30–174.87 | 8% |
| title-search-100k | Go native | 0.27 | 0.89 | 14.31–17.57 | 6% |
| title-search-100k | Rust native | 0.15 | 0.39 | 17.94–20.07 | 2% |
| title-search-100k | Marimay VM (source run) | 2.06 | 4.97 | 500.24–564.72 | 2% |
| title-search-100k | Bun / TypeScript | 0.27 | 0.71 | 35.84–36.78 | 2% |
| normalize-title | Marimay native | 0.34 | 0.82 | 56.77–66.29 | 2% |
| normalize-title | Go native | 0.17 | 0.50 | 8.18–9.75 | 6% |
| normalize-title | Rust native | 0.25 | 0.60 | 10.63–11.92 | 7% |
| normalize-title | Marimay VM (source run) | 0.75 | 1.76 | 139.34–141.53 | 2% |
| normalize-title | Bun / TypeScript | 0.19 | 0.43 | 16.77–19.92 | 3% |
| word-frequency | Marimay native | 1.23 | 5.44 | 144.33–195.29 | 4% |
| word-frequency | Go native | 0.28 | 0.58 | 18.58–25.08 | 4% |
| word-frequency | Rust native | 0.45 | 0.84 | 29.95–31.46 | 4% |
| word-frequency | Marimay VM (source run) | 0.80 | 1.66 | 267.72–272.56 | 2% |
| word-frequency | Bun / TypeScript | 0.15 | 0.43 | 18.24–18.92 | 2% |
| report-builder | Marimay native | 0.32 | 0.84 | 14.49–16.05 | 7% |
| report-builder | Go native | 0.30 | 0.76 | 4.63–5.74 | 18% |
| report-builder | Rust native | 0.30 | 0.53 | 4.79–5.81 | 17% |
| report-builder | Marimay VM (source run) | 0.50 | 1.04 | 82.49–85.23 | 2% |
| report-builder | Bun / TypeScript | 0.28 | 0.56 | 11.58–12.39 | 7% |
| report-concat | Marimay native | 0.32 | 0.79 | 95.75–108.41 | 2% |
| report-concat | Go native | 0.76 | 4.62 | 80.13–86.00 | 6% |
| report-concat | Rust native | 0.19 | 0.44 | 46.56–48.42 | 2% |
| report-concat | Marimay VM (source run) | 0.67 | 2.65 | 160.52–168.27 | 2% |
| report-concat | Bun / TypeScript | 0.09 | 0.24 | 11.98–12.38 | 2% |
| map-int | Marimay native | 0.15 | 0.50 | 7.24–7.88 | 6% |
| map-int | Go native | 0.35 | 0.67 | 4.79–6.09 | 20% |
| map-int | Rust native | 0.34 | 0.73 | 5.38–6.23 | 18% |
| map-int | Marimay VM (source run) | 0.05 | 0.09 | 67.20–67.74 | 2% |
| map-int | Bun / TypeScript | 0.39 | 0.78 | 12.80–14.21 | 9% |
| map-string | Marimay native | 0.37 | 0.74 | 22.91–25.94 | 5% |
| map-string | Go native | 0.15 | 0.33 | 6.96–8.89 | 6% |
| map-string | Rust native | 0.13 | 0.45 | 9.83–11.01 | 4% |
| map-string | Marimay VM (source run) | 0.63 | 2.25 | 87.89–93.36 | 3% |
| map-string | Bun / TypeScript | 0.34 | 0.70 | 14.50–17.69 | 7% |
| map-string-book | Marimay native | 0.27 | 0.63 | 31.00–35.20 | 3% |
| map-string-book | Go native | 0.20 | 0.55 | 6.07–10.59 | 9% |
| map-string-book | Rust native | 0.21 | 0.89 | 10.14–11.68 | 9% |
| map-string-book | Marimay VM (source run) | 1.08 | 2.95 | 90.92–100.67 | 3% |
| map-string-book | Bun / TypeScript | 0.19 | 0.47 | 14.83–20.07 | 4% |
| set-string-churn | Marimay native | 0.29 | 1.07 | 46.55–51.36 | 2% |
| set-string-churn | Go native | 0.21 | 0.54 | 9.57–10.55 | 6% |
| set-string-churn | Rust native | 0.57 | 1.22 | 17.03–19.21 | 10% |
| set-string-churn | Marimay VM (source run) | 0.60 | 15.26 | 111.24–127.54 | 14% |
| set-string-churn | Bun / TypeScript | 0.16 | 0.45 | 17.18–19.51 | 3% |
| catalog-match | Marimay native | 0.14 | 0.35 | 13.25–14.12 | 3% |
| catalog-match | Go native | 0.21 | 0.44 | 4.51–5.14 | 13% |
| catalog-match | Rust native | 0.36 | 0.72 | 3.73–10.70 | 26% |
| catalog-match | Marimay VM (source run) | 0.41 | 2.88 | 123.37–143.37 | 2% |
| catalog-match | Bun / TypeScript | 0.25 | 0.38 | 18.10–27.31 | 4% |
| catalog-dotcall | Marimay native | 0.07 | 0.29 | 8.40–9.56 | 3% |
| catalog-dotcall | Go native | 0.08 | 0.16 | 4.50–4.88 | 5% |
| catalog-dotcall | Rust native | 0.20 | 0.35 | 3.59–4.33 | 16% |
| catalog-dotcall | Marimay VM (source run) | 0.32 | 0.53 | 102.36–111.32 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.21 | 0.98 | 17.03–21.96 | 6% |
| catalog-interface | Marimay native | 0.20 | 0.41 | 11.87–12.87 | 5% |
| catalog-interface | Go native | 0.05 | 0.16 | 4.59–5.08 | 3% |
| catalog-interface | Rust native | 0.32 | 0.67 | 3.62–4.77 | 23% |
| catalog-interface | Marimay VM (source run) | 0.67 | 1.68 | 126.18–135.89 | 2% |
| catalog-interface | Bun / TypeScript | 0.20 | 0.74 | 17.64–23.04 | 4% |
| record-reference-copy | Marimay native | 0.16 | 0.38 | 6.54–7.83 | 7% |
| record-reference-copy | Go native | 0.23 | 0.42 | 4.14–5.43 | 15% |
| record-reference-copy | Rust native | 0.08 | 0.17 | 3.19–4.08 | 7% |
| record-reference-copy | Marimay VM (source run) | 0.62 | 1.58 | 86.92–105.68 | 2% |
| record-reference-copy | Bun / TypeScript | 0.30 | 1.13 | 13.75–17.51 | 8% |
| record-create | Marimay native | 0.30 | 1.20 | 22.35–24.74 | 5% |
| record-create | Go native | 0.15 | 0.70 | 6.59–7.66 | 10% |
| record-create | Rust native | 0.13 | 0.47 | 6.10–10.17 | 8% |
| record-create | Marimay VM (source run) | 0.43 | 1.42 | 116.41–179.06 | 2% |
| record-create | Bun / TypeScript | 0.25 | 0.79 | 15.03–17.86 | 5% |
| array-grow | Marimay native | 0.12 | 0.31 | 6.68–8.12 | 5% |
| array-grow | Go native | 0.08 | 0.21 | 6.07–7.03 | 4% |
| array-grow | Rust native | 0.10 | 0.32 | 3.90–4.68 | 7% |
| array-grow | Marimay VM (source run) | 0.32 | 0.69 | 91.17–97.09 | 2% |
| array-grow | Bun / TypeScript | 0.13 | 0.28 | 13.35–14.75 | 3% |
| array-reserved | Marimay native | 0.20 | 0.41 | 6.57–7.41 | 9% |
| array-reserved | Go native | 0.21 | 0.40 | 4.91–5.69 | 12% |
| array-reserved | Rust native | 0.48 | 0.74 | 3.84–5.23 | 32% |
| array-reserved | Marimay VM (source run) | 0.37 | 0.70 | 90.58–98.88 | 2% |
| array-reserved | Bun / TypeScript | 0.30 | 0.65 | 11.87–15.05 | 7% |
| bfs-int | Marimay native | 0.11 | 0.26 | 7.21–8.96 | 4% |
| bfs-int | Go native | 0.10 | 0.34 | 4.98–5.87 | 6% |
| bfs-int | Rust native | 0.28 | 0.44 | 4.19–5.13 | 18% |
| bfs-int | Marimay VM (source run) | 0.46 | 1.36 | 77.86–87.41 | 2% |
| bfs-int | Bun / TypeScript | 0.32 | 1.32 | 16.07–18.65 | 8% |
| bfs-record | Marimay native | 0.22 | 0.43 | 9.22–10.46 | 7% |
| bfs-record | Go native | 0.16 | 0.35 | 5.68–6.69 | 8% |
| bfs-record | Rust native | 0.22 | 0.44 | 4.47–5.82 | 13% |
| bfs-record | Marimay VM (source run) | 0.42 | 1.54 | 84.71–91.77 | 2% |
| bfs-record | Bun / TypeScript | 0.33 | 0.78 | 16.34–21.55 | 6% |
| catalog-pipeline | Marimay native | 0.23 | 0.96 | 37.76–41.39 | 3% |
| catalog-pipeline | Go native | 0.30 | 0.48 | 7.75–8.65 | 11% |
| catalog-pipeline | Rust native | 0.30 | 0.68 | 9.86–11.85 | 9% |
| catalog-pipeline | Marimay VM (source run) | 0.84 | 2.17 | 227.60–242.56 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.32 | 0.79 | 22.39–27.13 | 4% |
| parse-result | Marimay native | 0.24 | 0.48 | 18.80–21.15 | 4% |
| parse-result | Go native | 0.11 | 0.28 | 5.69–6.43 | 6% |
| parse-result | Rust native | 0.18 | 0.42 | 5.78–7.04 | 9% |
| parse-result | Marimay VM (source run) | 0.59 | 1.21 | 76.59–86.55 | 2% |
| parse-result | Bun / TypeScript | 0.24 | 0.61 | 12.52–16.19 | 6% |
| insertion | Marimay native | 0.81 | 1.70 | 48.56–53.57 | 5% |
| insertion | Go native | 0.25 | 0.53 | 28.94–36.22 | 3% |
| insertion | Rust native | 0.36 | 0.83 | 28.33–37.12 | 4% |
| insertion | Marimay VM (source run) | 1.12 | 5.36 | 211.28–243.61 | 3% |
| insertion | Bun / TypeScript | 0.65 | 2.27 | 80.07–88.20 | 3% |
| heap-sort | Marimay native | 0.24 | 0.75 | 32.24–35.99 | 2% |
| heap-sort | Go native | 0.76 | 2.23 | 38.84–46.80 | 6% |
| heap-sort | Rust native | 0.49 | 1.15 | 21.98–24.32 | 6% |
| heap-sort | Marimay VM (source run) | 1.72 | 41.48 | 278.90–443.78 | 15% |
| heap-sort | Bun / TypeScript | 1.15 | 2.33 | 92.39–107.78 | 4% |
| stack | Marimay native | 0.09 | 0.46 | 9.76–10.81 | 5% |
| stack | Go native | 0.30 | 0.54 | 14.18–15.32 | 6% |
| stack | Rust native | 0.11 | 0.39 | 8.84–9.39 | 4% |
| stack | Marimay VM (source run) | 0.53 | 5.12 | 319.63–342.68 | 2% |
| stack | Bun / TypeScript | 0.39 | 0.73 | 20.92–26.18 | 5% |
| queue | Marimay native | 0.05 | 0.22 | 9.77–10.64 | 2% |
| queue | Go native | 0.27 | 0.67 | 14.12–15.11 | 6% |
| queue | Rust native | 0.16 | 0.40 | 8.89–9.77 | 5% |
| queue | Marimay VM (source run) | 2.18 | 6.42 | 321.82–468.60 | 2% |
| queue | Bun / TypeScript | 0.21 | 0.41 | 24.96–27.68 | 2% |
| min-heap | Marimay native | 0.32 | 0.81 | 46.49–57.77 | 2% |
| min-heap | Go native | 0.72 | 1.56 | 53.54–60.54 | 4% |
| min-heap | Rust native | 0.45 | 0.93 | 29.42–35.39 | 4% |
| min-heap | Marimay VM (source run) | 3.23 | 44.55 | 633.94–824.37 | 7% |
| min-heap | Bun / TypeScript | 0.65 | 1.33 | 108.63–122.66 | 2% |
| dsu | Marimay native | 0.30 | 0.62 | 29.77–33.90 | 3% |
| dsu | Go native | 0.32 | 1.33 | 24.14–26.99 | 5% |
| dsu | Rust native | 0.48 | 1.05 | 21.53–24.41 | 7% |
| dsu | Marimay VM (source run) | 2.90 | 19.75 | 968.95–1107.23 | 2% |
| dsu | Bun / TypeScript | 0.29 | 0.58 | 47.13–50.90 | 2% |
| bfs | Marimay native | 0.43 | 4.13 | 65.63–74.40 | 6% |
| bfs | Go native | 0.77 | 1.68 | 61.50–67.85 | 4% |
| bfs | Rust native | 0.18 | 0.93 | 23.75–27.75 | 4% |
| bfs | Marimay VM (source run) | 6.50 | 20.49 | 1135.09–1409.76 | 2% |
| bfs | Bun / TypeScript | 0.21 | 0.39 | 68.01–77.74 | 2% |
| dijkstra | Marimay native | 0.20 | 0.55 | 41.69–46.58 | 2% |
| dijkstra | Go native | 0.21 | 0.57 | 41.15–45.44 | 2% |
| dijkstra | Rust native | 0.41 | 1.60 | 39.00–42.53 | 4% |
| dijkstra | Marimay VM (source run) | 0.82 | 12.11 | 215.82–256.95 | 6% |
| dijkstra | Bun / TypeScript | 0.58 | 1.27 | 75.11–84.06 | 2% |
| frequency-map | Marimay native | 0.38 | 0.76 | 32.55–37.56 | 3% |
| frequency-map | Go native | 0.10 | 0.31 | 13.76–14.94 | 2% |
| frequency-map | Rust native | 4.38 | 8.96 | 14.15–25.28 | 66% |
| frequency-map | Marimay VM (source run) | 2.34 | 4.73 | 225.51–241.27 | 3% |
| frequency-map | Bun / TypeScript | 0.41 | 0.82 | 26.91–30.31 | 4% |
| unique-set | Marimay native | 0.18 | 1.14 | 26.83–28.80 | 4% |
| unique-set | Go native | 0.16 | 0.37 | 15.86–16.68 | 3% |
| unique-set | Rust native | 0.17 | 0.42 | 18.93–21.21 | 3% |
| unique-set | Marimay VM (source run) | 1.49 | 3.54 | 167.13–175.62 | 3% |
| unique-set | Bun / TypeScript | 0.21 | 0.68 | 21.06–22.65 | 3% |
| rune-frequency | Marimay native | 0.13 | 0.25 | 22.05–22.69 | 2% |
| rune-frequency | Go native | 0.24 | 0.52 | 5.61–6.75 | 12% |
| rune-frequency | Rust native | 0.24 | 0.96 | 5.88–7.21 | 15% |
| rune-frequency | Marimay VM (source run) | 0.80 | 2.38 | 120.44–128.29 | 2% |
| rune-frequency | Bun / TypeScript | 0.45 | 0.91 | 16.07–17.45 | 8% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-ipo-unmetered-cursor/comprehensive-current.md`
