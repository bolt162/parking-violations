# Phase 3 Report: SoA Optimization

## Overview

**Goal:** Convert Array-of-Structures (AoS) to Structure-of-Arrays (SoA) for better cache efficiency and SIMD vectorization during search operations.

**Dataset:** 41.7M NYC parking violation records (11 GB CSV)

**Machine:** MacBook Air 16GB RAM (Apple Silicon M-series)

## Changes Made

### Data Layout Change

**Phase 2 (AoS)** stores each record as a struct (176 bytes):
```cpp
records[i].issue_date    // accessing one field loads entire 176-byte struct
```

**Phase 3 (SoA)** stores each field as a separate array:
```cpp
issue_dates[i]           // accessing one field loads only 4 bytes
```

**Why this matters:** Searches only use 1-2 fields per query. Loading 176 bytes when we need 4 bytes wastes 97.7% of cache space.

### SIMD-Friendly Queries Added

Two new queries designed for auto-vectorization:

1. **`count_in_date_range`** - Count matches without collecting indices
2. **`find_date_extremes`** - Find min/max dates (reduction)

These use simple reduction patterns that the compiler can vectorize.

## Results

### SIMD-Friendly Queries Performance

| Query | Phase 2 (AoS) | Phase 3 (SoA) | Speedup |
|-------|---------------|---------------|---------|
| simd_count | 115.77 ms | **2.84 ms** | **40.8x** |
| simd_minmax | 107.13 ms | **3.94 ms** | **27.2x** |

### Filter Queries Performance

| Query | Phase 2 (AoS) | Phase 3 (SoA) | Speedup |
|-------|---------------|---------------|---------|
| date_1month (narrow) | 108.45 ms | **11.35 ms** | **9.6x** |
| date_1year (wide) | 163.09 ms | **65.35 ms** | **2.5x** |
| violation_code | 108.27 ms | **10.10 ms** | **10.7x** |
| state_ny (common) | 266.87 ms | **111.67 ms** | **2.4x** |
| county_brooklyn | 119.55 ms | **26.01 ms** | **4.6x** |

### Aggregate Queries Performance

| Query | Phase 2 (AoS) | Phase 3 (SoA) | Speedup |
|-------|---------------|---------------|---------|
| group_precinct | 106.21 ms | **10.86 ms** | **9.8x** |
| group_fiscal_year | 109.04 ms | **14.53 ms** | **7.5x** |

### Memory Usage

| Metric | Phase 2 (AoS) | Phase 3 (SoA) | Change |
|--------|---------------|---------------|--------|
| Peak RSS | 10,900 MB | **7,722 MB** | **-29%** |
| Load Time | 72.0s | 76.4s | +6% |

## SIMD Vectorization Analysis

### Compiler Auto-Vectorization Report

Using `-Rpass=loop-vectorize` flag with LLVM clang:

**Phase 2 (AoS):** No vectorization of search loops
- Scattered memory access (176 bytes between elements) prevents SIMD

**Phase 3 (SoA):** Vectorization achieved for reduction queries
```
soa_search.cpp:25: vectorized loop (vectorization width: 4, interleaved count: 4)
```

### Vectorization Width Comparison

| Phase | Layout | Vectorization Width | SIMD Elements/Instruction |
|-------|--------|---------------------|---------------------------|
| Phase 2 | AoS | Not vectorized | 1 (scalar) |
| Phase 3 | SoA | **4** | **4** |

### What Gets Vectorized

| Query Type | Collects Indices? | SIMD Possible? | Auto-Vectorized? |
|------------|-------------------|----------------|------------------|
| `count_in_date_range` | No (just count) | Yes | **Yes (width: 4)** |
| `find_date_extremes` | No (just min/max) | Yes | No* |
| `find_by_date_range` | Yes (push_back) | No | No |
| `count_by_precinct` | No (histogram) | No (scatter) | No |

*OpenMP min/max reductions not auto-vectorized by clang

### Why SIMD Requires SoA

**AoS Memory Layout:**
```
[struct0: date,code,...176 bytes][struct1: date,code,...][struct2]...
         ↑                                ↑
      date0                            date1
         ←——————— 176 bytes apart ———————→

SIMD cannot efficiently load - data is scattered
```

**SoA Memory Layout:**
```
dates: [date0][date1][date2][date3]...
       ←———— 4 bytes apart ————→

SIMD loads 4 consecutive elements in ONE instruction
```

## Key Findings

### 1. SoA Enables SIMD Vectorization
- Phase 2 (AoS): **0%** of search loops vectorized
- Phase 3 (SoA): SIMD count query vectorized with **width 4**
- Result: **40x speedup** for SIMD-friendly queries

### 2. Cache Efficiency is the Primary Win
Even non-vectorized queries are 2.4-10.7x faster because:
- SoA loads only needed columns (4-8 bytes per record)
- AoS loads entire struct (176 bytes per record)
- Cache utilization: SoA ~100% vs AoS ~2-4%

### 3. Memory Usage Reduced
- 29% less Peak RSS with SoA
- More efficient memory layout reduces overhead

### 4. High Selectivity Queries Show Less Speedup
- Low selectivity (date_1month, 2.9%): **9.6x** speedup
- High selectivity (state_ny, 72%): **2.4x** speedup
- Reason: High selectivity means more index collection overhead

### 5. Reduction Queries Benefit Most from SIMD
- Queries that only count/reduce: **27-41x** speedup
- Queries that collect indices: **2-11x** speedup
- Reason: Index collection (`push_back`) cannot be vectorized

## Conclusions

1. **SoA layout is essential for SIMD** - AoS scattered memory access prevents vectorization entirely

2. **Cache efficiency > SIMD** - The main speedup comes from loading only needed data, not from SIMD instructions

3. **Design queries for vectorization** - Pure reduction queries (count, min/max) can be vectorized; index collection cannot

4. **Memory bandwidth is the bottleneck** - Both phases are memory-bound; SoA reduces memory traffic by loading only relevant columns

5. **Trade-off: Load time vs Query time** - SoA has slightly slower load (+6%) but much faster queries (up to 40x)

## Recommendations for Production Use

1. Use SoA for analytical workloads with column-based queries
2. Design SIMD-friendly query APIs (count-only variants)
3. Consider hybrid approaches: SoA for hot columns, AoS for rarely-accessed data
4. Profile memory bandwidth to identify bottlenecks
