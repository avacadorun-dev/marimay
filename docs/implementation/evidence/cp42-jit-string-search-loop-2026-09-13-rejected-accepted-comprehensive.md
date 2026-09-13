# Catalogue and language benchmark

Generated: 2026-09-13T23:20:12+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 7.00 | 4.64 | 4.85 | 57.80 | 13.77 | 1.51× | 1.44× | — | measured |
| title-search-100k | 100,000 × 128 | 46.83 | 16.06 | 19.31 | 154.59 | 38.41 | 2.92× | 2.42× | — | measured |
| normalize-title | 10,000 × 1 | 58.19 | 9.33 | 11.70 | 133.35 | 18.26 | 6.24× | 4.97× | — | measured |
| word-frequency | 10,000 × 10 | 146.23 | 19.78 | 31.29 | 260.12 | 19.12 | 7.39× | 4.67× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 12.32 | 5.34 | 5.95 | 81.56 | 12.88 | 2.31× | 2.07× | — | measured |
| report-concat | 10,000 × 1 | 65.70 | 82.91 | 48.91 | 136.96 | 13.32 | 0.79× | 1.34× | — | measured |
| map-int | 4,096 × 32 | 7.85 | 5.38 | 6.51 | 68.74 | 14.28 | 1.46× | 1.21× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 22.12 | 7.73 | 10.63 | 85.36 | 16.16 | 2.86× | 2.08× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 27.01 | 6.87 | 10.88 | 87.15 | 16.07 | 3.93× | 2.48× | — | measured |
| set-string-churn | 4,096 × 8 | 36.27 | 10.25 | 17.57 | 100.55 | 18.61 | 3.54× | 2.06× | — | measured |
| catalog-match | 10,000 × 32 | 14.04 | 4.94 | 4.17 | 124.39 | 19.64 | 2.85× | 3.37× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 9.32 | 4.88 | 4.45 | 106.28 | 19.26 | 1.91× | 2.10× | — | noisy; repeat |
| catalog-interface | 10,000 × 32 | 12.84 | 4.88 | 4.48 | 128.93 | 19.23 | 2.63× | 2.87× | — | noisy; repeat |
| record-reference-copy | 1,024 × 128 | 7.22 | 4.83 | 3.48 | 86.43 | 15.19 | 1.49× | 2.08× | — | measured |
| record-create | 1,024 × 128 | 24.86 | 7.35 | 6.93 | 117.76 | 16.55 | 3.38× | 3.59× | — | measured |
| array-grow | 100,000 × 4 | 7.47 | 6.80 | 4.81 | 84.49 | 14.53 | 1.10× | 1.55× | — | measured |
| array-reserved | 100,000 × 4 | 7.75 | 5.52 | 4.71 | 84.14 | 13.49 | 1.41× | 1.65× | — | noisy; repeat |
| bfs-int | 4,096 × 16 | 8.14 | 5.61 | 5.04 | 75.58 | 17.91 | 1.45× | 1.62× | — | measured |
| bfs-record | 4,096 × 16 | 10.15 | 6.41 | 5.59 | 81.87 | 18.12 | 1.58× | 1.82× | — | measured |
| catalog-pipeline | 10,000 × 16 | 37.00 | 8.42 | 10.78 | 209.74 | 24.14 | 4.40× | 3.43× | — | measured |
| parse-result | 10,000 × 8 | 19.01 | 6.34 | 6.55 | 75.33 | 13.94 | 3.00× | 2.90× | — | measured |
| insertion | fixed anchor | 51.68 | 30.04 | 29.05 | 212.63 | 82.52 | 1.72× | 1.78× | — | measured |
| heap-sort | fixed anchor | 34.42 | 40.37 | 22.89 | 250.07 | 98.21 | 0.85× | 1.50× | ≤ 1.3× | within target |
| stack | fixed anchor | 10.46 | 15.88 | 9.85 | 268.59 | 23.52 | 0.66× | 1.06× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.85 | 15.43 | 9.55 | 265.09 | 28.43 | 0.70× | 1.14× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 52.25 | 57.41 | 31.08 | 535.23 | 112.53 | 0.91× | 1.68× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.86 | 25.06 | 22.76 | 834.44 | 50.10 | 1.23× | 1.36× | — | measured |
| bfs | fixed anchor | 72.65 | 64.05 | 25.46 | 676.43 | 71.77 | 1.13× | 2.85× | — | measured |
| dijkstra | fixed anchor | 43.61 | 42.72 | 41.28 | 205.95 | 78.43 | 1.02× | 1.06× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 34.34 | 14.75 | 18.69 | 207.76 | 29.86 | 2.33× | 1.84× | — | noisy; repeat |
| unique-set | fixed anchor | 28.05 | 17.11 | 20.70 | 136.95 | 23.14 | 1.64× | 1.35× | — | measured |
| rune-frequency | fixed anchor | 23.17 | 6.14 | 6.84 | 110.54 | 18.05 | 3.77× | 3.39× | — | noisy; repeat |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.14 | 0.43 | 6.46–9.55 | 6% |
| title-search-10k | Go native | 0.27 | 0.69 | 4.29–5.87 | 18% |
| title-search-10k | Rust native | 0.27 | 0.82 | 4.04–6.62 | 17% |
| title-search-10k | Marimay VM (source run) | 1.14 | 2.43 | 56.65–188.23 | 6% |
| title-search-10k | Bun / TypeScript | 0.43 | 0.89 | 13.19–15.49 | 9% |
| title-search-100k | Marimay native | 0.71 | 4.26 | 45.52–54.54 | 9% |
| title-search-100k | Go native | 0.58 | 1.83 | 14.94–18.94 | 11% |
| title-search-100k | Rust native | 0.55 | 0.90 | 18.34–22.49 | 9% |
| title-search-100k | Marimay VM (source run) | 4.27 | 10.48 | 148.37–202.32 | 8% |
| title-search-100k | Bun / TypeScript | 0.54 | 1.29 | 36.59–67.61 | 4% |
| normalize-title | Marimay native | 0.75 | 6.20 | 57.35–66.95 | 11% |
| normalize-title | Go native | 0.60 | 1.58 | 8.67–11.41 | 19% |
| normalize-title | Rust native | 0.40 | 0.83 | 10.90–13.62 | 10% |
| normalize-title | Marimay VM (source run) | 2.77 | 6.91 | 130.32–153.19 | 6% |
| normalize-title | Bun / TypeScript | 0.33 | 1.99 | 17.77–20.58 | 11% |
| word-frequency | Marimay native | 2.02 | 6.33 | 143.92–161.32 | 4% |
| word-frequency | Go native | 0.48 | 1.79 | 18.99–22.19 | 9% |
| word-frequency | Rust native | 0.77 | 2.08 | 30.31–34.83 | 7% |
| word-frequency | Marimay VM (source run) | 3.73 | 20.60 | 256.39–397.84 | 8% |
| word-frequency | Bun / TypeScript | 0.10 | 0.60 | 18.93–22.12 | 3% |
| report-builder | Marimay native | 0.24 | 2.18 | 12.05–15.34 | 18% |
| report-builder | Go native | 0.31 | 1.06 | 4.82–6.36 | 20% |
| report-builder | Rust native | 0.26 | 0.50 | 5.15–7.56 | 13% |
| report-builder | Marimay VM (source run) | 1.51 | 5.43 | 79.56–88.43 | 7% |
| report-builder | Bun / TypeScript | 0.54 | 1.88 | 11.69–14.71 | 15% |
| report-concat | Marimay native | 1.58 | 6.49 | 64.06–111.14 | 10% |
| report-concat | Go native | 1.67 | 3.69 | 80.41–187.42 | 6% |
| report-concat | Rust native | 0.49 | 10.75 | 48.43–65.33 | 22% |
| report-concat | Marimay VM (source run) | 3.20 | 6.95 | 131.50–264.66 | 7% |
| report-concat | Bun / TypeScript | 0.67 | 2.97 | 12.64–16.05 | 22% |
| map-int | Marimay native | 0.32 | 1.40 | 7.41–9.81 | 18% |
| map-int | Go native | 0.27 | 0.49 | 5.02–6.23 | 15% |
| map-int | Rust native | 0.34 | 0.94 | 5.78–7.63 | 16% |
| map-int | Marimay VM (source run) | 0.29 | 2.60 | 67.99–77.62 | 4% |
| map-int | Bun / TypeScript | 0.31 | 1.36 | 13.47–20.12 | 10% |
| map-string | Marimay native | 0.63 | 1.91 | 21.34–24.67 | 9% |
| map-string | Go native | 0.26 | 1.42 | 7.38–9.82 | 18% |
| map-string | Rust native | 0.63 | 2.34 | 10.00–12.77 | 22% |
| map-string | Marimay VM (source run) | 1.02 | 7.58 | 84.29–94.41 | 9% |
| map-string | Bun / TypeScript | 0.44 | 2.06 | 15.33–19.43 | 13% |
| map-string-book | Marimay native | 0.62 | 2.28 | 26.12–30.13 | 8% |
| map-string-book | Go native | 0.28 | 0.51 | 6.47–8.27 | 12% |
| map-string-book | Rust native | 0.27 | 1.13 | 10.28–13.35 | 10% |
| map-string-book | Marimay VM (source run) | 1.03 | 8.92 | 86.12–117.90 | 10% |
| map-string-book | Bun / TypeScript | 0.43 | 0.92 | 15.62–17.93 | 8% |
| set-string-churn | Marimay native | 0.60 | 1.79 | 35.22–39.16 | 5% |
| set-string-churn | Go native | 0.33 | 0.74 | 9.59–11.01 | 10% |
| set-string-churn | Rust native | 0.18 | 0.44 | 16.81–18.47 | 3% |
| set-string-churn | Marimay VM (source run) | 0.99 | 3.31 | 99.26–195.41 | 3% |
| set-string-churn | Bun / TypeScript | 0.49 | 1.36 | 18.06–19.76 | 8% |
| catalog-match | Marimay native | 0.19 | 0.38 | 13.24–14.27 | 4% |
| catalog-match | Go native | 0.23 | 0.51 | 4.58–5.30 | 14% |
| catalog-match | Rust native | 0.34 | 0.77 | 3.82–4.88 | 25% |
| catalog-match | Marimay VM (source run) | 0.97 | 2.77 | 123.34–227.71 | 2% |
| catalog-match | Bun / TypeScript | 0.42 | 1.30 | 19.15–22.83 | 7% |
| catalog-dotcall | Marimay native | 0.37 | 0.81 | 8.71–10.87 | 12% |
| catalog-dotcall | Go native | 0.18 | 0.41 | 4.60–6.37 | 11% |
| catalog-dotcall | Rust native | 0.57 | 1.22 | 3.66–5.12 | 39% |
| catalog-dotcall | Marimay VM (source run) | 2.61 | 9.15 | 103.12–131.73 | 9% |
| catalog-dotcall | Bun / TypeScript | 0.45 | 1.12 | 17.78–23.47 | 7% |
| catalog-interface | Marimay native | 0.37 | 0.78 | 12.30–14.11 | 9% |
| catalog-interface | Go native | 0.14 | 0.47 | 4.74–5.95 | 10% |
| catalog-interface | Rust native | 0.49 | 0.85 | 3.76–5.06 | 33% |
| catalog-interface | Marimay VM (source run) | 1.67 | 3.62 | 126.63–143.35 | 4% |
| catalog-interface | Bun / TypeScript | 0.38 | 1.15 | 18.36–24.09 | 6% |
| record-reference-copy | Marimay native | 0.41 | 1.43 | 6.75–14.11 | 20% |
| record-reference-copy | Go native | 0.15 | 0.80 | 4.27–5.90 | 16% |
| record-reference-copy | Rust native | 0.14 | 0.63 | 3.24–4.44 | 18% |
| record-reference-copy | Marimay VM (source run) | 0.56 | 2.97 | 85.60–115.65 | 3% |
| record-reference-copy | Bun / TypeScript | 0.47 | 1.28 | 14.35–17.34 | 9% |
| record-create | Marimay native | 0.49 | 1.32 | 23.81–32.77 | 6% |
| record-create | Go native | 0.40 | 0.81 | 6.93–17.43 | 16% |
| record-create | Rust native | 0.48 | 1.03 | 6.29–8.11 | 21% |
| record-create | Marimay VM (source run) | 1.68 | 4.54 | 115.63–222.79 | 4% |
| record-create | Bun / TypeScript | 0.22 | 2.39 | 16.17–38.67 | 14% |
| array-grow | Marimay native | 0.27 | 0.59 | 7.03–19.19 | 11% |
| array-grow | Go native | 0.24 | 0.86 | 6.55–18.63 | 13% |
| array-grow | Rust native | 0.41 | 0.89 | 3.89–15.17 | 26% |
| array-grow | Marimay VM (source run) | 1.20 | 1.95 | 83.10–175.04 | 4% |
| array-grow | Bun / TypeScript | 0.09 | 0.14 | 14.33–15.81 | 2% |
| array-reserved | Marimay native | 0.27 | 0.83 | 6.83–9.63 | 11% |
| array-reserved | Go native | 0.27 | 0.61 | 5.14–7.26 | 15% |
| array-reserved | Rust native | 0.57 | 1.15 | 3.87–6.53 | 36% |
| array-reserved | Marimay VM (source run) | 0.53 | 4.42 | 83.34–92.79 | 5% |
| array-reserved | Bun / TypeScript | 0.79 | 1.73 | 12.45–16.95 | 18% |
| bfs-int | Marimay native | 0.30 | 0.98 | 7.64–9.85 | 12% |
| bfs-int | Go native | 0.29 | 0.87 | 5.30–7.77 | 16% |
| bfs-int | Rust native | 0.39 | 0.81 | 4.25–6.33 | 23% |
| bfs-int | Marimay VM (source run) | 1.57 | 6.27 | 73.58–86.87 | 8% |
| bfs-int | Bun / TypeScript | 0.80 | 1.60 | 16.98–21.96 | 13% |
| bfs-record | Marimay native | 0.53 | 1.04 | 9.34–11.46 | 16% |
| bfs-record | Go native | 0.22 | 1.39 | 5.72–7.84 | 22% |
| bfs-record | Rust native | 0.23 | 0.86 | 4.61–6.30 | 15% |
| bfs-record | Marimay VM (source run) | 1.21 | 7.34 | 80.31–98.62 | 9% |
| bfs-record | Bun / TypeScript | 0.73 | 2.42 | 17.18–41.75 | 13% |
| catalog-pipeline | Marimay native | 0.36 | 1.18 | 36.51–41.17 | 3% |
| catalog-pipeline | Go native | 0.20 | 0.67 | 8.02–9.50 | 8% |
| catalog-pipeline | Rust native | 0.19 | 0.60 | 9.97–12.48 | 6% |
| catalog-pipeline | Marimay VM (source run) | 1.19 | 4.14 | 208.35–224.24 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.45 | 1.30 | 23.33–28.66 | 6% |
| parse-result | Marimay native | 0.22 | 0.48 | 18.51–20.53 | 4% |
| parse-result | Go native | 0.18 | 0.31 | 5.86–6.65 | 9% |
| parse-result | Rust native | 0.63 | 1.51 | 5.85–7.63 | 29% |
| parse-result | Marimay VM (source run) | 0.65 | 1.55 | 74.37–80.25 | 3% |
| parse-result | Bun / TypeScript | 0.20 | 0.43 | 13.32–15.24 | 4% |
| insertion | Marimay native | 0.85 | 1.67 | 49.65–53.17 | 5% |
| insertion | Go native | 0.22 | 0.53 | 29.56–33.33 | 2% |
| insertion | Rust native | 0.18 | 0.54 | 28.82–29.68 | 2% |
| insertion | Marimay VM (source run) | 0.79 | 1.81 | 210.57–229.38 | 2% |
| insertion | Bun / TypeScript | 0.60 | 1.26 | 81.38–88.97 | 2% |
| heap-sort | Marimay native | 1.20 | 3.76 | 32.93–160.12 | 11% |
| heap-sort | Go native | 0.57 | 3.31 | 39.80–63.39 | 8% |
| heap-sort | Rust native | 0.45 | 1.20 | 22.15–25.27 | 6% |
| heap-sort | Marimay VM (source run) | 1.92 | 7.41 | 247.64–276.15 | 3% |
| heap-sort | Bun / TypeScript | 3.07 | 11.71 | 94.32–125.07 | 12% |
| stack | Marimay native | 0.52 | 1.18 | 9.90–11.60 | 15% |
| stack | Go native | 0.58 | 2.03 | 15.00–18.78 | 13% |
| stack | Rust native | 0.46 | 1.41 | 9.25–11.13 | 14% |
| stack | Marimay VM (source run) | 2.31 | 20.05 | 264.57–401.19 | 7% |
| stack | Bun / TypeScript | 1.14 | 2.95 | 22.18–26.79 | 15% |
| queue | Marimay native | 0.29 | 0.93 | 10.35–12.39 | 9% |
| queue | Go native | 0.41 | 1.19 | 14.91–17.03 | 8% |
| queue | Rust native | 0.31 | 0.87 | 8.92–10.53 | 10% |
| queue | Marimay VM (source run) | 4.69 | 41.05 | 258.95–380.21 | 15% |
| queue | Bun / TypeScript | 1.33 | 2.36 | 26.62–30.04 | 14% |
| min-heap | Marimay native | 2.75 | 6.20 | 48.17–196.19 | 16% |
| min-heap | Go native | 2.14 | 4.08 | 55.26–62.83 | 11% |
| min-heap | Rust native | 0.32 | 3.37 | 30.25–37.16 | 11% |
| min-heap | Marimay VM (source run) | 7.60 | 21.19 | 526.98–595.17 | 4% |
| min-heap | Bun / TypeScript | 1.68 | 14.92 | 110.85–135.95 | 13% |
| dsu | Marimay native | 0.43 | 1.40 | 30.40–33.03 | 5% |
| dsu | Go native | 0.41 | 1.83 | 24.49–28.24 | 7% |
| dsu | Rust native | 0.64 | 2.38 | 21.93–25.55 | 10% |
| dsu | Marimay VM (source run) | 44.35 | 111.73 | 786.73–992.10 | 16% |
| dsu | Bun / TypeScript | 0.58 | 3.14 | 49.35–145.27 | 6% |
| bfs | Marimay native | 3.34 | 6.70 | 67.79–100.29 | 14% |
| bfs | Go native | 1.83 | 4.45 | 61.97–95.43 | 9% |
| bfs | Rust native | 1.14 | 2.24 | 24.17–26.73 | 13% |
| bfs | Marimay VM (source run) | 6.89 | 54.71 | 668.08–809.46 | 8% |
| bfs | Bun / TypeScript | 1.71 | 5.67 | 70.06–98.87 | 8% |
| dijkstra | Marimay native | 1.48 | 3.50 | 42.13–47.83 | 10% |
| dijkstra | Go native | 1.14 | 3.06 | 41.23–45.79 | 8% |
| dijkstra | Rust native | 0.98 | 1.98 | 39.56–43.33 | 7% |
| dijkstra | Marimay VM (source run) | 1.56 | 8.34 | 203.31–223.82 | 4% |
| dijkstra | Bun / TypeScript | 1.80 | 4.59 | 76.26–84.67 | 7% |
| frequency-map | Marimay native | 0.92 | 2.42 | 33.30–36.84 | 8% |
| frequency-map | Go native | 0.15 | 0.44 | 14.40–15.89 | 3% |
| frequency-map | Rust native | 3.82 | 8.49 | 14.69–32.67 | 61% |
| frequency-map | Marimay VM (source run) | 3.48 | 12.89 | 201.72–286.46 | 6% |
| frequency-map | Bun / TypeScript | 1.06 | 2.56 | 28.05–31.79 | 11% |
| unique-set | Marimay native | 0.49 | 2.27 | 27.54–33.51 | 8% |
| unique-set | Go native | 0.40 | 0.91 | 16.36–18.83 | 7% |
| unique-set | Rust native | 0.45 | 1.24 | 19.54–263.15 | 6% |
| unique-set | Marimay VM (source run) | 2.11 | 5.43 | 134.41–151.05 | 5% |
| unique-set | Bun / TypeScript | 0.59 | 1.23 | 22.06–25.27 | 8% |
| rune-frequency | Marimay native | 0.36 | 0.71 | 22.62–24.00 | 5% |
| rune-frequency | Go native | 0.36 | 0.74 | 5.57–7.39 | 18% |
| rune-frequency | Rust native | 0.82 | 1.76 | 5.85–7.83 | 36% |
| rune-frequency | Marimay VM (source run) | 0.46 | 2.64 | 109.93–121.09 | 2% |
| rune-frequency | Bun / TypeScript | 0.44 | 1.64 | 17.11–20.82 | 9% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-string-search-recovery/comprehensive-control.md`
