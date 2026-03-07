# Phase 2 Report: OpenMP Parallelization

**Mini Project 1 — Memory Overload**
**Machine:** Apple M4 Max (16 cores: 4 Performance + 12 Efficiency), 36 GB RAM
**Dataset:** 41,741,361 NYC parking violation records (~13 GB CSV)

---

## 1. Overview

Phase 2 adds OpenMP parallelization to the Phase 1 serial engine. Three components were parallelized:

1. **CSV Parsing** — Chunked parallel parsing with thread-local TextPools
2. **Search Queries** — Thread-local accumulation patterns for filter and aggregation queries
3. **Index Building** — Parallel sections for concurrent sort operations

## 2. Parallelization Strategy

### 2.1 Search Queries (ParallelSearch Engine)

Two OpenMP patterns were implemented:

**Pattern A — Filter Queries (B1–B8):** Thread-local vector accumulation.
Each thread maintains its own `vector<size_t>` of matching indices. After the parallel scan, results are merged sequentially (count total → reserve once → copy).

```cpp
#pragma omp parallel
{
    int tid = omp_get_thread_num();
    auto& local = local_results[tid];
    #pragma omp for schedule(static)
    for (size_t i = 0; i < n; ++i) {
        if (predicate(records[i])) local.push_back(i);
    }
}
// merge: count → reserve → insert
```

**Pattern B — Aggregation Queries (B9–B10):** Thread-local fixed-size array reduction.
Each thread maintains a fixed-size count array (e.g., `array<size_t, 200>` for precincts). After the parallel scan, arrays are element-wise summed.

```cpp
#pragma omp parallel
{
    int tid = omp_get_thread_num();
    auto& local = thread_counts[tid];
    #pragma omp for schedule(static)
    for (size_t i = 0; i < n; ++i) {
        ++local[records[i].precinct];
    }
}
// reduce: sum across all thread arrays
```

### 2.2 CSV Parsing (Chunked Parallel)

The CSV parser reads data in chunks of 1M lines to cap memory overhead:

1. **Pass 1 (serial):** Count total lines for `reserve()`
2. **Pass 2 (chunked parallel):** For each chunk of 1M lines:
   - Read lines into `vector<string>` (serial I/O)
   - Parse records in parallel with thread-local TextPools
   - Merge thread-local TextPools into global pool via `append_bulk()`
   - Free chunk memory before reading next chunk

This keeps the temporary memory overhead at ~200 MB instead of ~8–9 GB (when buffering all 41.7M lines).

### 2.3 Index Building (Parallel Sections)

Four independent sort operations run concurrently using `#pragma omp parallel sections`:

```cpp
#pragma omp parallel sections
{
    #pragma omp section { std::sort(date_index_...); }
    #pragma omp section { std::sort(code_index_...); }
    #pragma omp section { std::sort(state_index_...); }
    #pragma omp section { std::sort(county_index_...); }
}
```

---

## 3. Search Thread Scaling Results

**Configuration:** 12 iterations per workload per thread count, with warmup run.
Data loaded once (~12 GB RSS), then thread count varied via `omp_set_num_threads()`.

### 3.1 Mean Execution Times (ms)

| Threads | B1 date_narrow | B2 date_wide | B3 code_common | B4 code_rare | B5 plate | B6 state_common | B7 state_rare | B8 county | B9 precinct_agg | B10 fy_agg |
|--------:|---------------:|-------------:|---------------:|-------------:|---------:|----------------:|--------------:|----------:|----------------:|-----------:|
| Serial¹ |         797.30 |       806.34 |         282.52 |       288.36 |   321.30 |          344.19 |        285.93 |    338.08 |          402.83 |     824.76 |
|       1 |         356.37 |       384.72 |         293.56 |       293.98 |   328.09 |          353.35 |        269.67 |    304.23 |          402.34 |     285.87 |
|       2 |         140.62 |       123.48 |         104.44 |       102.92 |   175.20 |          180.26 |        102.78 |    150.54 |          127.82 |     103.84 |
|       4 |          63.02 |        80.34 |          56.54 |        56.11 |   124.24 |          132.62 |         59.38 |    105.59 |           63.59 |      55.14 |
|       6 |          54.05 |        77.03 |          52.65 |        50.33 |   125.62 |          125.29 |         54.07 |     98.24 |           56.63 |      49.29 |
|       8 |          51.95 |        78.25 |          55.88 |        47.77 |   120.81 |          119.13 |         53.79 |     98.00 |           56.35 |      46.26 |
|      10 |          53.25 |        76.14 |          53.20 |        48.50 |   124.54 |          127.53 |         51.86 |     89.80 |           56.24 |      46.03 |
|      12 |          53.39 |        72.36 |          52.02 |        48.72 |   116.98 |          108.93 |         52.53 |     88.02 |           56.87 |      51.09 |
|      14 |          52.20 |        69.34 |          51.73 |        49.42 |   121.34 |          111.58 |         53.45 |     82.96 |           59.31 |      48.15 |
|      16 |          52.80 |        73.80 |          52.74 |        48.87 |   115.40 |          104.15 |         51.16 |     85.90 |           57.46 |      47.58 |

¹ Serial = LinearSearch engine (different code path; no thread-local allocation overhead)

### 3.2 Parallel Speedup (relative to ParallelSearch t=1)

This measures pure thread scaling by comparing the same algorithm at different thread counts:

| Threads | B1 | B2 | B3 | B4 | B5 | B6 | B7 | B8 | B9 | B10 | **Avg** |
|--------:|-----:|-----:|-----:|-----:|-----:|-----:|-----:|-----:|-----:|------:|--------:|
|       1 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |  1.00 | **1.00** |
|       2 | 2.53 | 3.12 | 2.81 | 2.86 | 1.87 | 1.96 | 2.62 | 2.02 | 3.15 |  2.75 | **2.57** |
|       4 | 5.65 | 4.79 | 5.19 | 5.24 | 2.64 | 2.66 | 4.54 | 2.88 | 6.33 |  5.19 | **4.51** |
|       6 | 6.59 | 4.99 | 5.58 | 5.84 | 2.61 | 2.82 | 4.99 | 3.10 | 7.10 |  5.80 | **4.94** |
|       8 | 6.86 | 4.92 | 5.25 | 6.15 | 2.72 | 2.97 | 5.01 | 3.10 | 7.14 |  6.18 | **5.03** |
|      10 | 6.69 | 5.05 | 5.52 | 6.06 | 2.63 | 2.77 | 5.20 | 3.39 | 7.15 |  6.21 | **5.07** |
|      12 | 6.68 | 5.32 | 5.64 | 6.03 | 2.80 | 3.24 | 5.13 | 3.46 | 7.07 |  5.60 | **5.10** |
|      14 | 6.83 | 5.55 | 5.68 | 5.95 | 2.70 | 3.17 | 5.04 | 3.67 | 6.78 |  5.94 | **5.13** |
|      16 | 6.75 | 5.21 | 5.57 | 6.02 | 2.84 | 3.39 | 5.27 | 3.54 | 7.00 |  6.01 | **5.16** |

### 3.3 Parallel Efficiency (Speedup / Threads)

| Threads | B1 | B2 | B3 | B4 | B5 | B6 | B7 | B8 | B9 | B10 | **Avg** |
|--------:|-----:|-----:|-----:|-----:|-----:|-----:|-----:|-----:|-----:|------:|--------:|
|       1 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |  1.00 | **1.00** |
|       2 | 1.27 | 1.56 | 1.41 | 1.43 | 0.94 | 0.98 | 1.31 | 1.01 | 1.57 |  1.38 | **1.28** |
|       4 | 1.41 | 1.20 | 1.30 | 1.31 | 0.66 | 0.67 | 1.14 | 0.72 | 1.58 |  1.30 | **1.13** |
|       6 | 1.10 | 0.83 | 0.93 | 0.97 | 0.44 | 0.47 | 0.83 | 0.52 | 1.18 |  0.97 | **0.82** |
|       8 | 0.86 | 0.61 | 0.66 | 0.77 | 0.34 | 0.37 | 0.63 | 0.39 | 0.89 |  0.77 | **0.63** |
|      10 | 0.67 | 0.51 | 0.55 | 0.61 | 0.26 | 0.28 | 0.52 | 0.34 | 0.72 |  0.62 | **0.51** |
|      12 | 0.56 | 0.44 | 0.47 | 0.50 | 0.23 | 0.27 | 0.43 | 0.29 | 0.59 |  0.47 | **0.42** |
|      14 | 0.49 | 0.40 | 0.41 | 0.43 | 0.19 | 0.23 | 0.36 | 0.26 | 0.48 |  0.42 | **0.37** |
|      16 | 0.42 | 0.33 | 0.35 | 0.38 | 0.18 | 0.21 | 0.33 | 0.22 | 0.44 |  0.38 | **0.32** |

### 3.4 Key Observations

**Strong scaling to 4 threads, diminishing returns after 6:**
- Most workloads achieve 4–6× speedup at 4 threads (efficiency > 1.0, indicating super-linear effects from cache)
- Beyond 6 threads, speedup plateaus at ~5–7× for most workloads
- This aligns with the M4 Max architecture: 4 performance cores provide the most compute per core, while efficiency cores add diminishing throughput

**Memory-bandwidth-bound workloads scale poorly:**
- B5 (plate lookup) and B6 (state=NY, 72% selectivity) plateau at ~2.6–3.4×
- These workloads have high selectivity (B6 returns 30M of 41M records), making the merge phase dominate
- The merge is serial (count → reserve → copy), creating an Amdahl's law bottleneck

**Low-selectivity queries scale best:**
- B4 (code=99, 13K matches) achieves 6.15× at 8 threads
- B9 (precinct aggregation) achieves 7.15× at 10 threads — the fixed-array reduction pattern is very efficient
- These workloads are pure scan with minimal merge overhead

**Super-linear speedup at 2–4 threads:**
- Efficiency > 1.0 at 2 and 4 threads for several workloads (e.g., B2: 1.56 at 2T)
- This is caused by better L2 cache utilization: each thread scans a smaller range that fits better in per-core cache

**Amdahl's Law Analysis:**

Using the formula: `f = (1/S - 1/p) / (1 - 1/p)` where S = speedup, p = threads:

For the average speedup of 5.16× at 16 threads:
- Serial fraction f ≈ 0.14 (14% of work is inherently serial)
- This serial fraction comes from: result merge, OpenMP fork/join overhead, memory allocation
- Theoretical max speedup (p→∞): 1/f ≈ 7.1×

### 3.5 Algorithm Speedup: ParallelSearch vs LinearSearch

ParallelSearch at 1 thread is already faster than LinearSearch for several workloads due to algorithmic differences:

| Workload | LinearSearch (ms) | ParallelSearch t=1 (ms) | Algo Speedup |
|----------|------------------:|------------------------:|-------------:|
| B1 date_narrow | 797.30 | 356.37 | 2.24× |
| B2 date_wide | 806.34 | 384.72 | 2.10× |
| B10 fy_agg | 824.76 | 285.87 | 2.89× |
| B3 code_common | 282.52 | 293.56 | 0.96× |
| B5 plate | 321.30 | 328.09 | 0.98× |

The 2–3× algorithmic improvement for B1, B2, and B10 comes from:
- **B10:** Fixed-size array reduction (`array<size_t, 50>`) vs `unordered_map` in LinearSearch
- **B1/B2:** Pre-allocated thread-local vectors avoid repeated reallocations

The **total speedup** (ParallelSearch at 16 threads vs LinearSearch serial) reaches:
- B1: 797.30 / 52.80 = **15.1×**
- B2: 806.34 / 73.80 = **10.9×**
- B10: 824.76 / 47.58 = **17.3×**
- B4: 288.36 / 48.87 = **5.9×**
- B9: 402.83 / 57.46 = **7.0×**

---

## 4. Load Time Thread Scaling Results

*CSV parsing with chunked parallel approach (1M lines per chunk).*

**Configuration:** 5 iterations per thread count, with warmup.

| Threads | Mean Load Time (s) | Speedup (total) | Parse Phase (s) | Parse Speedup | Throughput (rec/s) |
|--------:|-------------------:|----------------:|----------------:|--------------:|-------------------:|
|       1 |             170.34 |           1.00× |           31.54 |         1.00× |            244,853 |
|       2 |             153.34 |           1.11× |           16.33 |         1.93× |            272,102 |
|       4 |             146.98 |           1.16× |            9.41 |         3.35× |            283,854 |
|       8 |             144.43 |           1.18× |            5.38 |         5.86× |            289,012 |
|      16 |             139.82 |           1.22× |            4.47 |         7.06× |            298,521 |

### 4.1 Load Time Breakdown

The load pipeline has three phases, only one of which parallelizes:

| Phase | Description | t=1 Time (s) | t=16 Time (s) | Parallelizable? |
|-------|-------------|-------------:|--------------:|:---------------:|
| Pass 1: Line count | `std::getline()` loop counting lines | ~55 | ~52 | No (serial I/O) |
| Pass 2a: Read lines | `std::getline()` into `vector<string>` chunk | ~78 | ~76 | No (serial I/O) |
| Pass 2b: Parse fields | Field extraction + TextPool writes | ~32 | ~5 | **Yes (OpenMP)** |
| Pass 2c: Merge pools | `append_bulk()` + offset adjustment | ~6 | ~6 | No (serial) |

**Only 18.5% of load time (32s / 170s) is parallelizable.** The rest is serial file I/O.

By Amdahl's law: max speedup = 1 / (1 - 0.185 + 0.185/p) = **1.23× at p→∞**

Measured: **1.22× at 16 threads** — nearly at the theoretical limit.

### 4.2 Parse Phase Scaling

Isolating just the parse phase shows excellent scaling:

| Threads | Parse Time (s) | Parse Speedup | Parse Efficiency |
|--------:|---------------:|--------------:|-----------------:|
|       1 |          31.54 |         1.00× |            100%  |
|       2 |          16.33 |         1.93× |             97%  |
|       4 |           9.41 |         3.35× |             84%  |
|       8 |           5.38 |         5.86× |             73%  |
|      16 |           4.47 |         7.06× |             44%  |

The parse phase itself scales well to 8 threads (73% efficiency), confirming the chunked approach works correctly. The diminishing returns at 16 threads reflect M4 Max E-core performance characteristics.

---

## 5. Memory Analysis

| Metric | Value |
|--------|------:|
| Dataset size (CSV) | ~11 GB |
| ViolationRecord struct | 176 bytes |
| In-memory structs (41.7M × 176B) | 6.84 GB |
| TextPool (string data) | 3.21 GB |
| Peak RSS (serial baseline) | 11,792 MB |
| Peak RSS (thread scaling study) | 12,947 MB |
| Additional overhead per thread | ~70 MB (thread-local vectors/arrays) |

The chunked parsing approach keeps temporary memory bounded:
- **Old approach:** Buffered all 41.7M lines → ~8–9 GB extra temporary memory
- **New approach:** 1M-line chunks → ~200 MB extra temporary memory
- Net saving: **~8 GB reduction** in peak memory usage

---

## 6. Architecture Insights: Apple M4 Max

The M4 Max has a heterogeneous core layout:
- **4 Performance (P) cores:** High clock speed, large L2 cache, full SIMD width
- **12 Efficiency (E) cores:** Lower clock/power, shared L2 cache

This explains the scaling behavior:
- **1→4 threads:** Near-linear scaling as work maps to P-cores
- **4→6 threads:** Good gains as first E-cores join
- **6→16 threads:** Diminishing returns — E-cores provide less compute per core
- **Efficiency drops below 0.5 at 8+ threads** because E-cores contribute less per-thread throughput

The workload is memory-bandwidth bound (scanning 10 GB of data), so the shared memory bus becomes the bottleneck beyond 4–6 active cores.

---

## 7. Conclusions

1. **Search parallelization delivers 5–17× total speedup** over serial LinearSearch, combining algorithmic improvements with thread-level parallelism. Pure thread scaling provides 5–7× at 16 threads.

2. **Optimal thread count is 4–6 for search** on M4 Max. Beyond 6 threads, efficiency drops below 50% and the memory bus saturates. The 4 P-cores provide the majority of the speedup.

3. **Load parallelization is I/O-bound:** Only the parse phase (18.5% of load time) parallelizes. Total load speedup is limited to 1.22× at 16 threads, matching the Amdahl's law prediction of 1.23×. The parse phase itself scales well (7× at 16T).

4. **Data structure choice matters more than thread count:** The switch from `unordered_map` to fixed-size arrays (Pattern B) gave 2–3× improvement at 1 thread — more impact than doubling from 4 to 8 threads.

5. **Chunked parsing is essential:** The original approach of buffering all lines consumed ~8 GB extra RAM. Chunked parsing (1M lines per chunk) reduced this to ~200 MB with no performance penalty.

6. **Amdahl's law governs both systems:**
   - Search: ~14% serial fraction → max ~7× thread scaling
   - Load: ~81.5% serial fraction → max ~1.23× thread scaling
   - Combined with algorithmic improvements, search total speedup reaches 15–17×

---

## Appendix: Benchmark Configuration

- **Compiler:** Apple Clang with Homebrew libomp
- **OpenMP:** libomp (installed via `brew install libomp`)
- **Build:** CMake Release mode (`-O2`)
- **Search iterations:** 12 per workload per thread count
- **Load iterations:** 5 per thread count
- **Thread counts tested:** 1, 2, 4, 6, 8, 10, 12, 14, 16
- **Peak RSS measurement:** `getrusage(RUSAGE_SELF).ru_maxrss`
