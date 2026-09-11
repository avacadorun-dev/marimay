# Catalogue and language benchmark

Generated: 2026-09-11T16:04:17+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.44 | 4.27 | 4.52 | 88.03 | 12.66 | 2.21× | 2.09× | — | measured |
| title-search-100k | 100,000 × 128 | 80.25 | 15.08 | 18.03 | 496.90 | 36.10 | 5.32× | 4.45× | — | measured |
| normalize-title | 10,000 × 1 | 57.30 | 8.69 | 11.03 | 136.85 | 17.28 | 6.59× | 5.19× | — | measured |
| word-frequency | 10,000 × 10 | 144.47 | 18.77 | 30.38 | 266.88 | 18.36 | 7.70× | 4.76× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 14.88 | 5.20 | 5.40 | 78.22 | 11.73 | 2.86× | 2.76× | — | measured |
| report-concat | 10,000 × 1 | 97.09 | 80.30 | 47.59 | 158.91 | 12.06 | 1.21× | 2.04× | — | measured |
| map-int | 4,096 × 32 | 7.52 | 5.16 | 5.97 | 63.88 | 13.18 | 1.46× | 1.26× | ≤ 1.5× | within target |
| map-string | 4,096 × 32 | 23.16 | 7.51 | 9.85 | 84.36 | 15.15 | 3.08× | 2.35× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 31.24 | 6.55 | 10.49 | 89.09 | 14.94 | 4.77× | 2.98× | — | measured |
| set-string-churn | 4,096 × 8 | 47.06 | 9.84 | 17.15 | 108.38 | 17.29 | 4.78× | 2.74× | — | measured |
| catalog-match | 10,000 × 32 | 13.54 | 4.72 | 4.19 | 118.77 | 18.82 | 2.87× | 3.23× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.72 | 4.59 | 3.80 | 99.29 | 17.66 | 1.90× | 2.29× | — | measured |
| catalog-interface | 10,000 × 32 | 12.28 | 4.91 | 4.14 | 123.38 | 18.12 | 2.50× | 2.97× | — | measured |
| record-reference-copy | 1,024 × 128 | 7.04 | 4.40 | 3.34 | 83.24 | 14.02 | 1.60× | 2.11× | — | measured |
| record-create | 1,024 × 128 | 22.81 | 6.79 | 6.34 | 112.03 | 15.53 | 3.36× | 3.60× | — | measured |
| array-grow | 100,000 × 4 | 6.99 | 6.65 | 4.33 | 87.84 | 13.60 | 1.05× | 1.62× | — | measured |
| array-reserved | 100,000 × 4 | 6.92 | 5.27 | 4.09 | 87.32 | 12.55 | 1.31× | 1.69× | — | measured |
| bfs-int | 4,096 × 16 | 7.64 | 5.40 | 4.49 | 73.10 | 16.38 | 1.42× | 1.70× | — | measured |
| bfs-record | 4,096 × 16 | 9.43 | 5.98 | 4.81 | 79.24 | 16.94 | 1.58× | 1.96× | — | measured |
| catalog-pipeline | 10,000 × 16 | 37.94 | 7.91 | 10.39 | 220.70 | 22.68 | 4.80× | 3.65× | — | measured |
| parse-result | 10,000 × 8 | 19.34 | 6.03 | 5.96 | 74.21 | 12.98 | 3.21× | 3.24× | — | measured |
| insertion | fixed anchor | 51.05 | 29.23 | 28.44 | 195.08 | 80.79 | 1.75× | 1.79× | — | measured |
| heap-sort | fixed anchor | 32.63 | 39.15 | 22.35 | 249.58 | 92.78 | 0.83× | 1.46× | ≤ 1.3× | within target |
| stack | fixed anchor | 9.90 | 14.63 | 8.89 | 305.29 | 21.54 | 0.68× | 1.11× | ≤ 1.3× | within target |
| queue | fixed anchor | 9.80 | 14.63 | 9.04 | 304.87 | 25.32 | 0.67× | 1.08× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 47.57 | 53.83 | 29.58 | 563.78 | 109.08 | 0.88× | 1.61× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.01 | 24.53 | 22.07 | 832.43 | 48.10 | 1.22× | 1.36× | — | measured |
| bfs | fixed anchor | 65.93 | 62.57 | 24.06 | 951.54 | 67.84 | 1.05× | 2.74× | — | measured |
| dijkstra | fixed anchor | 41.78 | 41.14 | 39.52 | 198.15 | 75.51 | 1.02× | 1.06× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 32.84 | 13.99 | 23.14 | 204.16 | 27.32 | 2.35× | 1.42× | — | measured |
| unique-set | fixed anchor | 27.14 | 16.08 | 18.97 | 138.34 | 21.74 | 1.69× | 1.43× | — | measured |
| rune-frequency | fixed anchor | 22.39 | 5.89 | 6.50 | 104.07 | 16.59 | 3.80× | 3.44× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.11 | 0.23 | 8.87–9.71 | 3% |
| title-search-10k | Go native | 0.14 | 0.31 | 4.01–5.07 | 10% |
| title-search-10k | Rust native | 0.25 | 0.56 | 3.96–4.88 | 17% |
| title-search-10k | Marimay VM (source run) | 0.35 | 0.78 | 87.40–92.44 | 2% |
| title-search-10k | Bun / TypeScript | 0.18 | 0.49 | 12.31–14.15 | 4% |
| title-search-100k | Marimay native | 3.92 | 8.44 | 73.32–88.61 | 15% |
| title-search-100k | Go native | 0.35 | 0.78 | 14.50–16.27 | 7% |
| title-search-100k | Rust native | 0.21 | 0.46 | 17.61–19.00 | 3% |
| title-search-100k | Marimay VM (source run) | 2.83 | 5.71 | 492.84–504.32 | 2% |
| title-search-100k | Bun / TypeScript | 0.15 | 0.58 | 35.84–36.94 | 2% |
| normalize-title | Marimay native | 0.41 | 1.17 | 56.87–60.22 | 2% |
| normalize-title | Go native | 0.13 | 0.29 | 8.34–9.23 | 4% |
| normalize-title | Rust native | 0.11 | 0.15 | 10.71–11.45 | 3% |
| normalize-title | Marimay VM (source run) | 0.60 | 2.73 | 136.10–139.98 | 2% |
| normalize-title | Bun / TypeScript | 0.09 | 0.24 | 16.79–17.97 | 2% |
| word-frequency | Marimay native | 0.32 | 1.05 | 143.77–156.22 | 2% |
| word-frequency | Go native | 0.13 | 0.42 | 18.31–19.35 | 2% |
| word-frequency | Rust native | 0.18 | 0.46 | 30.02–30.81 | 2% |
| word-frequency | Marimay VM (source run) | 0.75 | 1.99 | 265.59–271.10 | 2% |
| word-frequency | Bun / TypeScript | 0.13 | 0.35 | 18.08–18.67 | 2% |
| report-builder | Marimay native | 0.32 | 0.64 | 14.50–16.23 | 6% |
| report-builder | Go native | 0.17 | 0.64 | 4.82–5.74 | 12% |
| report-builder | Rust native | 0.22 | 0.58 | 4.95–5.71 | 12% |
| report-builder | Marimay VM (source run) | 0.15 | 0.30 | 77.66–78.49 | 2% |
| report-builder | Bun / TypeScript | 0.24 | 0.52 | 11.33–12.64 | 6% |
| report-concat | Marimay native | 0.56 | 1.29 | 95.95–101.30 | 2% |
| report-concat | Go native | 1.08 | 2.18 | 78.74–84.75 | 4% |
| report-concat | Rust native | 0.31 | 0.65 | 46.31–47.92 | 2% |
| report-concat | Marimay VM (source run) | 0.56 | 1.05 | 157.47–161.67 | 2% |
| report-concat | Bun / TypeScript | 0.10 | 0.19 | 11.79–12.37 | 3% |
| map-int | Marimay native | 0.24 | 0.56 | 7.10–7.85 | 9% |
| map-int | Go native | 0.24 | 0.78 | 4.79–5.95 | 15% |
| map-int | Rust native | 0.17 | 0.41 | 5.78–6.25 | 9% |
| map-int | Marimay VM (source run) | 0.19 | 0.40 | 63.65–64.19 | 2% |
| map-int | Bun / TypeScript | 0.34 | 0.71 | 12.41–13.82 | 8% |
| map-string | Marimay native | 0.22 | 0.52 | 22.80–23.61 | 3% |
| map-string | Go native | 0.16 | 0.47 | 7.00–7.67 | 6% |
| map-string | Rust native | 0.12 | 0.39 | 9.60–10.58 | 4% |
| map-string | Marimay VM (source run) | 0.30 | 0.93 | 84.06–85.12 | 2% |
| map-string | Bun / TypeScript | 0.21 | 0.77 | 14.36–15.44 | 5% |
| map-string-book | Marimay native | 0.25 | 0.59 | 30.71–31.90 | 2% |
| map-string-book | Go native | 0.14 | 0.30 | 6.07–7.01 | 6% |
| map-string-book | Rust native | 0.27 | 0.62 | 10.09–10.85 | 8% |
| map-string-book | Marimay VM (source run) | 0.52 | 1.08 | 88.25–100.21 | 2% |
| map-string-book | Bun / TypeScript | 0.06 | 0.56 | 14.55–16.37 | 4% |
| set-string-churn | Marimay native | 0.23 | 0.61 | 46.53–47.89 | 2% |
| set-string-churn | Go native | 0.20 | 0.38 | 9.50–10.30 | 6% |
| set-string-churn | Rust native | 0.23 | 0.53 | 16.57–17.71 | 4% |
| set-string-churn | Marimay VM (source run) | 0.25 | 0.55 | 107.02–110.10 | 2% |
| set-string-churn | Bun / TypeScript | 0.17 | 0.40 | 16.87–17.95 | 3% |
| catalog-match | Marimay native | 0.30 | 0.74 | 12.93–13.88 | 7% |
| catalog-match | Go native | 0.09 | 0.27 | 4.46–4.97 | 6% |
| catalog-match | Rust native | 0.17 | 0.56 | 3.69–4.60 | 13% |
| catalog-match | Marimay VM (source run) | 0.19 | 0.34 | 118.13–120.28 | 2% |
| catalog-match | Bun / TypeScript | 0.16 | 0.67 | 18.09–19.16 | 4% |
| catalog-dotcall | Marimay native | 0.09 | 0.22 | 8.26–9.14 | 3% |
| catalog-dotcall | Go native | 0.07 | 0.15 | 4.24–5.06 | 4% |
| catalog-dotcall | Rust native | 0.20 | 0.48 | 3.52–4.24 | 16% |
| catalog-dotcall | Marimay VM (source run) | 0.22 | 0.50 | 98.79–99.93 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.47 | 0.95 | 17.11–18.21 | 8% |
| catalog-interface | Marimay native | 0.21 | 0.36 | 11.86–12.52 | 5% |
| catalog-interface | Go native | 0.17 | 0.34 | 4.42–5.09 | 10% |
| catalog-interface | Rust native | 0.35 | 0.70 | 3.59–8.20 | 25% |
| catalog-interface | Marimay VM (source run) | 0.25 | 0.78 | 122.67–123.78 | 2% |
| catalog-interface | Bun / TypeScript | 0.15 | 0.29 | 17.70–18.65 | 2% |
| record-reference-copy | Marimay native | 0.23 | 0.55 | 6.59–7.52 | 10% |
| record-reference-copy | Go native | 0.14 | 0.55 | 4.23–4.96 | 13% |
| record-reference-copy | Rust native | 0.08 | 0.19 | 3.10–3.44 | 7% |
| record-reference-copy | Marimay VM (source run) | 0.36 | 0.75 | 82.65–84.32 | 2% |
| record-reference-copy | Bun / TypeScript | 0.28 | 0.69 | 13.69–14.55 | 6% |
| record-create | Marimay native | 0.13 | 0.44 | 22.33–23.50 | 2% |
| record-create | Go native | 0.27 | 0.51 | 6.43–7.38 | 12% |
| record-create | Rust native | 0.14 | 0.62 | 6.12–6.94 | 10% |
| record-create | Marimay VM (source run) | 0.31 | 0.71 | 111.58–114.31 | 2% |
| record-create | Bun / TypeScript | 0.19 | 0.32 | 15.02–15.78 | 4% |
| array-grow | Marimay native | 0.14 | 0.34 | 6.64–7.45 | 6% |
| array-grow | Go native | 0.27 | 0.47 | 6.19–7.10 | 12% |
| array-grow | Rust native | 0.28 | 0.48 | 3.93–4.90 | 19% |
| array-grow | Marimay VM (source run) | 0.74 | 1.71 | 87.06–92.65 | 3% |
| array-grow | Bun / TypeScript | 0.39 | 0.81 | 13.18–14.42 | 9% |
| array-reserved | Marimay native | 0.17 | 0.27 | 6.61–7.23 | 7% |
| array-reserved | Go native | 0.10 | 0.23 | 4.97–5.61 | 6% |
| array-reserved | Rust native | 0.32 | 0.83 | 3.69–4.79 | 24% |
| array-reserved | Marimay VM (source run) | 0.32 | 0.57 | 86.41–92.64 | 2% |
| array-reserved | Bun / TypeScript | 0.14 | 0.56 | 11.89–12.90 | 4% |
| bfs-int | Marimay native | 0.22 | 0.39 | 7.29–8.08 | 9% |
| bfs-int | Go native | 0.11 | 0.24 | 4.97–5.59 | 6% |
| bfs-int | Rust native | 0.16 | 0.33 | 4.23–5.08 | 11% |
| bfs-int | Marimay VM (source run) | 0.22 | 0.49 | 72.65–73.62 | 2% |
| bfs-int | Bun / TypeScript | 0.28 | 0.68 | 16.09–16.86 | 5% |
| bfs-record | Marimay native | 0.24 | 0.49 | 9.09–9.92 | 8% |
| bfs-record | Go native | 0.20 | 0.41 | 5.57–6.22 | 10% |
| bfs-record | Rust native | 0.14 | 0.26 | 4.49–5.33 | 9% |
| bfs-record | Marimay VM (source run) | 0.13 | 0.40 | 78.82–79.78 | 2% |
| bfs-record | Bun / TypeScript | 0.17 | 0.54 | 16.38–17.13 | 3% |
| catalog-pipeline | Marimay native | 0.25 | 0.56 | 37.57–39.37 | 2% |
| catalog-pipeline | Go native | 0.20 | 0.58 | 7.71–8.73 | 7% |
| catalog-pipeline | Rust native | 0.31 | 0.58 | 9.93–24.02 | 9% |
| catalog-pipeline | Marimay VM (source run) | 0.88 | 2.19 | 219.78–222.94 | 2% |
| catalog-pipeline | Bun / TypeScript | 0.37 | 0.74 | 22.20–23.52 | 5% |
| parse-result | Marimay native | 0.08 | 0.28 | 18.78–19.46 | 2% |
| parse-result | Go native | 0.16 | 0.48 | 5.65–6.30 | 8% |
| parse-result | Rust native | 0.22 | 0.51 | 5.74–6.52 | 11% |
| parse-result | Marimay VM (source run) | 0.27 | 0.73 | 73.25–74.85 | 2% |
| parse-result | Bun / TypeScript | 0.15 | 0.39 | 12.49–13.37 | 3% |
| insertion | Marimay native | 1.45 | 3.25 | 46.72–53.34 | 9% |
| insertion | Go native | 0.09 | 0.27 | 28.93–29.58 | 2% |
| insertion | Rust native | 0.23 | 0.56 | 28.21–28.98 | 2% |
| insertion | Marimay VM (source run) | 0.27 | 0.55 | 194.48–208.16 | 2% |
| insertion | Bun / TypeScript | 0.34 | 0.90 | 80.03–82.42 | 2% |
| heap-sort | Marimay native | 0.15 | 0.56 | 32.10–32.88 | 2% |
| heap-sort | Go native | 0.15 | 0.34 | 38.88–39.60 | 2% |
| heap-sort | Rust native | 0.18 | 0.35 | 21.79–22.61 | 2% |
| heap-sort | Marimay VM (source run) | 0.30 | 0.71 | 249.11–250.78 | 2% |
| heap-sort | Bun / TypeScript | 0.37 | 0.76 | 91.95–93.85 | 2% |
| stack | Marimay native | 0.10 | 0.24 | 9.60–10.22 | 3% |
| stack | Go native | 0.21 | 0.56 | 14.05–15.48 | 4% |
| stack | Rust native | 0.04 | 0.14 | 8.77–8.95 | 2% |
| stack | Marimay VM (source run) | 1.52 | 4.59 | 302.58–308.93 | 2% |
| stack | Bun / TypeScript | 0.08 | 0.22 | 20.95–21.73 | 2% |
| queue | Marimay native | 0.07 | 0.23 | 9.50–10.10 | 2% |
| queue | Go native | 0.33 | 0.71 | 14.19–15.32 | 7% |
| queue | Rust native | 0.13 | 0.26 | 8.85–9.29 | 4% |
| queue | Marimay VM (source run) | 0.43 | 1.02 | 303.95–307.95 | 2% |
| queue | Bun / TypeScript | 0.12 | 0.54 | 24.88–25.82 | 2% |
| min-heap | Marimay native | 0.65 | 1.39 | 46.24–49.68 | 4% |
| min-heap | Go native | 0.26 | 0.59 | 53.12–54.64 | 2% |
| min-heap | Rust native | 0.22 | 0.48 | 29.04–30.09 | 2% |
| min-heap | Marimay VM (source run) | 1.74 | 4.71 | 561.49–569.56 | 2% |
| min-heap | Bun / TypeScript | 0.54 | 1.24 | 108.17–113.75 | 2% |
| dsu | Marimay native | 0.22 | 0.46 | 29.77–30.43 | 2% |
| dsu | Go native | 0.07 | 0.21 | 24.04–24.89 | 2% |
| dsu | Rust native | 0.26 | 0.63 | 21.58–22.48 | 4% |
| dsu | Marimay VM (source run) | 2.29 | 5.20 | 826.48–851.33 | 2% |
| dsu | Bun / TypeScript | 0.21 | 0.75 | 47.39–48.73 | 2% |
| bfs | Marimay native | 0.15 | 0.34 | 65.49–67.91 | 2% |
| bfs | Go native | 1.16 | 2.44 | 61.37–204.78 | 6% |
| bfs | Rust native | 0.14 | 0.34 | 23.47–50.14 | 2% |
| bfs | Marimay VM (source run) | 4.27 | 13.95 | 942.46–980.57 | 2% |
| bfs | Bun / TypeScript | 0.27 | 0.96 | 67.34–70.34 | 2% |
| dijkstra | Marimay native | 0.23 | 0.51 | 41.47–43.65 | 2% |
| dijkstra | Go native | 0.15 | 0.50 | 40.80–43.61 | 2% |
| dijkstra | Rust native | 0.22 | 0.52 | 39.10–41.76 | 2% |
| dijkstra | Marimay VM (source run) | 0.12 | 0.35 | 197.51–203.74 | 2% |
| dijkstra | Bun / TypeScript | 0.21 | 0.47 | 75.01–78.03 | 2% |
| frequency-map | Marimay native | 0.28 | 0.63 | 32.21–34.58 | 3% |
| frequency-map | Go native | 0.09 | 0.22 | 13.72–15.37 | 2% |
| frequency-map | Rust native | 1.42 | 5.86 | 14.02–25.39 | 25% |
| frequency-map | Marimay VM (source run) | 0.61 | 1.53 | 202.27–211.76 | 2% |
| frequency-map | Bun / TypeScript | 0.19 | 0.63 | 27.00–29.08 | 2% |
| unique-set | Marimay native | 0.10 | 0.20 | 26.70–28.47 | 2% |
| unique-set | Go native | 0.10 | 0.34 | 15.61–17.02 | 2% |
| unique-set | Rust native | 0.11 | 0.57 | 18.65–20.37 | 3% |
| unique-set | Marimay VM (source run) | 0.52 | 1.08 | 137.50–144.08 | 2% |
| unique-set | Bun / TypeScript | 0.28 | 0.59 | 20.95–23.82 | 4% |
| rune-frequency | Marimay native | 0.20 | 0.34 | 22.12–24.35 | 3% |
| rune-frequency | Go native | 0.15 | 0.45 | 5.55–6.58 | 8% |
| rune-frequency | Rust native | 0.48 | 1.28 | 5.86–7.62 | 22% |
| rune-frequency | Marimay VM (source run) | 0.59 | 1.37 | 102.79–106.50 | 2% |
| rune-frequency | Bun / TypeScript | 0.08 | 0.25 | 16.20–18.04 | 2% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/vm-reachable-preparation-v2/comprehensive-current.md`
