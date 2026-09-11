# Catalogue and language benchmark

Generated: 2026-09-11T14:28:28+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.53 | 4.55 | 4.51 | 92.21 | 13.16 | 2.10× | 2.11× | — | measured |
| title-search-100k | 100,000 × 128 | 80.80 | 15.51 | 18.52 | 509.25 | 36.36 | 5.21× | 4.36× | — | measured |
| normalize-title | 10,000 × 1 | 57.54 | 8.81 | 11.25 | 141.10 | 17.51 | 6.53× | 5.12× | — | measured |
| word-frequency | 10,000 × 10 | 145.82 | 19.28 | 30.64 | 270.54 | 18.66 | 7.56× | 4.76× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 15.17 | 5.55 | 5.59 | 82.98 | 12.07 | 2.74× | 2.72× | — | measured |
| report-concat | 10,000 × 1 | 98.06 | 81.01 | 48.04 | 162.72 | 12.29 | 1.21× | 2.04× | — | measured |
| map-int | 4,096 × 32 | 7.80 | 5.35 | 6.08 | 68.24 | 13.58 | 1.46× | 1.28× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 23.34 | 7.38 | 10.10 | 88.74 | 15.21 | 3.16× | 2.31× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 31.30 | 6.41 | 10.64 | 91.91 | 15.15 | 4.89× | 2.94× | — | measured |
| set-string-churn | 4,096 × 8 | 47.72 | 10.24 | 17.32 | 111.38 | 17.66 | 4.66× | 2.76× | — | measured |
| catalog-match | 10,000 × 32 | 13.68 | 4.84 | 4.24 | 122.66 | 18.66 | 2.83× | 3.23× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.90 | 4.82 | 3.73 | 103.23 | 18.10 | 1.85× | 2.38× | — | measured |
| catalog-interface | 10,000 × 32 | 12.37 | 4.88 | 4.27 | 126.66 | 18.36 | 2.53× | 2.90× | — | measured |
| record-reference-copy | 1,024 × 128 | 7.11 | 4.62 | 3.28 | 87.98 | 14.45 | 1.54× | 2.16× | — | measured |
| record-create | 1,024 × 128 | 22.88 | 6.96 | 6.40 | 116.06 | 15.73 | 3.29× | 3.58× | — | measured |
| array-grow | 100,000 × 4 | 6.99 | 6.60 | 4.39 | 91.16 | 13.78 | 1.06× | 1.59× | — | measured |
| array-reserved | 100,000 × 4 | 6.93 | 5.32 | 4.31 | 91.16 | 13.02 | 1.30× | 1.61× | — | measured |
| bfs-int | 4,096 × 16 | 7.84 | 5.54 | 4.56 | 77.00 | 16.82 | 1.42× | 1.72× | — | measured |
| bfs-record | 4,096 × 16 | 9.72 | 6.02 | 4.83 | 83.07 | 17.03 | 1.61× | 2.01× | — | measured |
| catalog-pipeline | 10,000 × 16 | 38.15 | 8.12 | 10.59 | 224.26 | 23.30 | 4.70× | 3.60× | — | measured |
| parse-result | 10,000 × 8 | 19.36 | 6.12 | 6.19 | 77.95 | 13.17 | 3.16× | 3.13× | — | measured |
| insertion | fixed anchor | 50.90 | 29.53 | 28.89 | 212.15 | 81.26 | 1.72× | 1.76× | — | measured |
| heap-sort | fixed anchor | 32.77 | 39.69 | 22.27 | 266.79 | 93.78 | 0.83× | 1.47× | ≤ 1.3× | within target |
| stack | fixed anchor | 9.98 | 14.61 | 9.01 | 323.02 | 21.31 | 0.68× | 1.11× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.03 | 14.63 | 9.22 | 323.72 | 25.76 | 0.69× | 1.09× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 47.30 | 54.78 | 30.35 | 585.64 | 111.38 | 0.86× | 1.56× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.40 | 24.61 | 22.09 | 850.93 | 48.78 | 1.23× | 1.38× | — | measured |
| bfs | fixed anchor | 65.87 | 62.66 | 24.18 | 963.72 | 68.59 | 1.05× | 2.72× | — | measured |
| dijkstra | fixed anchor | 42.39 | 41.36 | 39.72 | 211.54 | 76.21 | 1.02× | 1.07× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 33.06 | 14.26 | 19.59 | 222.56 | 27.66 | 2.32× | 1.69× | — | noisy; repeat |
| unique-set | fixed anchor | 27.26 | 16.22 | 19.33 | 156.11 | 21.69 | 1.68× | 1.41× | — | measured |
| rune-frequency | fixed anchor | 22.42 | 6.05 | 6.55 | 119.72 | 16.90 | 3.70× | 3.42× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.38 | 0.76 | 9.07–11.32 | 12% |
| title-search-10k | Go native | 0.28 | 0.73 | 4.11–5.23 | 19% |
| title-search-10k | Rust native | 0.16 | 0.33 | 4.19–5.03 | 11% |
| title-search-10k | Marimay VM (source run) | 0.56 | 0.93 | 90.98–101.23 | 2% |
| title-search-10k | Bun / TypeScript | 0.68 | 1.66 | 12.35–15.14 | 16% |
| title-search-100k | Marimay native | 1.42 | 3.82 | 74.15–151.59 | 5% |
| title-search-100k | Go native | 0.29 | 0.75 | 14.82–16.51 | 6% |
| title-search-100k | Rust native | 0.26 | 0.83 | 18.20–22.13 | 5% |
| title-search-100k | Marimay VM (source run) | 5.05 | 10.89 | 500.32–526.15 | 3% |
| title-search-100k | Bun / TypeScript | 0.44 | 0.85 | 35.77–40.23 | 4% |
| normalize-title | Marimay native | 0.45 | 1.00 | 56.59–61.02 | 2% |
| normalize-title | Go native | 0.24 | 0.89 | 8.19–10.72 | 10% |
| normalize-title | Rust native | 0.23 | 0.56 | 10.95–13.06 | 6% |
| normalize-title | Marimay VM (source run) | 1.06 | 2.36 | 138.75–293.32 | 2% |
| normalize-title | Bun / TypeScript | 0.36 | 0.53 | 16.93–18.30 | 6% |
| word-frequency | Marimay native | 1.18 | 2.53 | 143.50–147.46 | 2% |
| word-frequency | Go native | 0.40 | 0.94 | 18.82–20.06 | 6% |
| word-frequency | Rust native | 0.26 | 0.92 | 30.38–37.64 | 3% |
| word-frequency | Marimay VM (source run) | 0.91 | 3.44 | 269.42–279.25 | 2% |
| word-frequency | Bun / TypeScript | 0.12 | 0.31 | 18.46–20.20 | 2% |
| report-builder | Marimay native | 0.22 | 0.46 | 14.33–18.86 | 4% |
| report-builder | Go native | 0.38 | 0.77 | 4.68–6.35 | 20% |
| report-builder | Rust native | 0.20 | 0.45 | 5.05–7.57 | 11% |
| report-builder | Marimay VM (source run) | 0.96 | 2.36 | 81.59–90.78 | 3% |
| report-builder | Bun / TypeScript | 0.47 | 1.08 | 11.44–13.34 | 12% |
| report-concat | Marimay native | 1.44 | 2.99 | 95.54–120.50 | 4% |
| report-concat | Go native | 0.92 | 1.83 | 79.22–93.62 | 3% |
| report-concat | Rust native | 0.46 | 0.91 | 46.92–54.00 | 3% |
| report-concat | Marimay VM (source run) | 0.82 | 3.24 | 160.20–175.98 | 2% |
| report-concat | Bun / TypeScript | 0.10 | 0.39 | 11.95–13.47 | 3% |
| map-int | Marimay native | 0.15 | 0.32 | 7.35–8.58 | 6% |
| map-int | Go native | 0.19 | 0.32 | 5.05–5.93 | 11% |
| map-int | Rust native | 0.18 | 0.58 | 5.51–16.41 | 10% |
| map-int | Marimay VM (source run) | 0.87 | 4.41 | 67.17–186.97 | 6% |
| map-int | Bun / TypeScript | 0.19 | 0.39 | 12.74–14.51 | 4% |
| map-string | Marimay native | 0.15 | 0.30 | 22.89–23.74 | 2% |
| map-string | Go native | 0.18 | 0.40 | 7.04–7.76 | 7% |
| map-string | Rust native | 0.31 | 0.62 | 9.61–10.80 | 9% |
| map-string | Marimay VM (source run) | 0.45 | 1.06 | 87.71–92.24 | 2% |
| map-string | Bun / TypeScript | 0.31 | 0.74 | 14.52–16.20 | 6% |
| map-string-book | Marimay native | 0.23 | 0.47 | 30.87–32.44 | 2% |
| map-string-book | Go native | 0.20 | 0.37 | 6.18–7.11 | 9% |
| map-string-book | Rust native | 0.19 | 0.35 | 10.35–11.00 | 5% |
| map-string-book | Marimay VM (source run) | 0.49 | 0.98 | 91.30–92.72 | 2% |
| map-string-book | Bun / TypeScript | 0.11 | 0.36 | 14.77–15.75 | 2% |
| set-string-churn | Marimay native | 0.22 | 0.49 | 46.82–51.00 | 2% |
| set-string-churn | Go native | 0.27 | 0.60 | 9.53–10.86 | 8% |
| set-string-churn | Rust native | 0.42 | 1.02 | 16.86–18.08 | 7% |
| set-string-churn | Marimay VM (source run) | 1.04 | 2.60 | 110.18–114.75 | 3% |
| set-string-churn | Bun / TypeScript | 0.29 | 0.61 | 17.31–18.11 | 5% |
| catalog-match | Marimay native | 0.26 | 0.73 | 13.01–14.01 | 6% |
| catalog-match | Go native | 0.15 | 0.27 | 4.61–5.04 | 9% |
| catalog-match | Rust native | 0.31 | 0.73 | 3.67–4.65 | 22% |
| catalog-match | Marimay VM (source run) | 0.55 | 1.04 | 121.98–123.67 | 2% |
| catalog-match | Bun / TypeScript | 0.27 | 0.56 | 18.11–19.68 | 4% |
| catalog-dotcall | Marimay native | 0.16 | 0.35 | 8.51–9.51 | 5% |
| catalog-dotcall | Go native | 0.15 | 0.38 | 4.38–5.00 | 9% |
| catalog-dotcall | Rust native | 0.06 | 0.46 | 3.66–4.44 | 12% |
| catalog-dotcall | Marimay VM (source run) | 0.43 | 1.00 | 101.83–103.88 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.28 | 0.59 | 17.13–19.94 | 5% |
| catalog-interface | Marimay native | 0.15 | 0.38 | 11.82–14.08 | 4% |
| catalog-interface | Go native | 0.18 | 0.38 | 4.41–5.39 | 11% |
| catalog-interface | Rust native | 0.23 | 0.75 | 3.67–4.50 | 18% |
| catalog-interface | Marimay VM (source run) | 0.61 | 2.78 | 126.05–129.89 | 2% |
| catalog-interface | Bun / TypeScript | 0.19 | 0.54 | 17.58–19.76 | 3% |
| record-reference-copy | Marimay native | 0.25 | 0.52 | 6.69–7.85 | 11% |
| record-reference-copy | Go native | 0.14 | 0.34 | 4.26–4.93 | 9% |
| record-reference-copy | Rust native | 0.06 | 0.23 | 3.23–3.93 | 7% |
| record-reference-copy | Marimay VM (source run) | 1.35 | 2.80 | 86.10–92.43 | 5% |
| record-reference-copy | Bun / TypeScript | 0.24 | 0.54 | 13.69–15.17 | 5% |
| record-create | Marimay native | 0.09 | 0.44 | 22.74–23.83 | 2% |
| record-create | Go native | 0.13 | 0.27 | 6.68–7.50 | 6% |
| record-create | Rust native | 0.19 | 0.65 | 6.19–7.27 | 10% |
| record-create | Marimay VM (source run) | 0.18 | 0.56 | 114.74–123.44 | 2% |
| record-create | Bun / TypeScript | 0.06 | 0.28 | 15.24–16.57 | 2% |
| array-grow | Marimay native | 0.03 | 0.18 | 6.75–7.40 | 3% |
| array-grow | Go native | 0.14 | 0.40 | 6.43–7.07 | 7% |
| array-grow | Rust native | 0.13 | 0.34 | 4.16–5.00 | 9% |
| array-grow | Marimay VM (source run) | 0.18 | 0.46 | 90.60–97.31 | 2% |
| array-grow | Bun / TypeScript | 0.14 | 0.32 | 13.29–14.69 | 3% |
| array-reserved | Marimay native | 0.27 | 0.60 | 6.64–7.83 | 12% |
| array-reserved | Go native | 0.18 | 0.37 | 5.06–6.10 | 10% |
| array-reserved | Rust native | 0.18 | 0.54 | 4.08–4.92 | 13% |
| array-reserved | Marimay VM (source run) | 0.61 | 2.35 | 90.44–96.11 | 3% |
| array-reserved | Bun / TypeScript | 0.20 | 0.92 | 12.05–13.36 | 7% |
| bfs-int | Marimay native | 0.26 | 0.52 | 7.26–8.64 | 10% |
| bfs-int | Go native | 0.19 | 0.42 | 5.27–6.06 | 11% |
| bfs-int | Rust native | 0.24 | 0.44 | 4.29–5.13 | 16% |
| bfs-int | Marimay VM (source run) | 0.39 | 0.95 | 76.08–81.71 | 2% |
| bfs-int | Bun / TypeScript | 0.45 | 0.98 | 16.09–18.94 | 8% |
| bfs-record | Marimay native | 0.13 | 0.27 | 9.07–10.09 | 4% |
| bfs-record | Go native | 0.11 | 0.22 | 5.82–6.41 | 5% |
| bfs-record | Rust native | 0.20 | 0.43 | 4.54–5.79 | 13% |
| bfs-record | Marimay VM (source run) | 0.55 | 1.01 | 82.10–89.27 | 2% |
| bfs-record | Bun / TypeScript | 0.28 | 0.83 | 16.57–17.80 | 5% |
| catalog-pipeline | Marimay native | 0.12 | 0.96 | 37.76–40.22 | 3% |
| catalog-pipeline | Go native | 0.16 | 0.36 | 7.88–8.94 | 6% |
| catalog-pipeline | Rust native | 0.30 | 0.73 | 9.91–11.29 | 9% |
| catalog-pipeline | Marimay VM (source run) | 0.98 | 2.71 | 223.05–228.98 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.38 | 0.91 | 22.42–24.07 | 5% |
| parse-result | Marimay native | 0.23 | 0.54 | 19.08–19.75 | 4% |
| parse-result | Go native | 0.09 | 0.16 | 5.79–6.42 | 4% |
| parse-result | Rust native | 0.18 | 0.39 | 5.92–6.49 | 8% |
| parse-result | Marimay VM (source run) | 0.59 | 1.15 | 76.66–87.19 | 2% |
| parse-result | Bun / TypeScript | 0.11 | 0.28 | 12.73–13.49 | 2% |
| insertion | Marimay native | 2.21 | 4.93 | 47.04–55.69 | 13% |
| insertion | Go native | 0.23 | 0.36 | 28.94–30.26 | 2% |
| insertion | Rust native | 0.26 | 0.66 | 28.43–29.42 | 3% |
| insertion | Marimay VM (source run) | 0.34 | 1.14 | 211.31–219.35 | 2% |
| insertion | Bun / TypeScript | 0.36 | 0.93 | 80.14–85.35 | 2% |
| heap-sort | Marimay native | 0.25 | 0.56 | 32.17–33.36 | 2% |
| heap-sort | Go native | 0.32 | 0.57 | 38.80–40.12 | 2% |
| heap-sort | Rust native | 0.28 | 0.64 | 21.89–22.84 | 4% |
| heap-sort | Marimay VM (source run) | 0.59 | 1.49 | 265.50–277.53 | 2% |
| heap-sort | Bun / TypeScript | 0.79 | 1.50 | 92.44–97.38 | 3% |
| stack | Marimay native | 0.08 | 0.22 | 9.62–11.33 | 2% |
| stack | Go native | 0.36 | 0.75 | 14.14–16.69 | 7% |
| stack | Rust native | 0.09 | 0.27 | 8.86–10.22 | 3% |
| stack | Marimay VM (source run) | 2.86 | 8.04 | 319.60–465.67 | 3% |
| stack | Bun / TypeScript | 0.35 | 1.25 | 20.87–23.44 | 6% |
| queue | Marimay native | 0.20 | 0.50 | 9.72–10.27 | 6% |
| queue | Go native | 0.22 | 0.57 | 13.89–15.10 | 4% |
| queue | Rust native | 0.13 | 0.37 | 8.87–9.84 | 4% |
| queue | Marimay VM (source run) | 1.53 | 3.23 | 320.20–326.86 | 2% |
| queue | Bun / TypeScript | 0.53 | 1.08 | 24.99–32.91 | 6% |
| min-heap | Marimay native | 0.45 | 1.17 | 46.54–50.06 | 3% |
| min-heap | Go native | 0.48 | 1.32 | 54.19–56.18 | 3% |
| min-heap | Rust native | 0.37 | 0.72 | 29.41–32.30 | 4% |
| min-heap | Marimay VM (source run) | 2.72 | 7.09 | 579.59–611.87 | 2% |
| min-heap | Bun / TypeScript | 0.86 | 1.76 | 108.94–115.92 | 2% |
| dsu | Marimay native | 0.32 | 0.88 | 29.97–31.50 | 3% |
| dsu | Go native | 0.20 | 0.35 | 24.20–25.43 | 2% |
| dsu | Rust native | 0.32 | 0.72 | 21.50–22.99 | 4% |
| dsu | Marimay VM (source run) | 5.55 | 28.93 | 843.71–908.39 | 3% |
| dsu | Bun / TypeScript | 0.38 | 0.98 | 48.07–50.02 | 2% |
| bfs | Marimay native | 0.29 | 0.95 | 64.80–68.55 | 2% |
| bfs | Go native | 0.70 | 2.15 | 61.28–64.70 | 3% |
| bfs | Rust native | 0.12 | 0.24 | 23.88–24.83 | 2% |
| bfs | Marimay VM (source run) | 5.24 | 10.91 | 956.43–998.02 | 2% |
| bfs | Bun / TypeScript | 0.68 | 1.37 | 67.73–71.53 | 3% |
| dijkstra | Marimay native | 0.29 | 0.94 | 41.53–45.24 | 2% |
| dijkstra | Go native | 0.31 | 0.79 | 40.77–42.38 | 2% |
| dijkstra | Rust native | 0.28 | 0.57 | 39.06–41.04 | 2% |
| dijkstra | Marimay VM (source run) | 0.54 | 1.83 | 210.78–220.04 | 2% |
| dijkstra | Bun / TypeScript | 0.60 | 1.72 | 75.52–81.12 | 2% |
| frequency-map | Marimay native | 0.18 | 1.17 | 32.69–35.07 | 4% |
| frequency-map | Go native | 0.20 | 0.68 | 13.95–14.87 | 5% |
| frequency-map | Rust native | 3.57 | 8.16 | 15.47–25.78 | 55% |
| frequency-map | Marimay VM (source run) | 0.80 | 4.96 | 221.48–261.33 | 2% |
| frequency-map | Bun / TypeScript | 0.23 | 0.92 | 27.24–32.25 | 3% |
| unique-set | Marimay native | 0.26 | 0.58 | 26.93–28.18 | 3% |
| unique-set | Go native | 0.12 | 0.27 | 15.87–16.65 | 2% |
| unique-set | Rust native | 0.40 | 0.81 | 18.84–20.38 | 6% |
| unique-set | Marimay VM (source run) | 0.65 | 4.83 | 153.98–162.61 | 3% |
| unique-set | Bun / TypeScript | 0.37 | 0.79 | 21.05–22.63 | 5% |
| rune-frequency | Marimay native | 0.10 | 0.26 | 22.17–23.11 | 2% |
| rune-frequency | Go native | 0.27 | 0.45 | 5.67–6.58 | 13% |
| rune-frequency | Rust native | 0.26 | 0.85 | 5.92–7.51 | 13% |
| rune-frequency | Marimay VM (source run) | 1.05 | 2.32 | 118.67–124.54 | 3% |
| rune-frequency | Bun / TypeScript | 0.35 | 0.80 | 16.43–18.12 | 6% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-reference-values/comprehensive-current.md`
