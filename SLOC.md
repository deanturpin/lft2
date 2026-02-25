# Source Lines of Code

Generated with [SLOCCount](https://dwheeler.com/sloccount/) and `wc -l`.

| Language               | Files  | SLOC      |
|------------------------|--------|-----------|
| C++ (`src/`)           | 14     | 2,645     |
| Go (`cmd/`)            | 9      | 1,586     |
| Svelte/JS (`web/src/`) | 4      | 425       |
| **Total**              | **27** | **4,656** |

## COCOMO Estimate (C++ only)

SLOCCount applies the Basic COCOMO model to the C++ source:

| Metric             | Value                                   |
|--------------------|-----------------------------------------|
| Physical SLOC      | 2,645                                   |
| Effort             | 0.56 person-years (6.66 person-months)  |
| Schedule           | 0.43 years (5.14 months)                |
| Average developers | 1.30                                    |
| **Estimated cost** | **$75,022**                             |

*(Salary basis: $56,286/year + 2.4× overhead — COCOMO 1981 rates)*

Go and Svelte are not supported by SLOCCount natively. Scaling the
C++ COCOMO estimate by total SLOC ratio (4,656 / 2,645 ≈ 1.76×) gives
a rough whole-project estimate of **~$132,000**.
