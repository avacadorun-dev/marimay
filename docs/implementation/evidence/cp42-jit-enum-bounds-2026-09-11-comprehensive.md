# Catalogue and language benchmark

Generated: 2026-09-11T20:06:40+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.64 | 4.50 | 4.74 | 92.75 | 13.57 | 2.14× | 2.03× | — | measured |
| title-search-100k | 100,000 × 128 | 80.54 | 15.71 | 18.87 | 502.98 | 36.94 | 5.13× | 4.27× | — | measured |
| normalize-title | 10,000 × 1 | 58.76 | 9.30 | 11.40 | 137.25 | 18.34 | 6.32× | 5.15× | — | measured |
| word-frequency | 10,000 × 10 | 148.10 | 19.63 | 31.41 | 272.84 | 19.01 | 7.54× | 4.72× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 15.43 | 5.31 | 5.48 | 82.99 | 12.34 | 2.91× | 2.82× | — | measured |
| report-concat | 10,000 × 1 | 101.25 | 81.90 | 48.40 | 168.40 | 12.77 | 1.24× | 2.09× | — | measured |
| map-int | 4,096 × 32 | 7.80 | 5.43 | 6.13 | 68.15 | 14.01 | 1.44× | 1.27× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 23.67 | 7.55 | 10.42 | 90.25 | 15.82 | 3.14× | 2.27× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 32.04 | 6.74 | 10.62 | 94.58 | 15.70 | 4.75× | 3.02× | — | measured |
| set-string-churn | 4,096 × 8 | 48.55 | 10.37 | 17.88 | 116.76 | 19.01 | 4.68× | 2.71× | — | measured |
| catalog-match | 10,000 × 32 | 13.80 | 5.06 | 4.52 | 124.91 | 20.12 | 2.73× | 3.05× | ≤ 1.5× | noisy; repeat |
| catalog-dotcall | 10,000 × 32 | 9.26 | 4.86 | 4.29 | 104.84 | 18.79 | 1.90× | 2.16× | — | noisy; repeat |
| catalog-interface | 10,000 × 32 | 12.65 | 5.18 | 4.32 | 128.38 | 19.13 | 2.44× | 2.93× | — | noisy; repeat |
| record-reference-copy | 1,024 × 128 | 7.22 | 4.85 | 3.47 | 85.89 | 14.73 | 1.49× | 2.08× | — | measured |
| record-create | 1,024 × 128 | 24.15 | 7.16 | 6.81 | 118.07 | 16.40 | 3.37× | 3.55× | — | measured |
| array-grow | 100,000 × 4 | 7.29 | 6.76 | 4.63 | 83.78 | 14.21 | 1.08× | 1.57× | — | measured |
| array-reserved | 100,000 × 4 | 7.18 | 5.47 | 4.53 | 83.82 | 13.67 | 1.31× | 1.59× | — | measured |
| bfs-int | 4,096 × 16 | 7.89 | 5.54 | 4.95 | 73.72 | 17.35 | 1.42× | 1.59× | — | measured |
| bfs-record | 4,096 × 16 | 9.74 | 6.07 | 5.19 | 82.20 | 17.66 | 1.61× | 1.88× | — | measured |
| catalog-pipeline | 10,000 × 16 | 38.33 | 8.28 | 10.50 | 228.27 | 23.71 | 4.63× | 3.65× | — | measured |
| parse-result | 10,000 × 8 | 19.83 | 6.15 | 6.13 | 75.64 | 13.69 | 3.22× | 3.24× | — | measured |
| insertion | fixed anchor | 50.27 | 29.74 | 28.89 | 211.18 | 81.98 | 1.69× | 1.74× | — | measured |
| heap-sort | fixed anchor | 33.86 | 40.30 | 22.78 | 248.12 | 95.53 | 0.84× | 1.49× | ≤ 1.3× | within target |
| stack | fixed anchor | 10.50 | 15.39 | 9.33 | 266.97 | 22.97 | 0.68× | 1.12× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.21 | 15.16 | 9.45 | 261.35 | 26.74 | 0.67× | 1.08× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 48.42 | 55.79 | 30.83 | 531.62 | 112.64 | 0.87× | 1.57× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.71 | 25.15 | 22.63 | 785.40 | 49.29 | 1.22× | 1.36× | — | measured |
| bfs | fixed anchor | 68.04 | 63.54 | 24.66 | 668.81 | 69.83 | 1.07× | 2.76× | — | measured |
| dijkstra | fixed anchor | 42.46 | 42.01 | 40.00 | 204.85 | 76.85 | 1.01× | 1.06× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 33.48 | 14.45 | 19.78 | 203.46 | 28.61 | 2.32× | 1.69× | — | measured |
| unique-set | fixed anchor | 27.81 | 16.65 | 19.87 | 135.75 | 22.31 | 1.67× | 1.40× | — | measured |
| rune-frequency | fixed anchor | 22.82 | 6.24 | 6.40 | 109.64 | 17.55 | 3.66× | 3.56× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.31 | 0.51 | 9.23–10.30 | 10% |
| title-search-10k | Go native | 0.10 | 0.64 | 4.24–5.28 | 14% |
| title-search-10k | Rust native | 0.32 | 0.67 | 4.12–5.23 | 21% |
| title-search-10k | Marimay VM (source run) | 1.01 | 3.25 | 91.57–97.10 | 4% |
| title-search-10k | Bun / TypeScript | 0.61 | 1.02 | 12.38–14.83 | 13% |
| title-search-100k | Marimay native | 3.84 | 9.63 | 76.36–88.08 | 14% |
| title-search-100k | Go native | 0.43 | 0.93 | 14.87–17.10 | 8% |
| title-search-100k | Rust native | 0.37 | 0.87 | 17.82–20.92 | 6% |
| title-search-100k | Marimay VM (source run) | 2.82 | 6.42 | 493.81–513.74 | 2% |
| title-search-100k | Bun / TypeScript | 0.31 | 1.09 | 36.24–41.23 | 3% |
| normalize-title | Marimay native | 0.64 | 2.28 | 57.65–60.73 | 4% |
| normalize-title | Go native | 0.21 | 0.42 | 8.84–9.74 | 7% |
| normalize-title | Rust native | 0.22 | 0.55 | 10.92–11.84 | 6% |
| normalize-title | Marimay VM (source run) | 1.80 | 4.41 | 134.56–141.20 | 4% |
| normalize-title | Bun / TypeScript | 0.62 | 1.31 | 17.31–19.16 | 10% |
| word-frequency | Marimay native | 2.14 | 5.65 | 145.97–154.03 | 4% |
| word-frequency | Go native | 0.42 | 1.09 | 18.76–20.85 | 6% |
| word-frequency | Rust native | 0.60 | 1.21 | 30.22–33.80 | 6% |
| word-frequency | Marimay VM (source run) | 1.63 | 3.32 | 270.74–280.00 | 2% |
| word-frequency | Bun / TypeScript | 0.37 | 0.98 | 18.63–20.33 | 6% |
| report-builder | Marimay native | 0.47 | 0.98 | 14.65–16.11 | 9% |
| report-builder | Go native | 0.36 | 0.78 | 4.72–6.52 | 20% |
| report-builder | Rust native | 0.26 | 0.61 | 4.99–6.16 | 14% |
| report-builder | Marimay VM (source run) | 0.63 | 1.08 | 81.42–85.70 | 2% |
| report-builder | Bun / TypeScript | 0.38 | 0.94 | 11.81–13.58 | 9% |
| report-concat | Marimay native | 3.76 | 9.50 | 96.82–109.12 | 11% |
| report-concat | Go native | 1.30 | 2.17 | 79.93–84.87 | 5% |
| report-concat | Rust native | 0.90 | 2.22 | 47.38–50.86 | 6% |
| report-concat | Marimay VM (source run) | 5.37 | 10.14 | 160.96–175.83 | 10% |
| report-concat | Bun / TypeScript | 0.30 | 1.12 | 12.18–64.58 | 9% |
| map-int | Marimay native | 0.24 | 0.88 | 7.39–10.35 | 11% |
| map-int | Go native | 0.18 | 0.38 | 4.96–5.75 | 10% |
| map-int | Rust native | 0.18 | 0.51 | 5.75–6.88 | 9% |
| map-int | Marimay VM (source run) | 0.49 | 2.48 | 67.59–71.29 | 4% |
| map-int | Bun / TypeScript | 0.57 | 1.17 | 12.79–15.54 | 12% |
| map-string | Marimay native | 0.40 | 1.23 | 23.02–25.04 | 5% |
| map-string | Go native | 0.26 | 0.53 | 7.19–8.87 | 10% |
| map-string | Rust native | 0.28 | 0.57 | 9.76–11.57 | 8% |
| map-string | Marimay VM (source run) | 1.55 | 3.10 | 88.34–94.10 | 5% |
| map-string | Bun / TypeScript | 0.41 | 0.82 | 15.12–16.94 | 8% |
| map-string-book | Marimay native | 0.49 | 1.52 | 31.34–34.59 | 5% |
| map-string-book | Go native | 0.23 | 0.60 | 6.20–7.66 | 10% |
| map-string-book | Rust native | 0.31 | 0.79 | 10.14–12.32 | 9% |
| map-string-book | Marimay VM (source run) | 1.78 | 4.32 | 92.57–100.33 | 6% |
| map-string-book | Bun / TypeScript | 0.59 | 0.94 | 15.00–17.49 | 11% |
| set-string-churn | Marimay native | 0.92 | 2.70 | 47.39–52.79 | 6% |
| set-string-churn | Go native | 0.12 | 0.23 | 10.18–11.39 | 4% |
| set-string-churn | Rust native | 0.45 | 1.01 | 16.92–19.32 | 7% |
| set-string-churn | Marimay VM (source run) | 3.51 | 7.09 | 112.40–124.00 | 9% |
| set-string-churn | Bun / TypeScript | 1.07 | 2.57 | 17.94–21.81 | 17% |
| catalog-match | Marimay native | 0.28 | 0.69 | 13.25–14.62 | 6% |
| catalog-match | Go native | 0.10 | 0.29 | 4.63–5.51 | 6% |
| catalog-match | Rust native | 0.61 | 1.18 | 3.79–5.35 | 41% |
| catalog-match | Marimay VM (source run) | 1.85 | 5.16 | 122.05–130.14 | 4% |
| catalog-match | Bun / TypeScript | 0.34 | 1.32 | 18.32–21.26 | 7% |
| catalog-dotcall | Marimay native | 0.15 | 0.52 | 8.76–10.02 | 6% |
| catalog-dotcall | Go native | 0.16 | 0.44 | 4.55–5.65 | 10% |
| catalog-dotcall | Rust native | 0.44 | 0.73 | 3.68–4.86 | 31% |
| catalog-dotcall | Marimay VM (source run) | 0.76 | 2.05 | 102.77–112.00 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.49 | 1.31 | 17.75–20.81 | 8% |
| catalog-interface | Marimay native | 0.40 | 1.03 | 12.05–14.11 | 9% |
| catalog-interface | Go native | 0.13 | 0.45 | 4.57–5.64 | 9% |
| catalog-interface | Rust native | 0.43 | 0.84 | 3.77–5.14 | 30% |
| catalog-interface | Marimay VM (source run) | 2.06 | 5.14 | 125.68–134.46 | 5% |
| catalog-interface | Bun / TypeScript | 0.60 | 1.25 | 17.90–21.11 | 9% |
| record-reference-copy | Marimay native | 0.28 | 0.72 | 6.74–8.53 | 12% |
| record-reference-copy | Go native | 0.33 | 0.72 | 4.41–5.60 | 21% |
| record-reference-copy | Rust native | 0.14 | 0.38 | 3.24–4.00 | 12% |
| record-reference-copy | Marimay VM (source run) | 1.09 | 4.05 | 84.16–90.07 | 5% |
| record-reference-copy | Bun / TypeScript | 0.39 | 1.60 | 14.12–19.19 | 11% |
| record-create | Marimay native | 0.54 | 1.23 | 22.94–24.69 | 7% |
| record-create | Go native | 0.39 | 0.79 | 6.56–7.69 | 16% |
| record-create | Rust native | 0.28 | 0.55 | 6.13–7.64 | 13% |
| record-create | Marimay VM (source run) | 1.37 | 2.80 | 115.04–124.85 | 3% |
| record-create | Bun / TypeScript | 0.81 | 1.45 | 15.47–17.98 | 15% |
| array-grow | Marimay native | 0.24 | 0.53 | 6.63–7.89 | 10% |
| array-grow | Go native | 0.19 | 0.57 | 6.42–7.50 | 8% |
| array-grow | Rust native | 0.16 | 0.42 | 4.22–5.03 | 10% |
| array-grow | Marimay VM (source run) | 1.20 | 3.33 | 81.75–88.21 | 4% |
| array-grow | Bun / TypeScript | 0.51 | 1.22 | 13.71–15.63 | 11% |
| array-reserved | Marimay native | 0.28 | 0.62 | 6.77–8.18 | 12% |
| array-reserved | Go native | 0.22 | 0.51 | 5.04–6.03 | 12% |
| array-reserved | Rust native | 0.29 | 0.79 | 4.08–5.15 | 19% |
| array-reserved | Marimay VM (source run) | 0.99 | 3.30 | 82.14–88.80 | 4% |
| array-reserved | Bun / TypeScript | 0.46 | 1.34 | 12.38–14.90 | 10% |
| bfs-int | Marimay native | 0.19 | 0.45 | 7.43–9.09 | 7% |
| bfs-int | Go native | 0.28 | 0.59 | 5.25–6.43 | 15% |
| bfs-int | Rust native | 0.18 | 0.49 | 4.28–5.39 | 11% |
| bfs-int | Marimay VM (source run) | 0.57 | 2.27 | 71.96–78.46 | 3% |
| bfs-int | Bun / TypeScript | 0.51 | 1.11 | 16.51–19.67 | 9% |
| bfs-record | Marimay native | 0.35 | 1.01 | 9.07–11.02 | 11% |
| bfs-record | Go native | 0.14 | 0.62 | 5.56–6.67 | 10% |
| bfs-record | Rust native | 0.37 | 0.78 | 4.64–6.28 | 21% |
| bfs-record | Marimay VM (source run) | 1.94 | 4.50 | 79.68–86.21 | 7% |
| bfs-record | Bun / TypeScript | 0.31 | 1.42 | 16.58–19.97 | 8% |
| catalog-pipeline | Marimay native | 0.49 | 0.92 | 37.74–39.69 | 4% |
| catalog-pipeline | Go native | 0.21 | 0.50 | 8.02–9.14 | 8% |
| catalog-pipeline | Rust native | 0.34 | 0.70 | 10.12–11.19 | 10% |
| catalog-pipeline | Marimay VM (source run) | 3.92 | 8.16 | 222.43–241.83 | 5% |
| catalog-pipeline | Bun / TypeScript | 0.70 | 1.41 | 22.52–26.12 | 9% |
| parse-result | Marimay native | 0.22 | 0.51 | 19.24–21.57 | 3% |
| parse-result | Go native | 0.25 | 0.58 | 5.88–6.86 | 12% |
| parse-result | Rust native | 0.29 | 0.90 | 5.80–7.36 | 15% |
| parse-result | Marimay VM (source run) | 0.78 | 3.33 | 74.72–80.13 | 4% |
| parse-result | Bun / TypeScript | 0.39 | 0.84 | 13.01–15.09 | 9% |
| insertion | Marimay native | 1.41 | 3.10 | 48.54–56.81 | 8% |
| insertion | Go native | 0.36 | 0.86 | 28.82–31.82 | 4% |
| insertion | Rust native | 0.25 | 0.69 | 28.59–30.52 | 3% |
| insertion | Marimay VM (source run) | 1.08 | 3.14 | 208.69–221.05 | 2% |
| insertion | Bun / TypeScript | 0.75 | 1.56 | 80.40–87.39 | 3% |
| heap-sort | Marimay native | 0.75 | 2.47 | 32.90–38.44 | 7% |
| heap-sort | Go native | 0.62 | 2.03 | 39.55–42.70 | 5% |
| heap-sort | Rust native | 0.49 | 1.09 | 22.12–25.36 | 6% |
| heap-sort | Marimay VM (source run) | 1.85 | 6.61 | 244.36–256.30 | 3% |
| heap-sort | Bun / TypeScript | 1.79 | 6.24 | 93.34–104.47 | 7% |
| stack | Marimay native | 0.09 | 0.23 | 9.85–12.02 | 3% |
| stack | Go native | 0.42 | 0.90 | 14.67–15.98 | 8% |
| stack | Rust native | 0.30 | 0.76 | 8.92–10.68 | 10% |
| stack | Marimay VM (source run) | 1.99 | 9.30 | 264.26–281.80 | 3% |
| stack | Bun / TypeScript | 0.63 | 1.43 | 21.68–25.33 | 8% |
| queue | Marimay native | 0.18 | 0.60 | 9.75–11.11 | 6% |
| queue | Go native | 0.19 | 0.62 | 14.69–16.32 | 4% |
| queue | Rust native | 0.26 | 0.60 | 8.89–10.44 | 8% |
| queue | Marimay VM (source run) | 2.59 | 4.80 | 256.99–419.35 | 3% |
| queue | Bun / TypeScript | 0.47 | 1.28 | 25.87–29.25 | 5% |
| min-heap | Marimay native | 1.16 | 2.91 | 47.11–51.34 | 7% |
| min-heap | Go native | 1.20 | 1.96 | 54.16–59.22 | 6% |
| min-heap | Rust native | 0.98 | 2.37 | 29.85–33.59 | 10% |
| min-heap | Marimay VM (source run) | 7.85 | 20.73 | 521.41–699.57 | 4% |
| min-heap | Bun / TypeScript | 2.26 | 4.25 | 109.39–115.99 | 6% |
| dsu | Marimay native | 0.47 | 1.01 | 30.04–31.70 | 5% |
| dsu | Go native | 0.42 | 1.22 | 24.62–26.54 | 5% |
| dsu | Rust native | 0.42 | 0.78 | 21.80–25.80 | 6% |
| dsu | Marimay VM (source run) | 1.88 | 15.22 | 781.09–842.11 | 2% |
| dsu | Bun / TypeScript | 0.50 | 1.09 | 48.27–54.04 | 3% |
| bfs | Marimay native | 1.10 | 2.73 | 66.80–73.36 | 5% |
| bfs | Go native | 0.76 | 2.25 | 60.93–65.90 | 4% |
| bfs | Rust native | 0.44 | 1.66 | 24.11–89.76 | 7% |
| bfs | Marimay VM (source run) | 4.07 | 11.39 | 658.32–697.31 | 2% |
| bfs | Bun / TypeScript | 0.93 | 2.85 | 68.82–74.41 | 4% |
| dijkstra | Marimay native | 0.20 | 0.43 | 41.88–43.19 | 2% |
| dijkstra | Go native | 0.56 | 1.14 | 41.22–44.14 | 4% |
| dijkstra | Rust native | 0.36 | 0.56 | 39.38–62.75 | 3% |
| dijkstra | Marimay VM (source run) | 1.07 | 3.85 | 202.04–213.64 | 2% |
| dijkstra | Bun / TypeScript | 0.20 | 0.80 | 75.91–77.95 | 2% |
| frequency-map | Marimay native | 0.26 | 0.68 | 32.87–157.09 | 2% |
| frequency-map | Go native | 0.14 | 0.52 | 14.16–33.62 | 4% |
| frequency-map | Rust native | 1.80 | 3.14 | 13.91–24.79 | 27% |
| frequency-map | Marimay VM (source run) | 0.21 | 0.63 | 200.77–207.51 | 2% |
| frequency-map | Bun / TypeScript | 0.51 | 1.10 | 27.93–29.57 | 5% |
| unique-set | Marimay native | 0.18 | 0.51 | 27.33–28.60 | 2% |
| unique-set | Go native | 0.22 | 0.37 | 16.32–17.13 | 4% |
| unique-set | Rust native | 0.46 | 1.22 | 19.08–20.77 | 7% |
| unique-set | Marimay VM (source run) | 0.62 | 1.25 | 134.14–137.89 | 2% |
| unique-set | Bun / TypeScript | 0.24 | 0.66 | 21.98–23.46 | 3% |
| rune-frequency | Marimay native | 0.26 | 0.62 | 22.36–23.81 | 3% |
| rune-frequency | Go native | 0.35 | 0.78 | 5.74–7.24 | 17% |
| rune-frequency | Rust native | 0.42 | 0.67 | 5.95–7.63 | 20% |
| rune-frequency | Marimay VM (source run) | 0.55 | 1.66 | 108.34–116.36 | 2% |
| rune-frequency | Bun / TypeScript | 0.33 | 0.81 | 16.67–18.85 | 6% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-enum-bounds/comprehensive-current.md`
