# Catalogue and language benchmark

Generated: 2026-09-10T23:30:09+07:00; profile: **standard**; scope: **local-only**; status: **passed**.

Host: Darwin arm64; Apple M2.

Samples: **11** after **3** process warmups; timeout: 300 s; Marimay managed heap: 67108864 bytes.

Native columns measure release executables directly, including input generation, checksum, process startup and teardown. Compilation is excluded. VM uses `mmay run --engine vm`: source loading, checking and lowering are included. VM timings are not used for Go/Rust ratios.

Local host isolation is not independently verified. Raw samples, order, commands, sources and executable SHA-256, runtime and toolchain metadata are in the companion JSON.

Bun runs TypeScript source with JavaScriptCore JIT and default GC. Each observation starts a fresh process; TypeScript loading/transpilation and JIT work are included. Process warmups do not retain a warmed JIT. Bun is not a native AOT column. See the representation differences in the catalogue methodology.

| Workload | Size × rounds | Marimay native ms | Go native ms | Rust native ms | Marimay VM (source run) ms | Bun / TypeScript ms | native / Go | native / Rust | Go target | Assessment |
|---|---|---|---|---|---|---|---|---|---|---|
| title-search-10k | 10,000 × 128 | 9.63 | 4.48 | 4.65 | 117.88 | 13.63 | 2.15× | 2.07× | — | measured |
| title-search-100k | 100,000 × 128 | 77.83 | 15.51 | 18.56 | 617.70 | 37.10 | 5.02× | 4.19× | — | measured |
| normalize-title | 10,000 × 1 | 58.29 | 9.09 | 11.30 | 186.67 | 17.76 | 6.42× | 5.16× | — | measured |
| word-frequency | 10,000 × 10 | 147.01 | 19.49 | 31.00 | 339.62 | 19.13 | 7.54× | 4.74× | ≤ 2.0× | above target |
| report-builder | 10,000 × 1 | 15.51 | 5.47 | 5.61 | 117.40 | 12.83 | 2.84× | 2.77× | — | measured |
| report-concat | 10,000 × 1 | 107.86 | 84.38 | 49.98 | 206.73 | 13.12 | 1.28× | 2.16× | — | measured |
| map-int | 4,096 × 32 | 8.20 | 5.44 | 6.48 | 91.96 | 15.52 | 1.51× | 1.26× | ≤ 1.5× | above target |
| map-string | 4,096 × 32 | 24.61 | 7.63 | 10.70 | 117.16 | 16.50 | 3.22× | 2.30× | ≤ 2.0× | above target |
| map-string-book | 10,000 × 4 | 31.95 | 6.52 | 10.57 | 128.75 | 16.38 | 4.90× | 3.02× | — | measured |
| set-string-churn | 4,096 × 8 | 48.76 | 10.25 | 17.75 | 147.03 | 18.34 | 4.76× | 2.75× | — | measured |
| catalog-match | 10,000 × 32 | 13.69 | 4.96 | 4.33 | 195.62 | 19.59 | 2.76× | 3.16× | ≤ 1.5× | above target |
| catalog-dotcall | 10,000 × 32 | 8.88 | 4.69 | 3.82 | 157.56 | 18.64 | 1.90× | 2.33× | — | measured |
| catalog-interface | 10,000 × 32 | 12.43 | 5.05 | 4.29 | 208.63 | 19.11 | 2.46× | 2.90× | — | measured |
| record-reference-copy | 1,024 × 128 | 7.38 | 4.56 | 3.43 | 124.03 | 14.71 | 1.62× | 2.15× | — | measured |
| record-create | 1,024 × 128 | 23.76 | 7.17 | 6.92 | 162.64 | 16.87 | 3.31× | 3.44× | — | measured |
| array-grow | 100,000 × 4 | 7.31 | 6.87 | 4.65 | 129.04 | 14.97 | 1.06× | 1.57× | — | measured |
| array-reserved | 100,000 × 4 | 7.28 | 5.34 | 4.43 | 129.31 | 13.55 | 1.36× | 1.64× | — | measured |
| bfs-int | 4,096 × 16 | 8.27 | 5.57 | 4.94 | 107.75 | 17.54 | 1.48× | 1.67× | — | measured |
| bfs-record | 4,096 × 16 | 9.78 | 6.29 | 5.17 | 114.60 | 17.98 | 1.55× | 1.89× | — | measured |
| catalog-pipeline | 10,000 × 16 | 38.67 | 8.27 | 10.96 | 293.84 | 23.65 | 4.68× | 3.53× | — | measured |
| parse-result | 10,000 × 8 | 20.09 | 6.24 | 6.08 | 110.90 | 14.18 | 3.22× | 3.31× | — | measured |
| insertion | fixed anchor | 50.24 | 29.94 | 29.24 | 227.00 | 82.80 | 1.68× | 1.72× | — | measured |
| heap-sort | fixed anchor | 33.51 | 40.41 | 22.87 | 335.50 | 97.61 | 0.83× | 1.47× | ≤ 1.3× | within target |
| stack | fixed anchor | 10.43 | 15.39 | 9.37 | 558.64 | 22.92 | 0.68× | 1.11× | ≤ 1.3× | within target |
| queue | fixed anchor | 10.31 | 15.26 | 9.50 | 583.07 | 27.09 | 0.68× | 1.09× | ≤ 1.3× | within target |
| min-heap | fixed anchor | 48.77 | 55.06 | 30.58 | 939.53 | 111.56 | 0.89× | 1.59× | ≤ 1.3× | within target |
| dsu | fixed anchor | 30.61 | 25.06 | 22.50 | 1419.29 | 49.16 | 1.22× | 1.36× | — | measured |
| bfs | fixed anchor | 69.36 | 63.72 | 24.46 | 1478.22 | 70.90 | 1.09× | 2.84× | — | measured |
| dijkstra | fixed anchor | 42.85 | 41.85 | 39.84 | 231.43 | 76.89 | 1.02× | 1.08× | ≤ 1.3× | within target |
| frequency-map | fixed anchor | 33.58 | 14.41 | 19.05 | 301.89 | 29.62 | 2.33× | 1.76× | — | noisy; repeat |
| unique-set | fixed anchor | 27.67 | 16.47 | 19.88 | 212.05 | 22.31 | 1.68× | 1.39× | — | measured |
| rune-frequency | fixed anchor | 22.60 | 6.27 | 6.56 | 162.12 | 17.16 | 3.60× | 3.45× | — | measured |

## Dispersion

| Workload | Engine | MAD ms | IQR ms | Min–max ms | Noise |
|---|---|---:|---:|---:|---:|
| title-search-10k | Marimay native | 0.15 | 0.58 | 9.15–10.50 | 6% |
| title-search-10k | Go native | 0.33 | 0.69 | 4.06–6.10 | 22% |
| title-search-10k | Rust native | 0.27 | 0.69 | 4.21–6.13 | 18% |
| title-search-10k | Marimay VM (source run) | 1.41 | 5.08 | 116.22–131.23 | 4% |
| title-search-10k | Bun / TypeScript | 0.32 | 0.79 | 12.92–15.33 | 7% |
| title-search-100k | Marimay native | 3.06 | 8.02 | 74.77–95.03 | 12% |
| title-search-100k | Go native | 0.44 | 0.85 | 14.68–19.11 | 9% |
| title-search-100k | Rust native | 0.25 | 0.61 | 18.17–24.29 | 4% |
| title-search-100k | Marimay VM (source run) | 8.93 | 80.76 | 608.73–775.86 | 13% |
| title-search-100k | Bun / TypeScript | 0.68 | 1.42 | 36.28–47.08 | 5% |
| normalize-title | Marimay native | 0.71 | 8.94 | 57.29–69.23 | 15% |
| normalize-title | Go native | 0.43 | 1.03 | 8.54–13.66 | 14% |
| normalize-title | Rust native | 0.31 | 2.24 | 10.62–33.84 | 20% |
| normalize-title | Marimay VM (source run) | 1.75 | 11.28 | 184.08–221.73 | 6% |
| normalize-title | Bun / TypeScript | 0.25 | 0.68 | 17.32–22.08 | 4% |
| word-frequency | Marimay native | 1.87 | 8.86 | 145.15–171.16 | 6% |
| word-frequency | Go native | 0.42 | 1.67 | 18.47–26.00 | 9% |
| word-frequency | Rust native | 0.89 | 2.21 | 29.84–34.65 | 9% |
| word-frequency | Marimay VM (source run) | 4.97 | 28.13 | 331.97–391.13 | 8% |
| word-frequency | Bun / TypeScript | 0.26 | 3.61 | 18.54–25.10 | 19% |
| report-builder | Marimay native | 0.57 | 1.60 | 14.83–20.77 | 11% |
| report-builder | Go native | 0.28 | 0.71 | 5.13–6.76 | 15% |
| report-builder | Rust native | 0.33 | 1.08 | 5.13–7.67 | 19% |
| report-builder | Marimay VM (source run) | 2.19 | 9.93 | 114.81–133.66 | 8% |
| report-builder | Bun / TypeScript | 0.75 | 2.03 | 11.77–17.25 | 17% |
| report-concat | Marimay native | 6.13 | 13.37 | 99.04–126.45 | 17% |
| report-concat | Go native | 3.87 | 8.69 | 79.84–118.83 | 14% |
| report-concat | Rust native | 1.61 | 7.74 | 48.30–59.89 | 15% |
| report-concat | Marimay VM (source run) | 11.01 | 35.93 | 195.72–247.54 | 17% |
| report-concat | Bun / TypeScript | 0.76 | 1.98 | 12.35–16.87 | 17% |
| map-int | Marimay native | 0.46 | 0.95 | 7.54–8.82 | 17% |
| map-int | Go native | 0.40 | 1.19 | 4.88–6.41 | 22% |
| map-int | Rust native | 0.28 | 0.66 | 5.76–7.80 | 13% |
| map-int | Marimay VM (source run) | 1.92 | 7.20 | 90.04–98.96 | 8% |
| map-int | Bun / TypeScript | 0.97 | 2.04 | 13.27–64.63 | 19% |
| map-string | Marimay native | 1.08 | 2.26 | 23.11–41.46 | 13% |
| map-string | Go native | 0.24 | 1.09 | 7.36–29.96 | 14% |
| map-string | Rust native | 0.77 | 1.92 | 9.70–18.04 | 22% |
| map-string | Marimay VM (source run) | 1.65 | 10.88 | 115.51–303.49 | 9% |
| map-string | Bun / TypeScript | 1.20 | 2.31 | 15.17–25.19 | 22% |
| map-string-book | Marimay native | 0.32 | 0.65 | 31.31–35.88 | 3% |
| map-string-book | Go native | 0.12 | 0.39 | 6.34–7.66 | 6% |
| map-string-book | Rust native | 0.26 | 0.57 | 10.15–12.87 | 7% |
| map-string-book | Marimay VM (source run) | 1.01 | 11.25 | 127.73–144.85 | 9% |
| map-string-book | Bun / TypeScript | 0.64 | 2.22 | 15.49–25.19 | 14% |
| set-string-churn | Marimay native | 0.89 | 1.34 | 47.24–103.92 | 5% |
| set-string-churn | Go native | 0.26 | 0.57 | 9.63–10.68 | 8% |
| set-string-churn | Rust native | 0.20 | 0.69 | 16.74–20.58 | 4% |
| set-string-churn | Marimay VM (source run) | 1.85 | 4.54 | 144.50–159.71 | 4% |
| set-string-churn | Bun / TypeScript | 0.41 | 1.08 | 17.61–21.40 | 7% |
| catalog-match | Marimay native | 0.13 | 0.62 | 13.25–14.99 | 5% |
| catalog-match | Go native | 0.21 | 0.50 | 4.73–5.42 | 12% |
| catalog-match | Rust native | 0.20 | 0.40 | 3.84–4.75 | 14% |
| catalog-match | Marimay VM (source run) | 1.26 | 3.52 | 193.80–216.67 | 2% |
| catalog-match | Bun / TypeScript | 0.25 | 0.41 | 18.77–20.64 | 4% |
| catalog-dotcall | Marimay native | 0.16 | 0.38 | 8.68–9.30 | 5% |
| catalog-dotcall | Go native | 0.13 | 0.35 | 4.55–5.35 | 8% |
| catalog-dotcall | Rust native | 0.14 | 0.52 | 3.59–4.53 | 14% |
| catalog-dotcall | Marimay VM (source run) | 1.09 | 2.21 | 155.69–159.73 | 2% |
| catalog-dotcall | Bun / TypeScript | 0.45 | 0.92 | 17.67–19.43 | 7% |
| catalog-interface | Marimay native | 0.27 | 0.51 | 12.03–31.18 | 6% |
| catalog-interface | Go native | 0.29 | 0.47 | 4.67–8.82 | 17% |
| catalog-interface | Rust native | 0.37 | 1.00 | 3.56–5.05 | 26% |
| catalog-interface | Marimay VM (source run) | 1.17 | 2.58 | 204.23–217.11 | 2% |
| catalog-interface | Bun / TypeScript | 0.31 | 0.61 | 17.93–29.83 | 5% |
| record-reference-copy | Marimay native | 0.30 | 0.60 | 6.63–16.37 | 12% |
| record-reference-copy | Go native | 0.31 | 0.74 | 4.20–10.95 | 20% |
| record-reference-copy | Rust native | 0.19 | 0.43 | 3.20–6.79 | 17% |
| record-reference-copy | Marimay VM (source run) | 2.03 | 6.09 | 121.26–246.24 | 5% |
| record-reference-copy | Bun / TypeScript | 0.47 | 1.43 | 14.24–34.84 | 10% |
| record-create | Marimay native | 0.45 | 0.75 | 23.13–51.06 | 6% |
| record-create | Go native | 0.26 | 0.84 | 6.78–19.21 | 12% |
| record-create | Rust native | 0.57 | 1.15 | 6.20–11.94 | 25% |
| record-create | Marimay VM (source run) | 5.54 | 9.97 | 155.65–300.78 | 10% |
| record-create | Bun / TypeScript | 0.75 | 1.47 | 15.44–54.33 | 13% |
| array-grow | Marimay native | 0.15 | 0.47 | 6.71–8.13 | 6% |
| array-grow | Go native | 0.31 | 0.95 | 6.48–7.81 | 14% |
| array-grow | Rust native | 0.44 | 1.00 | 4.14–5.45 | 29% |
| array-grow | Marimay VM (source run) | 2.12 | 7.91 | 126.02–148.52 | 6% |
| array-grow | Bun / TypeScript | 0.59 | 1.21 | 13.94–20.60 | 12% |
| array-reserved | Marimay native | 0.27 | 0.81 | 6.80–8.84 | 11% |
| array-reserved | Go native | 0.07 | 0.29 | 5.25–6.38 | 5% |
| array-reserved | Rust native | 0.27 | 0.61 | 3.94–4.88 | 18% |
| array-reserved | Marimay VM (source run) | 2.10 | 4.39 | 126.78–140.98 | 5% |
| array-reserved | Bun / TypeScript | 0.53 | 0.95 | 12.76–15.78 | 12% |
| bfs-int | Marimay native | 0.49 | 0.92 | 7.32–12.51 | 18% |
| bfs-int | Go native | 0.33 | 0.81 | 5.20–6.92 | 18% |
| bfs-int | Rust native | 0.13 | 0.56 | 4.26–5.16 | 11% |
| bfs-int | Marimay VM (source run) | 1.21 | 3.33 | 104.71–114.52 | 3% |
| bfs-int | Bun / TypeScript | 0.38 | 0.79 | 16.50–18.68 | 6% |
| bfs-record | Marimay native | 0.40 | 0.85 | 9.17–10.53 | 12% |
| bfs-record | Go native | 0.27 | 0.54 | 5.83–6.70 | 13% |
| bfs-record | Rust native | 0.21 | 0.58 | 4.59–5.46 | 12% |
| bfs-record | Marimay VM (source run) | 1.33 | 3.93 | 112.77–130.82 | 3% |
| bfs-record | Bun / TypeScript | 0.28 | 1.10 | 16.73–30.26 | 6% |
| catalog-pipeline | Marimay native | 0.40 | 0.83 | 37.45–67.09 | 3% |
| catalog-pipeline | Go native | 0.22 | 0.50 | 7.89–17.27 | 8% |
| catalog-pipeline | Rust native | 0.61 | 0.92 | 9.82–13.74 | 17% |
| catalog-pipeline | Marimay VM (source run) | 3.53 | 15.51 | 288.33–321.99 | 5% |
| catalog-pipeline | Bun / TypeScript | 0.56 | 0.86 | 22.52–42.55 | 7% |
| parse-result | Marimay native | 0.40 | 0.67 | 19.30–21.60 | 6% |
| parse-result | Go native | 0.36 | 0.88 | 5.77–7.51 | 17% |
| parse-result | Rust native | 0.23 | 0.80 | 5.85–8.09 | 13% |
| parse-result | Marimay VM (source run) | 2.67 | 8.99 | 107.91–239.28 | 8% |
| parse-result | Bun / TypeScript | 0.40 | 1.03 | 12.97–16.30 | 8% |
| insertion | Marimay native | 0.88 | 2.59 | 48.95–53.74 | 5% |
| insertion | Go native | 0.20 | 0.49 | 29.36–32.26 | 2% |
| insertion | Rust native | 0.50 | 1.06 | 28.39–31.66 | 5% |
| insertion | Marimay VM (source run) | 1.86 | 4.53 | 224.57–248.01 | 2% |
| insertion | Bun / TypeScript | 0.89 | 4.78 | 81.21–88.50 | 6% |
| heap-sort | Marimay native | 0.83 | 2.40 | 32.13–37.22 | 7% |
| heap-sort | Go native | 1.17 | 3.08 | 39.03–44.18 | 9% |
| heap-sort | Rust native | 0.46 | 1.80 | 21.90–25.55 | 8% |
| heap-sort | Marimay VM (source run) | 5.73 | 11.96 | 327.48–366.91 | 5% |
| heap-sort | Bun / TypeScript | 3.25 | 6.57 | 92.71–109.33 | 10% |
| stack | Marimay native | 0.39 | 0.83 | 9.92–14.73 | 11% |
| stack | Go native | 0.61 | 1.79 | 14.73–17.42 | 12% |
| stack | Rust native | 0.35 | 0.70 | 8.89–10.24 | 11% |
| stack | Marimay VM (source run) | 8.97 | 32.13 | 549.57–595.12 | 6% |
| stack | Bun / TypeScript | 0.63 | 1.93 | 21.74–28.71 | 8% |
| queue | Marimay native | 0.15 | 0.45 | 9.70–10.72 | 4% |
| queue | Go native | 0.11 | 0.30 | 14.93–15.90 | 2% |
| queue | Rust native | 0.29 | 0.72 | 8.91–10.21 | 9% |
| queue | Marimay VM (source run) | 8.26 | 18.72 | 569.52–636.88 | 4% |
| queue | Bun / TypeScript | 0.61 | 2.02 | 25.53–28.36 | 7% |
| min-heap | Marimay native | 0.86 | 1.54 | 46.86–55.57 | 5% |
| min-heap | Go native | 0.71 | 2.51 | 53.31–60.49 | 5% |
| min-heap | Rust native | 0.27 | 0.74 | 30.27–36.51 | 3% |
| min-heap | Marimay VM (source run) | 18.15 | 100.58 | 910.33–1112.75 | 11% |
| min-heap | Bun / TypeScript | 2.21 | 4.49 | 109.09–134.41 | 6% |
| dsu | Marimay native | 0.38 | 0.85 | 29.70–32.58 | 4% |
| dsu | Go native | 0.30 | 0.93 | 24.22–27.28 | 4% |
| dsu | Rust native | 0.31 | 0.62 | 21.52–25.15 | 4% |
| dsu | Marimay VM (source run) | 16.93 | 30.67 | 1375.64–1474.32 | 4% |
| dsu | Bun / TypeScript | 0.98 | 1.71 | 47.86–54.41 | 6% |
| bfs | Marimay native | 2.03 | 5.11 | 66.73–77.10 | 9% |
| bfs | Go native | 0.87 | 2.02 | 61.83–68.53 | 4% |
| bfs | Rust native | 0.23 | 0.59 | 23.94–25.25 | 3% |
| bfs | Marimay VM (source run) | 27.89 | 77.95 | 1418.67–1572.03 | 6% |
| bfs | Bun / TypeScript | 1.86 | 7.51 | 68.64–110.41 | 11% |
| dijkstra | Marimay native | 1.02 | 4.22 | 41.79–56.49 | 10% |
| dijkstra | Go native | 0.54 | 2.59 | 41.01–50.32 | 6% |
| dijkstra | Rust native | 0.17 | 1.49 | 39.31–42.18 | 4% |
| dijkstra | Marimay VM (source run) | 1.76 | 8.77 | 229.39–249.86 | 4% |
| dijkstra | Bun / TypeScript | 0.86 | 4.42 | 75.97–84.81 | 6% |
| frequency-map | Marimay native | 0.70 | 1.53 | 32.79–37.37 | 6% |
| frequency-map | Go native | 0.22 | 1.30 | 14.14–16.91 | 9% |
| frequency-map | Rust native | 2.25 | 4.36 | 16.28–23.81 | 35% |
| frequency-map | Marimay VM (source run) | 5.69 | 11.51 | 290.81–360.85 | 6% |
| frequency-map | Bun / TypeScript | 1.27 | 2.42 | 27.80–39.65 | 13% |
| unique-set | Marimay native | 0.28 | 0.61 | 26.83–29.62 | 3% |
| unique-set | Go native | 0.29 | 0.72 | 16.17–18.57 | 5% |
| unique-set | Rust native | 0.78 | 2.15 | 18.86–99.49 | 12% |
| unique-set | Marimay VM (source run) | 1.99 | 5.34 | 206.15–299.44 | 3% |
| unique-set | Bun / TypeScript | 0.47 | 1.16 | 21.12–25.83 | 6% |
| rune-frequency | Marimay native | 0.18 | 2.20 | 22.26–31.97 | 10% |
| rune-frequency | Go native | 0.44 | 1.32 | 5.71–7.34 | 21% |
| rune-frequency | Rust native | 0.31 | 0.67 | 5.78–11.00 | 14% |
| rune-frequency | Marimay VM (source run) | 1.52 | 6.11 | 159.74–192.27 | 4% |
| rune-frequency | Bun / TypeScript | 0.46 | 0.98 | 16.57–30.61 | 8% |

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

Command: `/workspace/Projects/Marimay/algorithm-benchmarks/benchmarks/catalog_bench.py --anchors --profile standard --warmups 3 --samples 11 --output /workspace/Projects/Marimay/marimay-vm-parity/build/jit-array-identity/comprehensive-current.md`
