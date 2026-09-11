# Catalogue and language benchmark

Generated: 2026-09-11T08:22:56+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 13.21 | 6.23 | 6.35 | 164.32 | 17.65 | 2.12× | 2.08× | — | noisy; repeat |
| title-search-100k | 100,000 × 128 | 105.68 | 20.54 | 26.13 | 859.47 | 50.35 | 5.15× | 4.04× | — | noisy; repeat |
| normalize-title | 10,000 × 1 | 82.28 | 12.21 | 15.32 | 259.58 | 23.97 | 6.74× | 5.37× | — | noisy; repeat |
| word-frequency | 10,000 × 10 | 208.76 | 26.91 | 43.57 | 477.32 | 25.40 | 7.76× | 4.79× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 21.29 | 6.58 | 7.17 | 157.87 | 16.33 | 3.24× | 2.97× | — | noisy; repeat |
| report-concat | 10,000 × 1 | 135.90 | 100.86 | 68.94 | 268.20 | 16.83 | 1.35× | 1.97× | — | noisy; repeat |
| map-int | 4,096 × 32 | 11.13 | 7.46 | 8.35 | 130.67 | 18.72 | 1.49× | 1.33× | ≤ 1.5× | noisy; repeat |
| map-string | 4,096 × 32 | 33.97 | 10.33 | 14.53 | 165.19 | 21.11 | 3.29× | 2.34× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 44.74 | 8.88 | 14.80 | 175.46 | 21.25 | 5.04× | 3.02× | — | noisy; repeat |
| set-string-churn | 4,096 × 8 | 68.11 | 13.85 | 24.25 | 207.99 | 24.21 | 4.92× | 2.81× | — | measured |
| catalog-match | 10,000 × 32 | 19.07 | 6.63 | 5.90 | 238.80 | 26.48 | 2.88× | 3.23× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 13.00 | 6.73 | 5.51 | 206.78 | 25.71 | 1.93× | 2.36× | — | noisy; repeat |
| catalog-interface | 10,000 × 32 | 17.42 | 6.49 | 5.43 | 263.07 | 25.59 | 2.69× | 3.21× | — | noisy; repeat |
| record-reference-copy | 1,024 × 128 | 9.88 | 6.05 | 4.72 | 170.43 | 20.13 | 1.63× | 2.09× | — | noisy; repeat |
| record-create | 1,024 × 128 | 32.41 | 9.43 | 9.77 | 220.06 | 21.43 | 3.44× | 3.32× | — | noisy; repeat |
| array-grow | 100,000 × 4 | 9.74 | 9.02 | 6.20 | 183.62 | 20.74 | 1.08× | 1.57× | — | measured |
| array-reserved | 100,000 × 4 | 9.86 | 7.27 | 6.25 | 181.00 | 17.64 | 1.36× | 1.58× | — | measured |
| bfs-int | 4,096 × 16 | 10.80 | 7.61 | 6.62 | 150.86 | 23.04 | 1.42× | 1.63× | — | noisy; repeat |
| bfs-record | 4,096 × 16 | 13.89 | 8.03 | 7.04 | 163.51 | 24.19 | 1.73× | 1.97× | — | noisy; repeat |
| catalog-pipeline | 10,000 × 16 | 54.33 | 11.47 | 14.92 | 396.64 | 34.07 | 4.74× | 3.64× | — | noisy; repeat |
| parse-result | 10,000 × 8 | 28.44 | 8.41 | 8.78 | 149.82 | 18.96 | 3.38× | 3.24× | — | noisy; repeat |
| insertion | fixed anchor | 71.67 | 41.60 | 40.79 | 321.26 | 115.70 | 1.72× | 1.76× | — | noisy; repeat |
| heap-sort | fixed anchor | 46.90 | 55.94 | 32.13 | 437.62 | 132.75 | 0.84× | 1.46× | ≤ 1.3× | within target |
| stack | fixed anchor | 14.37 | 19.34 | 12.64 | 640.10 | 30.11 | 0.74× | 1.14× | ≤ 1.3× | within target |
| queue | fixed anchor | 14.84 | 20.07 | 12.74 | 659.25 | 36.45 | 0.74× | 1.16× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 68.25 | 76.97 | 43.87 | 1097.69 | 158.27 | 0.89× | 1.56× | ≤ 1.3× | noisy; repeat |
| dsu | fixed anchor | 44.36 | 34.68 | 31.58 | 1765.03 | 71.68 | 1.28× | 1.40× | — | measured |
| bfs | fixed anchor | 95.82 | 81.03 | 34.10 | 2044.65 | 97.15 | 1.18× | 2.81× | — | measured |
| dijkstra | fixed anchor | 59.84 | 58.04 | 55.81 | 331.13 | 109.83 | 1.03× | 1.07× | ≤ 1.3× | noisy; repeat |
| frequency-map | fixed anchor | 47.08 | 19.78 | 22.86 | 416.47 | 37.55 | 2.38× | 2.06× | — | measured |
| unique-set | fixed anchor | 39.00 | 23.22 | 28.89 | 289.10 | 30.08 | 1.68× | 1.35× | — | noisy; repeat |
| rune-frequency | fixed anchor | 32.55 | 8.02 | 8.57 | 225.23 | 22.97 | 4.06× | 3.80× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 1.22 | 2.68 | 10.18–15.14 | 28% |
| title-search-10k | Go native | 1.18 | 2.31 | 4.71–13.99 | 57% |
| title-search-10k | Rust native | 1.06 | 2.14 | 4.71–7.90 | 50% |
| title-search-10k | Marimay VM (source run) | 9.48 | 40.72 | 123.45–177.81 | 25% |
| title-search-10k | Bun / TypeScript | 1.58 | 4.30 | 13.84–19.87 | 27% |
| title-search-100k | Marimay native | 6.56 | 18.83 | 79.36–124.48 | 19% |
| title-search-100k | Go native | 1.01 | 5.17 | 15.05–22.50 | 25% |
| title-search-100k | Rust native | 3.39 | 6.28 | 19.42–31.84 | 39% |
| title-search-100k | Marimay VM (source run) | 40.62 | 222.48 | 643.52–900.09 | 26% |
| title-search-100k | Bun / TypeScript | 6.91 | 11.07 | 38.26–63.49 | 41% |
| normalize-title | Marimay native | 6.89 | 20.16 | 61.05–93.97 | 25% |
| normalize-title | Go native | 0.93 | 3.47 | 8.85–13.18 | 28% |
| normalize-title | Rust native | 1.79 | 4.40 | 11.65–18.53 | 35% |
| normalize-title | Marimay VM (source run) | 14.72 | 70.15 | 191.03–287.65 | 27% |
| normalize-title | Bun / TypeScript | 1.21 | 6.40 | 18.12–27.11 | 27% |
| word-frequency | Marimay native | 5.05 | 53.55 | 153.69–226.87 | 26% |
| word-frequency | Go native | 2.61 | 6.63 | 19.65–32.44 | 29% |
| word-frequency | Rust native | 0.42 | 10.12 | 32.87–49.55 | 23% |
| word-frequency | Marimay VM (source run) | 33.42 | 121.58 | 353.41–513.42 | 25% |
| word-frequency | Bun / TypeScript | 1.01 | 5.79 | 19.39–29.96 | 23% |
| report-builder | Marimay native | 0.81 | 4.09 | 16.25–22.24 | 19% |
| report-builder | Go native | 0.50 | 1.42 | 5.18–8.03 | 23% |
| report-builder | Rust native | 1.02 | 1.56 | 5.53–8.35 | 43% |
| report-builder | Marimay VM (source run) | 3.58 | 41.71 | 117.69–272.97 | 26% |
| report-builder | Bun / TypeScript | 1.29 | 4.02 | 12.24–18.91 | 25% |
| report-concat | Marimay native | 6.05 | 30.66 | 101.69–145.79 | 23% |
| report-concat | Go native | 5.51 | 30.73 | 74.37–116.88 | 30% |
| report-concat | Rust native | 11.96 | 20.31 | 52.62–107.86 | 52% |
| report-concat | Marimay VM (source run) | 12.88 | 67.94 | 202.78–294.16 | 25% |
| report-concat | Bun / TypeScript | 0.85 | 3.74 | 13.14–18.49 | 22% |
| map-int | Marimay native | 0.93 | 2.99 | 7.96–12.57 | 27% |
| map-int | Go native | 0.80 | 2.47 | 5.19–8.34 | 33% |
| map-int | Rust native | 0.97 | 1.74 | 6.43–10.31 | 35% |
| map-int | Marimay VM (source run) | 4.69 | 34.89 | 95.79–142.34 | 27% |
| map-int | Bun / TypeScript | 1.41 | 4.37 | 13.68–21.49 | 23% |
| map-string | Marimay native | 3.31 | 9.46 | 24.92–38.30 | 29% |
| map-string | Go native | 0.67 | 2.79 | 7.56–11.65 | 27% |
| map-string | Rust native | 1.09 | 4.22 | 10.46–15.62 | 29% |
| map-string | Marimay VM (source run) | 10.05 | 41.73 | 123.02–180.85 | 25% |
| map-string | Bun / TypeScript | 1.92 | 5.97 | 16.06–25.00 | 28% |
| map-string-book | Marimay native | 2.39 | 10.76 | 32.87–47.89 | 24% |
| map-string-book | Go native | 0.60 | 2.14 | 6.37–10.39 | 24% |
| map-string-book | Rust native | 1.88 | 5.01 | 10.95–16.87 | 38% |
| map-string-book | Marimay VM (source run) | 9.91 | 48.90 | 129.89–187.41 | 28% |
| map-string-book | Bun / TypeScript | 3.49 | 5.55 | 15.87–27.86 | 49% |
| set-string-churn | Marimay native | 3.17 | 18.69 | 50.13–73.25 | 27% |
| set-string-churn | Go native | 0.95 | 3.48 | 10.46–15.26 | 25% |
| set-string-churn | Rust native | 1.49 | 6.21 | 18.65–26.45 | 26% |
| set-string-churn | Marimay VM (source run) | 7.52 | 58.31 | 152.40–236.71 | 28% |
| set-string-churn | Bun / TypeScript | 0.64 | 6.10 | 18.27–27.08 | 25% |
| catalog-match | Marimay native | 0.72 | 4.95 | 14.51–20.01 | 26% |
| catalog-match | Go native | 0.17 | 1.61 | 5.08–7.06 | 24% |
| catalog-match | Rust native | 0.36 | 1.37 | 4.22–6.86 | 23% |
| catalog-match | Marimay VM (source run) | 7.85 | 61.65 | 177.45–253.69 | 26% |
| catalog-match | Bun / TypeScript | 1.61 | 7.57 | 19.48–29.90 | 29% |
| catalog-dotcall | Marimay native | 1.06 | 3.86 | 8.97–14.24 | 30% |
| catalog-dotcall | Go native | 0.36 | 1.83 | 4.91–7.28 | 27% |
| catalog-dotcall | Rust native | 0.74 | 1.82 | 4.04–7.56 | 40% |
| catalog-dotcall | Marimay VM (source run) | 4.88 | 55.04 | 153.96–213.80 | 27% |
| catalog-dotcall | Bun / TypeScript | 1.58 | 7.73 | 18.44–28.90 | 30% |
| catalog-interface | Marimay native | 1.32 | 4.72 | 13.13–19.07 | 27% |
| catalog-interface | Go native | 0.68 | 1.78 | 4.88–8.88 | 32% |
| catalog-interface | Rust native | 0.59 | 1.51 | 4.73–11.84 | 33% |
| catalog-interface | Marimay VM (source run) | 9.18 | 65.89 | 201.58–276.17 | 25% |
| catalog-interface | Bun / TypeScript | 1.52 | 5.87 | 19.23–27.42 | 23% |
| record-reference-copy | Marimay native | 0.49 | 2.29 | 7.81–11.82 | 23% |
| record-reference-copy | Go native | 0.98 | 2.20 | 4.34–9.93 | 48% |
| record-reference-copy | Rust native | 0.88 | 1.82 | 3.34–7.04 | 56% |
| record-reference-copy | Marimay VM (source run) | 7.79 | 47.37 | 128.38–181.98 | 28% |
| record-reference-copy | Bun / TypeScript | 0.54 | 5.00 | 15.19–21.35 | 25% |
| record-create | Marimay native | 1.93 | 8.89 | 24.05–39.40 | 27% |
| record-create | Go native | 0.97 | 2.92 | 7.21–13.01 | 31% |
| record-create | Rust native | 0.74 | 2.56 | 6.60–11.04 | 26% |
| record-create | Marimay VM (source run) | 12.23 | 63.36 | 165.47–239.95 | 29% |
| record-create | Bun / TypeScript | 0.62 | 4.98 | 16.17–22.88 | 23% |
| array-grow | Marimay native | 0.61 | 2.46 | 7.62–11.15 | 25% |
| array-grow | Go native | 0.56 | 2.07 | 6.56–10.00 | 23% |
| array-grow | Rust native | 0.49 | 1.12 | 4.37–7.33 | 24% |
| array-grow | Marimay VM (source run) | 7.47 | 50.81 | 133.54–198.37 | 28% |
| array-grow | Bun / TypeScript | 2.43 | 7.88 | 14.30–24.30 | 38% |
| array-reserved | Marimay native | 0.30 | 2.02 | 7.39–10.44 | 20% |
| array-reserved | Go native | 0.51 | 1.83 | 5.55–7.93 | 25% |
| array-reserved | Rust native | 0.42 | 1.53 | 4.32–6.84 | 24% |
| array-reserved | Marimay VM (source run) | 4.63 | 44.38 | 134.43–187.79 | 25% |
| array-reserved | Bun / TypeScript | 0.88 | 4.73 | 12.85–19.54 | 27% |
| bfs-int | Marimay native | 0.93 | 2.94 | 8.58–16.40 | 27% |
| bfs-int | Go native | 0.55 | 2.47 | 5.47–9.68 | 32% |
| bfs-int | Rust native | 0.69 | 1.92 | 4.56–8.22 | 31% |
| bfs-int | Marimay VM (source run) | 4.84 | 40.00 | 111.23–168.44 | 27% |
| bfs-int | Bun / TypeScript | 2.14 | 6.42 | 18.00–28.06 | 28% |
| bfs-record | Marimay native | 0.83 | 4.02 | 9.96–14.94 | 29% |
| bfs-record | Go native | 0.80 | 2.29 | 6.00–10.85 | 30% |
| bfs-record | Rust native | 0.36 | 1.42 | 5.21–8.58 | 20% |
| bfs-record | Marimay VM (source run) | 11.92 | 50.27 | 118.78–351.10 | 31% |
| bfs-record | Bun / TypeScript | 1.07 | 5.89 | 17.11–26.44 | 24% |
| catalog-pipeline | Marimay native | 6.62 | 18.88 | 40.82–67.89 | 37% |
| catalog-pipeline | Go native | 0.79 | 3.06 | 8.49–18.34 | 27% |
| catalog-pipeline | Rust native | 1.39 | 4.20 | 10.56–19.37 | 28% |
| catalog-pipeline | Marimay VM (source run) | 18.63 | 108.81 | 288.61–446.65 | 27% |
| catalog-pipeline | Bun / TypeScript | 3.56 | 12.43 | 23.72–45.67 | 36% |
| parse-result | Marimay native | 1.93 | 7.74 | 19.90–37.61 | 27% |
| parse-result | Go native | 1.17 | 2.88 | 6.14–14.45 | 42% |
| parse-result | Rust native | 1.02 | 2.88 | 6.61–15.44 | 35% |
| parse-result | Marimay VM (source run) | 11.68 | 46.95 | 106.46–486.13 | 31% |
| parse-result | Bun / TypeScript | 1.32 | 6.21 | 12.94–32.13 | 33% |
| insertion | Marimay native | 3.69 | 17.82 | 50.32–95.19 | 25% |
| insertion | Go native | 3.45 | 12.55 | 30.92–46.22 | 30% |
| insertion | Rust native | 2.71 | 11.42 | 30.31–46.55 | 28% |
| insertion | Marimay VM (source run) | 20.77 | 93.65 | 237.74–416.25 | 29% |
| insertion | Bun / TypeScript | 5.87 | 32.44 | 86.10–149.14 | 28% |
| heap-sort | Marimay native | 0.70 | 10.91 | 34.59–53.51 | 23% |
| heap-sort | Go native | 0.84 | 10.97 | 41.73–58.43 | 20% |
| heap-sort | Rust native | 1.60 | 7.90 | 24.90–35.72 | 25% |
| heap-sort | Marimay VM (source run) | 22.28 | 121.32 | 331.68–493.41 | 28% |
| heap-sort | Bun / TypeScript | 2.54 | 33.32 | 100.58–145.70 | 25% |
| stack | Marimay native | 0.64 | 3.22 | 10.94–16.32 | 22% |
| stack | Go native | 0.76 | 4.69 | 15.06–20.82 | 24% |
| stack | Rust native | 0.60 | 2.33 | 9.91–13.88 | 18% |
| stack | Marimay VM (source run) | 45.28 | 183.02 | 488.56–767.29 | 29% |
| stack | Bun / TypeScript | 2.46 | 7.82 | 22.91–42.80 | 26% |
| queue | Marimay native | 1.28 | 4.08 | 10.25–16.40 | 27% |
| queue | Go native | 1.15 | 5.63 | 14.56–21.28 | 28% |
| queue | Rust native | 0.96 | 3.30 | 9.93–15.05 | 26% |
| queue | Marimay VM (source run) | 33.20 | 186.41 | 482.86–700.33 | 28% |
| queue | Bun / TypeScript | 3.25 | 9.57 | 27.31–40.76 | 27% |
| min-heap | Marimay native | 4.56 | 17.97 | 52.20–85.46 | 26% |
| min-heap | Go native | 7.70 | 23.03 | 59.21–88.28 | 30% |
| min-heap | Rust native | 4.62 | 13.99 | 33.68–56.23 | 32% |
| min-heap | Marimay VM (source run) | 58.06 | 325.04 | 822.06–1210.93 | 30% |
| min-heap | Bun / TypeScript | 17.55 | 51.69 | 119.42–181.06 | 33% |
| dsu | Marimay native | 1.20 | 10.71 | 33.71–49.78 | 24% |
| dsu | Go native | 1.47 | 7.37 | 26.86–38.79 | 21% |
| dsu | Rust native | 1.16 | 7.46 | 23.97–35.39 | 24% |
| dsu | Marimay VM (source run) | 42.99 | 414.41 | 1308.21–1927.63 | 23% |
| dsu | Bun / TypeScript | 2.63 | 16.62 | 52.60–78.31 | 23% |
| bfs | Marimay native | 6.48 | 24.40 | 69.92–106.14 | 25% |
| bfs | Go native | 6.50 | 24.03 | 61.85–95.85 | 30% |
| bfs | Rust native | 2.88 | 8.35 | 26.61–45.91 | 25% |
| bfs | Marimay VM (source run) | 146.36 | 548.94 | 1537.55–2241.33 | 27% |
| bfs | Bun / TypeScript | 5.03 | 24.22 | 74.19–113.18 | 25% |
| dijkstra | Marimay native | 6.87 | 16.28 | 44.97–66.86 | 34% |
| dijkstra | Go native | 5.53 | 14.40 | 44.78–64.33 | 29% |
| dijkstra | Rust native | 3.26 | 13.17 | 43.92–59.43 | 24% |
| dijkstra | Marimay VM (source run) | 30.92 | 91.13 | 250.82–539.45 | 28% |
| dijkstra | Bun / TypeScript | 7.64 | 32.05 | 81.18–121.67 | 29% |
| frequency-map | Marimay native | 4.55 | 12.10 | 36.07–55.90 | 29% |
| frequency-map | Go native | 1.33 | 5.29 | 14.92–22.10 | 27% |
| frequency-map | Rust native | 2.01 | 4.45 | 16.18–32.25 | 26% |
| frequency-map | Marimay VM (source run) | 36.81 | 109.12 | 322.84–454.78 | 27% |
| frequency-map | Bun / TypeScript | 3.57 | 9.90 | 28.75–43.39 | 29% |
| unique-set | Marimay native | 3.51 | 11.60 | 29.84–42.89 | 30% |
| unique-set | Go native | 1.95 | 6.53 | 17.47–28.63 | 28% |
| unique-set | Rust native | 4.09 | 10.12 | 21.24–36.22 | 42% |
| unique-set | Marimay VM (source run) | 30.31 | 79.98 | 217.25–323.92 | 31% |
| unique-set | Bun / TypeScript | 1.96 | 8.05 | 23.05–33.68 | 27% |
| rune-frequency | Marimay native | 2.48 | 7.99 | 24.28–36.59 | 25% |
| rune-frequency | Go native | 0.71 | 1.50 | 5.85–9.10 | 26% |
| rune-frequency | Rust native | 0.75 | 1.63 | 6.68–9.64 | 26% |
| rune-frequency | Marimay VM (source run) | 5.37 | 53.77 | 174.13–241.01 | 24% |
| rune-frequency | Bun / TypeScript | 2.28 | 6.72 | 17.37–26.90 | 30% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-prepared-activations/v3-comprehensive-current.md`
