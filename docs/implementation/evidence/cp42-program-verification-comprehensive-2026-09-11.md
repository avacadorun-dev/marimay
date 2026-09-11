# Catalogue and language benchmark

Generated: 2026-09-11T09:40:43+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.21 | 4.27 | 4.46 | 104.92 | 12.71 | 2.16× | 2.06× | — | measured |
| title-search-100k | 100,000 × 128 | 74.02 | 14.95 | 17.91 | 568.38 | 35.57 | 4.95× | 4.13× | — | measured |
| normalize-title | 10,000 × 1 | 56.90 | 8.59 | 10.88 | 168.34 | 17.05 | 6.62× | 5.23× | — | measured |
| word-frequency | 10,000 × 10 | 144.25 | 18.61 | 30.28 | 317.68 | 18.25 | 7.75× | 4.76× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 14.62 | 5.22 | 5.28 | 97.01 | 11.72 | 2.80× | 2.77× | — | measured |
| report-concat | 10,000 × 1 | 95.03 | 81.19 | 46.99 | 172.78 | 12.00 | 1.17× | 2.02× | — | measured |
| map-int | 4,096 × 32 | 7.55 | 4.93 | 5.94 | 80.83 | 13.25 | 1.53× | 1.27× | ≤ 1.5× | above target |
| map-string | 4,096 × 32 | 22.84 | 7.15 | 9.89 | 104.35 | 14.93 | 3.19× | 2.31× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 31.06 | 6.45 | 10.36 | 110.02 | 14.89 | 4.82× | 3.00× | — | measured |
| set-string-churn | 4,096 × 8 | 46.67 | 9.93 | 17.33 | 132.24 | 17.11 | 4.70× | 2.69× | — | measured |
| catalog-match | 10,000 × 32 | 13.27 | 4.59 | 4.04 | 155.23 | 18.52 | 2.89× | 3.28× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.57 | 4.56 | 3.84 | 131.17 | 17.58 | 1.88× | 2.23× | — | measured |
| catalog-interface | 10,000 × 32 | 12.10 | 4.74 | 3.97 | 171.28 | 17.91 | 2.55× | 3.05× | — | measured |
| record-reference-copy | 1,024 × 128 | 6.84 | 4.39 | 3.24 | 106.70 | 13.95 | 1.56× | 2.11× | — | measured |
| record-create | 1,024 × 128 | 22.75 | 6.64 | 6.17 | 141.48 | 15.35 | 3.42× | 3.69× | — | measured |
| array-grow | 100,000 × 4 | 6.84 | 6.40 | 4.23 | 115.39 | 13.44 | 1.07× | 1.62× | — | measured |
| array-reserved | 100,000 × 4 | 6.73 | 5.09 | 4.24 | 115.17 | 12.31 | 1.32× | 1.59× | — | measured |
| bfs-int | 4,096 × 16 | 7.40 | 5.24 | 4.51 | 94.02 | 16.36 | 1.41× | 1.64× | — | measured |
| bfs-record | 4,096 × 16 | 9.23 | 5.85 | 4.78 | 101.86 | 16.79 | 1.58× | 1.93× | — | measured |
| catalog-pipeline | 10,000 × 16 | 37.66 | 7.75 | 9.91 | 261.70 | 22.49 | 4.86× | 3.80× | — | measured |
| parse-result | 10,000 × 8 | 19.23 | 5.75 | 6.05 | 93.15 | 12.93 | 3.34× | 3.18× | — | measured |
| insertion | fixed anchor | 49.45 | 29.09 | 28.25 | 217.75 | 80.16 | 1.70× | 1.75× | — | measured |
| heap-sort | fixed anchor | 32.31 | 39.04 | 22.29 | 295.01 | 92.31 | 0.83× | 1.45× | ≤ 1.3× | within target |
| stack | fixed anchor | 9.86 | 14.43 | 8.86 | 427.23 | 21.47 | 0.68× | 1.11× | ≤ 1.3× | within target |
| queue | fixed anchor | 9.83 | 14.47 | 8.90 | 426.76 | 25.34 | 0.68× | 1.11× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 46.39 | 53.19 | 29.18 | 707.35 | 107.92 | 0.87× | 1.59× | ≤ 1.3× | within target |
| dsu | fixed anchor | 29.87 | 24.22 | 21.85 | 1181.94 | 47.84 | 1.23× | 1.37× | — | measured |
| bfs | fixed anchor | 65.43 | 62.11 | 24.00 | 1348.55 | 67.83 | 1.05× | 2.73× | — | measured |
| dijkstra | fixed anchor | 41.49 | 40.99 | 39.18 | 222.57 | 75.12 | 1.01× | 1.06× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 32.53 | 13.79 | 23.85 | 279.50 | 27.09 | 2.36× | 1.36× | — | measured |
| unique-set | fixed anchor | 26.94 | 16.01 | 18.84 | 199.48 | 21.31 | 1.68× | 1.43× | — | measured |
| rune-frequency | fixed anchor | 22.17 | 5.84 | 5.95 | 150.43 | 16.46 | 3.80× | 3.72× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.19 | 0.44 | 8.82–9.65 | 6% |
| title-search-10k | Go native | 0.12 | 0.25 | 4.07–5.05 | 8% |
| title-search-10k | Rust native | 0.18 | 0.41 | 4.04–4.69 | 12% |
| title-search-10k | Marimay VM (source run) | 0.32 | 0.79 | 104.22–105.80 | 2% |
| title-search-10k | Bun / TypeScript | 0.29 | 0.46 | 12.30–13.65 | 7% |
| title-search-100k | Marimay native | 2.20 | 4.57 | 71.65–76.67 | 9% |
| title-search-100k | Go native | 0.33 | 0.99 | 14.57–36.22 | 7% |
| title-search-100k | Rust native | 0.22 | 0.47 | 17.53–18.29 | 4% |
| title-search-100k | Marimay VM (source run) | 1.89 | 4.36 | 563.65–573.92 | 2% |
| title-search-100k | Bun / TypeScript | 0.13 | 0.34 | 35.34–36.13 | 2% |
| normalize-title | Marimay native | 0.21 | 0.44 | 56.22–58.66 | 2% |
| normalize-title | Go native | 0.27 | 0.55 | 8.08–8.95 | 10% |
| normalize-title | Rust native | 0.25 | 0.50 | 10.47–11.34 | 7% |
| normalize-title | Marimay VM (source run) | 0.62 | 1.16 | 166.72–169.71 | 2% |
| normalize-title | Bun / TypeScript | 0.13 | 0.33 | 16.77–17.37 | 2% |
| word-frequency | Marimay native | 0.66 | 2.71 | 142.47–147.74 | 2% |
| word-frequency | Go native | 0.18 | 0.44 | 18.33–19.26 | 3% |
| word-frequency | Rust native | 0.37 | 0.70 | 29.53–30.68 | 4% |
| word-frequency | Marimay VM (source run) | 1.23 | 2.52 | 315.83–320.82 | 2% |
| word-frequency | Bun / TypeScript | 0.19 | 0.40 | 17.67–18.68 | 3% |
| report-builder | Marimay native | 0.14 | 0.34 | 14.25–15.49 | 3% |
| report-builder | Go native | 0.29 | 0.60 | 4.66–5.63 | 17% |
| report-builder | Rust native | 0.21 | 0.37 | 4.99–5.60 | 12% |
| report-builder | Marimay VM (source run) | 0.31 | 0.81 | 96.54–99.60 | 2% |
| report-builder | Bun / TypeScript | 0.30 | 0.63 | 11.36–12.39 | 8% |
| report-concat | Marimay native | 0.68 | 1.45 | 94.28–97.07 | 2% |
| report-concat | Go native | 0.81 | 2.72 | 78.98–84.42 | 3% |
| report-concat | Rust native | 0.17 | 0.39 | 46.50–47.55 | 2% |
| report-concat | Marimay VM (source run) | 0.73 | 1.73 | 170.13–176.21 | 2% |
| report-concat | Bun / TypeScript | 0.15 | 0.27 | 11.79–12.24 | 4% |
| map-int | Marimay native | 0.14 | 0.28 | 7.10–7.68 | 5% |
| map-int | Go native | 0.11 | 0.74 | 4.77–5.88 | 15% |
| map-int | Rust native | 0.10 | 0.28 | 5.29–6.13 | 5% |
| map-int | Marimay VM (source run) | 0.23 | 0.82 | 80.02–81.06 | 2% |
| map-int | Bun / TypeScript | 0.17 | 0.48 | 12.56–13.74 | 4% |
| map-string | Marimay native | 0.08 | 0.25 | 22.54–23.22 | 2% |
| map-string | Go native | 0.07 | 0.31 | 7.03–7.80 | 4% |
| map-string | Rust native | 0.13 | 0.40 | 9.53–10.20 | 4% |
| map-string | Marimay VM (source run) | 0.49 | 1.07 | 103.38–105.55 | 2% |
| map-string | Bun / TypeScript | 0.21 | 0.69 | 14.29–15.21 | 5% |
| map-string-book | Marimay native | 0.08 | 0.36 | 30.69–31.75 | 2% |
| map-string-book | Go native | 0.11 | 0.23 | 6.16–6.75 | 5% |
| map-string-book | Rust native | 0.19 | 0.54 | 10.07–11.07 | 5% |
| map-string-book | Marimay VM (source run) | 0.27 | 0.60 | 109.22–110.70 | 2% |
| map-string-book | Bun / TypeScript | 0.21 | 0.41 | 14.55–15.30 | 4% |
| set-string-churn | Marimay native | 0.30 | 0.63 | 46.07–47.55 | 2% |
| set-string-churn | Go native | 0.12 | 0.31 | 9.63–10.65 | 4% |
| set-string-churn | Rust native | 0.29 | 0.51 | 16.38–17.70 | 5% |
| set-string-churn | Marimay VM (source run) | 0.58 | 1.94 | 130.56–134.37 | 2% |
| set-string-churn | Bun / TypeScript | 0.19 | 0.54 | 16.88–17.91 | 3% |
| catalog-match | Marimay native | 0.27 | 0.83 | 12.86–13.84 | 6% |
| catalog-match | Go native | 0.06 | 0.14 | 4.43–4.85 | 4% |
| catalog-match | Rust native | 0.22 | 0.39 | 3.70–4.45 | 16% |
| catalog-match | Marimay VM (source run) | 0.49 | 1.09 | 153.38–175.74 | 2% |
| catalog-match | Bun / TypeScript | 0.14 | 0.31 | 17.99–18.93 | 2% |
| catalog-dotcall | Marimay native | 0.06 | 0.17 | 8.03–9.36 | 2% |
| catalog-dotcall | Go native | 0.12 | 0.37 | 4.36–4.89 | 8% |
| catalog-dotcall | Rust native | 0.15 | 0.36 | 3.51–4.18 | 12% |
| catalog-dotcall | Marimay VM (source run) | 0.76 | 1.71 | 130.05–133.44 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.47 | 0.79 | 16.78–19.00 | 8% |
| catalog-interface | Marimay native | 0.09 | 0.41 | 11.67–12.32 | 3% |
| catalog-interface | Go native | 0.12 | 0.22 | 4.58–5.00 | 8% |
| catalog-interface | Rust native | 0.24 | 0.51 | 3.60–4.52 | 18% |
| catalog-interface | Marimay VM (source run) | 0.78 | 1.36 | 169.84–174.46 | 2% |
| catalog-interface | Bun / TypeScript | 0.37 | 0.77 | 17.01–18.50 | 6% |
| record-reference-copy | Marimay native | 0.12 | 0.28 | 6.61–7.07 | 5% |
| record-reference-copy | Go native | 0.13 | 0.35 | 4.21–4.66 | 9% |
| record-reference-copy | Rust native | 0.02 | 0.04 | 3.17–3.33 | 2% |
| record-reference-copy | Marimay VM (source run) | 0.23 | 0.64 | 106.10–107.58 | 2% |
| record-reference-copy | Bun / TypeScript | 0.14 | 0.34 | 13.50–14.47 | 3% |
| record-create | Marimay native | 0.17 | 0.38 | 22.41–23.02 | 2% |
| record-create | Go native | 0.13 | 0.31 | 6.30–7.30 | 6% |
| record-create | Rust native | 0.03 | 0.48 | 6.14–6.88 | 8% |
| record-create | Marimay VM (source run) | 0.48 | 0.75 | 140.64–142.33 | 2% |
| record-create | Bun / TypeScript | 0.19 | 0.60 | 14.89–15.58 | 4% |
| array-grow | Marimay native | 0.13 | 0.28 | 6.55–7.08 | 6% |
| array-grow | Go native | 0.10 | 0.18 | 6.09–6.79 | 5% |
| array-grow | Rust native | 0.14 | 0.30 | 3.95–4.51 | 10% |
| array-grow | Marimay VM (source run) | 0.62 | 1.40 | 114.29–116.53 | 2% |
| array-grow | Bun / TypeScript | 0.14 | 0.46 | 13.13–14.64 | 3% |
| array-reserved | Marimay native | 0.11 | 0.23 | 6.49–6.96 | 5% |
| array-reserved | Go native | 0.11 | 0.32 | 4.89–5.30 | 6% |
| array-reserved | Rust native | 0.17 | 0.54 | 3.73–4.50 | 13% |
| array-reserved | Marimay VM (source run) | 0.24 | 0.69 | 114.38–116.19 | 2% |
| array-reserved | Bun / TypeScript | 0.24 | 0.69 | 11.69–12.88 | 6% |
| bfs-int | Marimay native | 0.13 | 0.32 | 7.26–7.67 | 5% |
| bfs-int | Go native | 0.02 | 0.20 | 5.08–5.54 | 4% |
| bfs-int | Rust native | 0.13 | 0.29 | 4.26–4.64 | 9% |
| bfs-int | Marimay VM (source run) | 0.45 | 1.02 | 92.91–95.89 | 2% |
| bfs-int | Bun / TypeScript | 0.30 | 0.75 | 15.78–17.21 | 6% |
| bfs-record | Marimay native | 0.18 | 0.33 | 8.94–9.74 | 6% |
| bfs-record | Go native | 0.17 | 0.31 | 5.56–6.04 | 8% |
| bfs-record | Rust native | 0.13 | 0.22 | 4.55–5.10 | 8% |
| bfs-record | Marimay VM (source run) | 0.34 | 0.87 | 100.94–102.24 | 2% |
| bfs-record | Bun / TypeScript | 0.38 | 0.73 | 16.21–17.27 | 7% |
| catalog-pipeline | Marimay native | 0.12 | 0.30 | 37.26–38.36 | 2% |
| catalog-pipeline | Go native | 0.10 | 0.21 | 7.56–8.79 | 4% |
| catalog-pipeline | Rust native | 0.18 | 0.63 | 9.72–10.46 | 6% |
| catalog-pipeline | Marimay VM (source run) | 1.25 | 2.99 | 258.82–264.46 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.31 | 0.52 | 21.96–23.03 | 4% |
| parse-result | Marimay native | 0.19 | 0.37 | 18.75–19.58 | 3% |
| parse-result | Go native | 0.12 | 0.38 | 5.55–6.16 | 7% |
| parse-result | Rust native | 0.16 | 0.37 | 5.66–6.22 | 8% |
| parse-result | Marimay VM (source run) | 0.28 | 0.52 | 92.59–94.20 | 2% |
| parse-result | Bun / TypeScript | 0.18 | 0.47 | 12.51–13.11 | 4% |
| insertion | Marimay native | 2.15 | 3.25 | 45.61–53.27 | 13% |
| insertion | Go native | 0.15 | 0.47 | 28.63–29.80 | 2% |
| insertion | Rust native | 0.12 | 0.41 | 28.01–30.38 | 2% |
| insertion | Marimay VM (source run) | 0.22 | 0.84 | 216.76–218.23 | 2% |
| insertion | Bun / TypeScript | 0.26 | 0.60 | 79.54–80.95 | 2% |
| heap-sort | Marimay native | 0.26 | 0.51 | 31.98–32.78 | 2% |
| heap-sort | Go native | 0.15 | 0.27 | 38.49–39.41 | 2% |
| heap-sort | Rust native | 0.27 | 0.83 | 21.70–22.62 | 4% |
| heap-sort | Marimay VM (source run) | 0.59 | 3.87 | 293.93–308.87 | 2% |
| heap-sort | Bun / TypeScript | 0.40 | 0.84 | 91.58–96.24 | 2% |
| stack | Marimay native | 0.07 | 0.23 | 9.45–10.03 | 2% |
| stack | Go native | 0.16 | 0.35 | 14.02–14.83 | 3% |
| stack | Rust native | 0.11 | 0.21 | 8.62–9.04 | 4% |
| stack | Marimay VM (source run) | 4.53 | 7.52 | 418.84–435.23 | 3% |
| stack | Bun / TypeScript | 0.24 | 0.45 | 21.13–21.95 | 3% |
| queue | Marimay native | 0.10 | 0.25 | 9.46–10.29 | 3% |
| queue | Go native | 0.19 | 0.87 | 13.64–15.24 | 6% |
| queue | Rust native | 0.19 | 0.37 | 8.69–9.41 | 6% |
| queue | Marimay VM (source run) | 5.16 | 10.16 | 415.69–446.90 | 4% |
| queue | Bun / TypeScript | 0.26 | 0.61 | 25.04–25.89 | 3% |
| min-heap | Marimay native | 0.43 | 0.88 | 45.77–47.09 | 3% |
| min-heap | Go native | 0.22 | 0.66 | 52.89–53.93 | 2% |
| min-heap | Rust native | 0.20 | 0.46 | 28.71–29.50 | 2% |
| min-heap | Marimay VM (source run) | 5.09 | 10.66 | 700.70–718.38 | 2% |
| min-heap | Bun / TypeScript | 0.25 | 0.53 | 106.73–108.49 | 2% |
| dsu | Marimay native | 0.16 | 0.38 | 29.50–30.22 | 2% |
| dsu | Go native | 0.25 | 0.47 | 23.84–24.72 | 3% |
| dsu | Rust native | 0.33 | 0.74 | 21.36–22.72 | 5% |
| dsu | Marimay VM (source run) | 4.39 | 13.77 | 1167.34–1207.30 | 2% |
| dsu | Bun / TypeScript | 0.36 | 0.80 | 47.38–48.72 | 2% |
| bfs | Marimay native | 0.23 | 0.49 | 65.02–66.14 | 2% |
| bfs | Go native | 1.05 | 2.13 | 60.69–63.63 | 5% |
| bfs | Rust native | 0.24 | 0.52 | 23.50–24.27 | 3% |
| bfs | Marimay VM (source run) | 2.90 | 6.06 | 1344.49–1358.93 | 2% |
| bfs | Bun / TypeScript | 0.29 | 0.48 | 67.09–68.78 | 2% |
| dijkstra | Marimay native | 0.10 | 0.38 | 41.37–41.89 | 2% |
| dijkstra | Go native | 0.18 | 0.30 | 40.40–41.68 | 2% |
| dijkstra | Rust native | 0.30 | 0.62 | 38.78–40.03 | 2% |
| dijkstra | Marimay VM (source run) | 0.67 | 1.32 | 220.98–229.22 | 2% |
| dijkstra | Bun / TypeScript | 0.22 | 0.71 | 74.33–75.55 | 2% |
| frequency-map | Marimay native | 0.12 | 0.27 | 32.03–32.97 | 2% |
| frequency-map | Go native | 0.06 | 0.13 | 13.60–14.00 | 2% |
| frequency-map | Rust native | 0.95 | 5.19 | 13.64–24.86 | 22% |
| frequency-map | Marimay VM (source run) | 0.38 | 0.90 | 278.15–281.11 | 2% |
| frequency-map | Bun / TypeScript | 0.20 | 0.44 | 26.80–27.77 | 2% |
| unique-set | Marimay native | 0.27 | 0.55 | 26.58–27.36 | 3% |
| unique-set | Go native | 0.15 | 0.31 | 15.62–16.68 | 3% |
| unique-set | Rust native | 0.24 | 0.56 | 18.43–19.60 | 4% |
| unique-set | Marimay VM (source run) | 1.72 | 5.47 | 193.21–203.37 | 3% |
| unique-set | Bun / TypeScript | 0.27 | 0.66 | 20.86–21.76 | 4% |
| rune-frequency | Marimay native | 0.06 | 0.10 | 22.11–22.25 | 2% |
| rune-frequency | Go native | 0.23 | 0.51 | 5.44–6.49 | 12% |
| rune-frequency | Rust native | 0.14 | 0.44 | 5.71–6.95 | 7% |
| rune-frequency | Marimay VM (source run) | 0.40 | 0.94 | 149.22–151.35 | 2% |
| rune-frequency | Bun / TypeScript | 0.32 | 0.71 | 15.98–16.92 | 6% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/bytecode-program-preparation/comprehensive-current.md`
