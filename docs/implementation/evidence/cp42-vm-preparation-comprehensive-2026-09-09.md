# Catalogue and language benchmark

Generated: 2026-09-09T20:54:10+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.87 | 4.57 | 4.68 | 152.05 | 13.94 | 2.16× | 2.11× | — | measured |
| title-search-100k | 100,000 × 128 | 83.16 | 15.57 | 19.66 | 923.55 | 38.01 | 5.34× | 4.23× | — | measured |
| normalize-title | 10,000 × 1 | 59.56 | 9.05 | 11.30 | 226.18 | 18.13 | 6.58× | 5.27× | — | measured |
| word-frequency | 10,000 × 10 | 150.18 | 19.56 | 31.35 | 413.90 | 19.31 | 7.68× | 4.79× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 15.55 | 5.26 | 5.53 | 156.55 | 12.87 | 2.96× | 2.81× | — | measured |
| report-concat | 10,000 × 1 | 104.22 | 80.76 | 49.21 | 240.39 | 12.87 | 1.29× | 2.12× | — | measured |
| map-int | 4,096 × 32 | 7.85 | 5.61 | 6.22 | 108.52 | 14.02 | 1.40× | 1.26× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 23.85 | 7.68 | 10.31 | 138.45 | 16.02 | 3.10× | 2.31× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 32.69 | 6.62 | 10.91 | 150.63 | 16.38 | 4.94× | 3.00× | — | measured |
| set-string-churn | 4,096 × 8 | 50.12 | 10.30 | 18.07 | 158.60 | 19.20 | 4.86× | 2.77× | — | measured |
| catalog-match | 10,000 × 32 | 14.21 | 4.76 | 4.51 | 335.42 | 19.47 | 2.99× | 3.15× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.94 | 4.83 | 4.11 | 242.00 | 18.55 | 1.85× | 2.18× | — | measured |
| catalog-interface | 10,000 × 32 | 12.80 | 4.87 | 4.30 | 350.18 | 19.37 | 2.63× | 2.97× | — | measured |
| record-reference-copy | 1,024 × 128 | 7.17 | 4.51 | 3.43 | 198.11 | 15.04 | 1.59× | 2.09× | — | measured |
| record-create | 1,024 × 128 | 24.32 | 7.17 | 6.75 | 237.52 | 16.85 | 3.39× | 3.60× | — | measured |
| array-grow | 100,000 × 4 | 7.13 | 6.60 | 4.41 | 209.96 | 14.42 | 1.08× | 1.62× | — | measured |
| array-reserved | 100,000 × 4 | 7.15 | 5.33 | 4.49 | 209.96 | 13.13 | 1.34× | 1.59× | — | measured |
| bfs-int | 4,096 × 16 | 7.93 | 5.41 | 4.84 | 224.40 | 17.54 | 1.47× | 1.64× | — | measured |
| bfs-record | 4,096 × 16 | 9.87 | 6.19 | 5.23 | 243.26 | 17.91 | 1.60× | 1.89× | — | measured |
| catalog-pipeline | 10,000 × 16 | 39.09 | 8.20 | 10.38 | 577.70 | 24.22 | 4.76× | 3.77× | — | measured |
| parse-result | 10,000 × 8 | 20.48 | 6.21 | 6.23 | 131.77 | 13.98 | 3.30× | 3.29× | — | measured |
| insertion | fixed anchor | 52.91 | 30.18 | 29.65 | 980.82 | 84.33 | 1.75× | 1.78× | — | measured |
| heap-sort | fixed anchor | 33.66 | 40.57 | 23.27 | 1709.04 | 98.96 | 0.83× | 1.45× | ≤ 1.3× | within target |
| stack | fixed anchor | 10.39 | 14.69 | 9.41 | 940.84 | 22.85 | 0.71× | 1.10× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.73 | 15.02 | 9.45 | 1051.04 | 26.94 | 0.71× | 1.14× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 49.15 | 56.13 | 31.46 | 5773.97 | 115.43 | 0.88× | 1.56× | ≤ 1.3× | within target |
| dsu | fixed anchor | 31.18 | 25.42 | 22.73 | 3907.95 | 50.29 | 1.23× | 1.37× | — | measured |
| bfs | fixed anchor | 70.16 | 60.70 | 24.66 | 5721.49 | 71.75 | 1.16× | 2.85× | — | measured |
| dijkstra | fixed anchor | 42.91 | 42.59 | 40.73 | 2488.57 | 78.38 | 1.01× | 1.05× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 34.52 | 14.93 | 23.08 | 491.59 | 29.01 | 2.31× | 1.50× | — | noisy; repeat |
| unique-set | fixed anchor | 28.33 | 16.93 | 20.67 | 362.08 | 22.57 | 1.67× | 1.37× | — | measured |
| rune-frequency | fixed anchor | 23.33 | 6.10 | 6.51 | 255.82 | 17.70 | 3.82× | 3.58× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.32 | 0.59 | 9.38–11.11 | 10% |
| title-search-10k | Go native | 0.21 | 0.43 | 4.16–5.26 | 14% |
| title-search-10k | Rust native | 0.44 | 0.79 | 4.20–7.64 | 28% |
| title-search-10k | Marimay VM (source run) | 0.98 | 6.89 | 148.34–304.22 | 5% |
| title-search-10k | Bun / TypeScript | 0.23 | 0.73 | 13.15–14.49 | 5% |
| title-search-100k | Marimay native | 2.29 | 7.60 | 76.37–155.40 | 9% |
| title-search-100k | Go native | 0.46 | 0.79 | 14.79–61.99 | 9% |
| title-search-100k | Rust native | 0.56 | 1.09 | 17.88–41.97 | 9% |
| title-search-100k | Marimay VM (source run) | 5.59 | 14.30 | 912.11–955.03 | 2% |
| title-search-100k | Bun / TypeScript | 0.73 | 1.43 | 36.83–40.18 | 6% |
| normalize-title | Marimay native | 0.50 | 1.70 | 57.62–62.43 | 3% |
| normalize-title | Go native | 0.26 | 0.53 | 8.46–10.31 | 9% |
| normalize-title | Rust native | 0.33 | 0.71 | 10.84–12.15 | 9% |
| normalize-title | Marimay VM (source run) | 1.64 | 4.05 | 219.55–229.59 | 2% |
| normalize-title | Bun / TypeScript | 0.45 | 1.11 | 17.13–18.75 | 7% |
| word-frequency | Marimay native | 1.30 | 3.04 | 146.82–158.79 | 3% |
| word-frequency | Go native | 0.37 | 0.77 | 18.52–20.26 | 6% |
| word-frequency | Rust native | 0.47 | 0.99 | 30.69–32.17 | 5% |
| word-frequency | Marimay VM (source run) | 2.25 | 5.07 | 409.73–425.21 | 2% |
| word-frequency | Bun / TypeScript | 0.05 | 0.33 | 18.53–19.82 | 2% |
| report-builder | Marimay native | 0.22 | 0.59 | 14.85–16.10 | 4% |
| report-builder | Go native | 0.18 | 0.46 | 4.78–5.44 | 11% |
| report-builder | Rust native | 0.29 | 0.65 | 5.05–5.97 | 15% |
| report-builder | Marimay VM (source run) | 0.70 | 3.04 | 152.12–157.59 | 2% |
| report-builder | Bun / TypeScript | 0.23 | 0.63 | 11.80–16.34 | 5% |
| report-concat | Marimay native | 1.06 | 4.48 | 97.39–107.74 | 4% |
| report-concat | Go native | 1.93 | 4.11 | 77.03–86.58 | 7% |
| report-concat | Rust native | 1.23 | 3.09 | 47.66–52.35 | 7% |
| report-concat | Marimay VM (source run) | 2.80 | 8.48 | 229.44–247.40 | 4% |
| report-concat | Bun / TypeScript | 0.14 | 0.40 | 11.95–14.04 | 3% |
| map-int | Marimay native | 0.24 | 0.55 | 7.15–8.13 | 9% |
| map-int | Go native | 0.42 | 0.66 | 4.92–6.39 | 22% |
| map-int | Rust native | 0.31 | 0.61 | 5.69–6.99 | 15% |
| map-int | Marimay VM (source run) | 1.63 | 4.23 | 104.96–124.91 | 5% |
| map-int | Bun / TypeScript | 0.08 | 0.26 | 13.75–14.51 | 2% |
| map-string | Marimay native | 0.74 | 1.38 | 22.87–25.32 | 9% |
| map-string | Go native | 0.13 | 0.37 | 7.49–8.29 | 5% |
| map-string | Rust native | 0.19 | 0.40 | 9.79–10.76 | 5% |
| map-string | Marimay VM (source run) | 1.05 | 4.06 | 133.72–144.08 | 3% |
| map-string | Bun / TypeScript | 0.63 | 1.12 | 15.03–17.60 | 12% |
| map-string-book | Marimay native | 0.22 | 0.70 | 31.52–33.73 | 2% |
| map-string-book | Go native | 0.10 | 0.39 | 6.35–7.52 | 6% |
| map-string-book | Rust native | 0.23 | 0.48 | 10.23–11.60 | 6% |
| map-string-book | Marimay VM (source run) | 0.73 | 2.19 | 145.60–152.92 | 2% |
| map-string-book | Bun / TypeScript | 0.28 | 0.65 | 15.82–18.35 | 5% |
| set-string-churn | Marimay native | 0.32 | 1.09 | 48.53–51.46 | 2% |
| set-string-churn | Go native | 0.16 | 0.45 | 9.59–11.08 | 5% |
| set-string-churn | Rust native | 0.39 | 0.77 | 17.35–19.57 | 6% |
| set-string-churn | Marimay VM (source run) | 0.86 | 1.76 | 153.43–160.28 | 2% |
| set-string-churn | Bun / TypeScript | 0.22 | 0.74 | 18.19–19.48 | 4% |
| catalog-match | Marimay native | 0.43 | 0.73 | 13.28–15.34 | 9% |
| catalog-match | Go native | 0.12 | 0.45 | 4.61–5.17 | 10% |
| catalog-match | Rust native | 0.22 | 0.63 | 3.90–5.00 | 15% |
| catalog-match | Marimay VM (source run) | 2.93 | 6.85 | 330.06–343.86 | 3% |
| catalog-match | Bun / TypeScript | 0.75 | 1.19 | 18.45–20.66 | 12% |
| catalog-dotcall | Marimay native | 0.19 | 0.39 | 8.66–9.34 | 6% |
| catalog-dotcall | Go native | 0.09 | 0.22 | 4.53–5.14 | 6% |
| catalog-dotcall | Rust native | 0.27 | 0.60 | 3.71–4.65 | 20% |
| catalog-dotcall | Marimay VM (source run) | 1.87 | 3.15 | 238.02–269.53 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.42 | 1.00 | 17.56–19.57 | 7% |
| catalog-interface | Marimay native | 0.19 | 0.37 | 12.24–13.23 | 4% |
| catalog-interface | Go native | 0.10 | 0.22 | 4.50–5.16 | 6% |
| catalog-interface | Rust native | 0.28 | 0.61 | 3.84–4.86 | 20% |
| catalog-interface | Marimay VM (source run) | 2.89 | 6.26 | 345.35–365.10 | 2% |
| catalog-interface | Bun / TypeScript | 0.55 | 1.14 | 18.31–54.33 | 8% |
| record-reference-copy | Marimay native | 0.17 | 0.37 | 6.66–7.97 | 7% |
| record-reference-copy | Go native | 0.19 | 0.49 | 4.20–5.67 | 12% |
| record-reference-copy | Rust native | 0.11 | 0.18 | 3.20–4.13 | 9% |
| record-reference-copy | Marimay VM (source run) | 1.09 | 4.41 | 196.44–260.32 | 2% |
| record-reference-copy | Bun / TypeScript | 0.45 | 1.16 | 14.40–27.00 | 9% |
| record-create | Marimay native | 0.32 | 0.76 | 23.49–27.39 | 4% |
| record-create | Go native | 0.27 | 0.51 | 6.76–18.79 | 11% |
| record-create | Rust native | 0.24 | 0.52 | 6.18–12.77 | 11% |
| record-create | Marimay VM (source run) | 1.96 | 3.05 | 229.51–250.56 | 2% |
| record-create | Bun / TypeScript | 0.36 | 0.82 | 15.27–17.98 | 6% |
| array-grow | Marimay native | 0.09 | 0.27 | 6.89–7.50 | 4% |
| array-grow | Go native | 0.17 | 0.35 | 6.28–7.21 | 8% |
| array-grow | Rust native | 0.22 | 0.47 | 4.06–5.10 | 15% |
| array-grow | Marimay VM (source run) | 0.64 | 2.06 | 206.16–212.92 | 2% |
| array-grow | Bun / TypeScript | 0.11 | 0.46 | 13.94–16.46 | 3% |
| array-reserved | Marimay native | 0.17 | 0.44 | 6.70–7.42 | 7% |
| array-reserved | Go native | 0.12 | 0.29 | 5.12–5.84 | 7% |
| array-reserved | Rust native | 0.42 | 0.78 | 3.95–5.17 | 28% |
| array-reserved | Marimay VM (source run) | 1.64 | 7.13 | 203.32–213.66 | 3% |
| array-reserved | Bun / TypeScript | 0.38 | 0.55 | 12.65–13.81 | 9% |
| bfs-int | Marimay native | 0.27 | 0.55 | 7.56–8.41 | 10% |
| bfs-int | Go native | 0.08 | 0.18 | 5.19–6.26 | 4% |
| bfs-int | Rust native | 0.25 | 0.63 | 4.30–5.12 | 16% |
| bfs-int | Marimay VM (source run) | 1.59 | 4.72 | 220.68–230.43 | 2% |
| bfs-int | Bun / TypeScript | 0.45 | 1.40 | 16.45–18.91 | 8% |
| bfs-record | Marimay native | 0.39 | 1.10 | 9.08–29.60 | 12% |
| bfs-record | Go native | 0.27 | 0.60 | 5.76–10.23 | 13% |
| bfs-record | Rust native | 0.36 | 0.74 | 4.47–7.11 | 20% |
| bfs-record | Marimay VM (source run) | 1.30 | 3.16 | 239.27–277.98 | 2% |
| bfs-record | Bun / TypeScript | 0.32 | 0.66 | 16.65–42.86 | 5% |
| catalog-pipeline | Marimay native | 0.41 | 1.04 | 38.08–39.73 | 3% |
| catalog-pipeline | Go native | 0.20 | 0.62 | 7.67–8.85 | 8% |
| catalog-pipeline | Rust native | 0.24 | 0.68 | 10.04–16.27 | 7% |
| catalog-pipeline | Marimay VM (source run) | 3.87 | 8.23 | 569.61–603.33 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.25 | 0.68 | 22.94–29.34 | 3% |
| parse-result | Marimay native | 0.26 | 0.59 | 19.59–21.18 | 4% |
| parse-result | Go native | 0.17 | 0.35 | 5.94–7.06 | 8% |
| parse-result | Rust native | 0.17 | 0.71 | 5.86–7.48 | 11% |
| parse-result | Marimay VM (source run) | 0.83 | 2.46 | 126.88–134.08 | 2% |
| parse-result | Bun / TypeScript | 0.14 | 0.41 | 13.78–15.10 | 3% |
| insertion | Marimay native | 1.19 | 3.01 | 50.00–55.24 | 7% |
| insertion | Go native | 0.33 | 0.78 | 29.15–30.87 | 3% |
| insertion | Rust native | 0.08 | 0.68 | 28.38–30.62 | 2% |
| insertion | Marimay VM (source run) | 1.72 | 4.28 | 973.75–988.51 | 2% |
| insertion | Bun / TypeScript | 0.47 | 2.16 | 81.02–85.07 | 3% |
| heap-sort | Marimay native | 0.73 | 1.48 | 32.65–34.88 | 7% |
| heap-sort | Go native | 0.51 | 1.61 | 39.05–43.39 | 4% |
| heap-sort | Rust native | 0.48 | 1.02 | 22.29–25.10 | 6% |
| heap-sort | Marimay VM (source run) | 8.07 | 16.23 | 1693.40–1821.82 | 2% |
| heap-sort | Bun / TypeScript | 1.00 | 4.82 | 93.78–100.72 | 5% |
| stack | Marimay native | 0.19 | 0.40 | 9.71–12.44 | 6% |
| stack | Go native | 0.21 | 0.70 | 14.26–15.49 | 5% |
| stack | Rust native | 0.14 | 0.37 | 8.99–9.76 | 4% |
| stack | Marimay VM (source run) | 7.49 | 15.36 | 925.10–959.38 | 2% |
| stack | Bun / TypeScript | 0.39 | 0.85 | 22.19–23.40 | 5% |
| queue | Marimay native | 0.28 | 0.86 | 9.80–11.18 | 8% |
| queue | Go native | 0.17 | 0.40 | 14.56–15.43 | 3% |
| queue | Rust native | 0.17 | 0.41 | 9.13–9.86 | 5% |
| queue | Marimay VM (source run) | 9.03 | 17.17 | 1034.85–1092.57 | 3% |
| queue | Bun / TypeScript | 0.37 | 0.83 | 25.80–28.29 | 4% |
| min-heap | Marimay native | 0.85 | 2.33 | 46.98–50.09 | 5% |
| min-heap | Go native | 0.90 | 1.83 | 54.03–57.95 | 5% |
| min-heap | Rust native | 0.78 | 1.64 | 30.03–33.00 | 7% |
| min-heap | Marimay VM (source run) | 25.98 | 80.86 | 5704.27–5885.10 | 2% |
| min-heap | Bun / TypeScript | 2.29 | 5.33 | 109.78–126.71 | 6% |
| dsu | Marimay native | 0.38 | 1.57 | 29.97–33.73 | 5% |
| dsu | Go native | 0.17 | 0.41 | 24.82–25.60 | 2% |
| dsu | Rust native | 0.21 | 0.64 | 21.53–23.44 | 3% |
| dsu | Marimay VM (source run) | 23.56 | 78.87 | 3865.84–4025.99 | 2% |
| dsu | Bun / TypeScript | 0.22 | 0.70 | 47.86–50.78 | 2% |
| bfs | Marimay native | 0.35 | 1.43 | 68.01–71.34 | 2% |
| bfs | Go native | 1.40 | 3.03 | 58.62–62.67 | 7% |
| bfs | Rust native | 0.23 | 0.57 | 23.78–25.14 | 3% |
| bfs | Marimay VM (source run) | 41.41 | 113.33 | 5676.29–6051.66 | 2% |
| bfs | Bun / TypeScript | 0.48 | 1.46 | 70.41–74.10 | 2% |
| dijkstra | Marimay native | 0.46 | 0.87 | 42.18–44.82 | 3% |
| dijkstra | Go native | 0.40 | 0.83 | 41.34–43.86 | 3% |
| dijkstra | Rust native | 0.89 | 1.85 | 39.32–42.68 | 7% |
| dijkstra | Marimay VM (source run) | 5.14 | 11.69 | 2480.81–2514.67 | 2% |
| dijkstra | Bun / TypeScript | 0.55 | 1.34 | 76.96–81.10 | 2% |
| frequency-map | Marimay native | 0.56 | 0.79 | 33.00–37.90 | 5% |
| frequency-map | Go native | 0.43 | 0.75 | 14.23–79.25 | 9% |
| frequency-map | Rust native | 2.61 | 9.99 | 14.20–53.68 | 43% |
| frequency-map | Marimay VM (source run) | 3.35 | 7.68 | 484.91–527.03 | 2% |
| frequency-map | Bun / TypeScript | 0.47 | 0.95 | 28.32–30.36 | 5% |
| unique-set | Marimay native | 0.44 | 0.83 | 27.40–29.47 | 5% |
| unique-set | Go native | 0.25 | 0.67 | 16.43–17.33 | 4% |
| unique-set | Rust native | 0.53 | 1.17 | 19.73–21.58 | 8% |
| unique-set | Marimay VM (source run) | 3.89 | 8.11 | 355.38–376.92 | 3% |
| unique-set | Bun / TypeScript | 0.33 | 0.72 | 21.85–23.12 | 4% |
| rune-frequency | Marimay native | 0.28 | 0.62 | 22.64–24.26 | 4% |
| rune-frequency | Go native | 0.28 | 0.76 | 5.80–7.40 | 14% |
| rune-frequency | Rust native | 0.35 | 0.68 | 6.03–7.37 | 16% |
| rune-frequency | Marimay VM (source run) | 1.36 | 2.25 | 251.13–266.70 | 2% |
| rune-frequency | Bun / TypeScript | 0.48 | 1.01 | 16.83–20.31 | 8% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --samples 11 --warmups 3 --output /workspace/Projects/Marimay/marimay-vm-parity/build/vm-preparation-comprehensive.md`
