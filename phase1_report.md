# Phase 1 Report: Serial Baseline Engine for NYC Parking Violations

## 1. Project Overview

**Goal**: Build a serial C++ engine that loads ~41.7 million NYC parking violation records (~11.15 GB CSV, 44 columns) into memory using primitive types, provides search/aggregation APIs, and benchmarks as a baseline for Phase 2 (OpenMP) and Phase 3 (SoA vectorization).

**System**: Apple M4 Max, 36 GB RAM, Apple Clang 17.0 (ARM64), `-O3` optimization.

**Dataset**: 41,741,361 records across 3 fiscal years (FY2023-FY2025), 44 columns per row, merged from NYC OpenData.

---

## 2. Architecture

### 2.1 OOP Design (Facade + Strategy + Abstract Base)

```
ParkingAPI (Facade)
  |-- DataStore (owns records + TextPool)
  |-- FileLoader -> CsvReader (polymorphic)
  |-- SearchEngine* (abstract)
        |-- LinearSearch   (serial scan)
        |-- IndexedSearch  (sorted index + binary search)
```

- **ParkingAPI**: Single entry point. Hides all subsystems. Users call `load()`, `find_by_*()`, `count_by_*()`.
- **DataStore**: Owns `vector<ViolationRecord>` and a `TextPool` (global string pool).
- **SearchEngine**: Abstract base class with virtual methods; enables polymorphic swapping between LinearSearch and IndexedSearch without changing calling code.
- **CsvReader**: Abstract base with `SingleFileReader` concrete implementation.

### 2.2 Data Model: ViolationRecord (176 bytes)

Each CSV row maps to one `ViolationRecord` struct. Fields are stored as:

| Category | Count | Storage | Bytes |
|---|---|---|---|
| Numeric fields | 15 | `uint64/32/16` | 50 |
| Enum-encoded fields | 8 | `uint8_t` via lookup tables | 8 |
| String field refs | 21 | `uint32_t offset + uint8_t length` into TextPool | 105 |
| vtable pointer + padding | - | compiler-added | 13 |
| **Total** | **44** | | **176** |

**Low-cardinality encoding**: 8 fields with small value sets (e.g., registration_state has 65 codes, violation_county has 5 boroughs) are mapped to `uint8_t` enums via compile-time lookup tables, reducing each from a variable-length string to 1 byte.

### 2.3 String Pool Architecture (TextPool)

All 21 string fields across all 41.7M records are stored in a single contiguous `std::string`:

```
TextPool (one std::string):
  "GBB1234SUBNFORDMAIN ST...XYZ999SEDNHONDA5TH AVE..."
   ^                       ^
   record 0's text fields  record 1's text fields

Record 0:
  str_offsets[SF_PLATE_ID] = 0     -> pool[0..6]  = "GBB1234"
  str_lengths[SF_PLATE_ID] = 7
  str_offsets[SF_VEHICLE_MAKE] = 11 -> pool[11..14] = "FORD"
  str_lengths[SF_VEHICLE_MAKE] = 4
```

**Key properties**:
- Zero padding waste (each field stores only its exact characters)
- Only 2 large heap allocations total (records vector + pool string)
- ~70 bytes average text content per record
- Single `reserve()` call pre-allocates based on line count

---

## 3. Discovery Process and Key Findings

### 3.1 Initial Implementation: Bulk File Read + Fixed char[] Arrays

Our first implementation used an aggressive approach for maximum speed:
- **Bulk I/O**: Read the entire 11.15 GB CSV into a single `char*` buffer with one `read()` system call
- **Zero-copy parsing**: `FieldView` (pointer + length) pointed directly into the file buffer, avoiding string copies during parsing
- **Fixed char[] arrays**: Each string field stored as a fixed-size `char[]` in the struct (e.g., `char plate_id[10]`, `char vehicle_make[6]`, `char violation_description[66]`)

**Results**:
- **Load time**: 38.87 seconds (mean over 12 runs)
- **Throughput**: 1,073,993 records/sec
- **sizeof(ViolationRecord)**: 352 bytes
- **Peak RSS**: ~22 GB

### 3.2 Finding 1: Why Peak RSS Was 22 GB (Double Buffering)

We discovered that peak memory occurred during parsing when **both** the file buffer and all records existed simultaneously:

```
File buffer (full CSV):           11.15 GB
Records (41.7M x 352 bytes):     13.68 GB
                                  ────────
Peak RSS during parsing:          ~22 GB
```

After parsing completed and the file buffer was freed, RSS dropped to ~13.68 GB, but peak RSS (as reported by `getrusage`) had already been recorded.

### 3.3 Finding 2: In-Memory Structs Larger Than CSV File

Our struct size (352 bytes) was **larger** than the average CSV row (~267 bytes) because of fixed `char[]` padding:

| Field | Max actual length | char[] size | Waste per record |
|---|---|---|---|
| `violation_description` | 71 chars | `char[66]` | -5 (buffer overflow!) |
| `house_number` | 12 chars | `char[10]` | -2 (buffer overflow!) |
| `street_name` | 20 chars | `char[20]` | 0 |
| `plate_id` | 10 chars | `char[10]` | ~3 avg |
| `vehicle_make` | 5 chars | `char[6]` | ~2 avg |
| ... (21 string fields total) | | 234 bytes | ~164 bytes avg |

This meant 41.7M records x 352 bytes = **13.68 GB** in-memory, which was **more than the 11.15 GB CSV file itself**. Other groups achieved in-memory sizes below file size.

**Critical bug discovered**: `violation_description[66]` was too small for the maximum value of 71 characters, and `house_number[10]` was too small for the maximum of 12. These caused silent buffer overflows in the original implementation.

### 3.4 Finding 3: Professor Feedback -- Phase 1 Should Be "Basic Baseline"

The professor clarified that Phase 1 should use standard line-by-line I/O (not bulk read), since the purpose is establishing a baseline that Phase 2 and Phase 3 improve upon. Our 40-second load was too fast for a baseline.

### 3.5 Finding 4: String Pool Approach (Inspired by Peer Discussion)

Through discussion with classmates (Group 2), we discovered the **string pool / arena pattern**:

- Group 2's approach: Use `getline()` per row, parse fields into temporary `StringView` objects, then append string values to a global `std::string`. Store `(offset, length)` pairs in the record instead of `char[]`.
- **Result**: Only the exact characters are stored (no padding), and only 2 large heap allocations are needed (records vector + pool string).
- We adopted this approach, naming our types `TextPool` and `StringField` (instead of their `StringRef`).

### 3.6 The Refactoring

We refactored the entire codebase from bulk-read + char[] to getline + TextPool:

| Aspect | Before (Bulk) | After (TextPool) |
|---|---|---|
| I/O strategy | Single `read()` of entire file | `getline()` line-by-line, two-pass |
| String storage | Fixed `char[]` arrays (234 bytes) | `(offset, length)` into pool (105 bytes) |
| sizeof(ViolationRecord) | 352 bytes | 176 bytes |
| Records memory | 41.7M x 352 = 13.68 GB | 41.7M x 176 = 6.84 GB |
| String data memory | Included in struct (padded) | TextPool: ~2.9 GB (exact lengths) |
| File buffer | 11.15 GB during parsing | ~300 bytes (one line) |
| **Expected Peak RSS** | **~22 GB** | **~10 GB** |
| Load time | ~39 seconds | ~1-2 minutes (expected) |
| Buffer overflow bugs | 2 fields too small | Impossible (exact lengths) |

**Files changed**: 15 files modified, 2 new files created. All 100 unit tests pass (70 parser + 30 search).

---

## 4. Benchmark Results (Pre-Refactor Baseline)

These results were collected with the bulk-read implementation over 12 iterations. They serve as comparison data for the refactored version and for Phase 2/3 improvements.

### 4.1 Load Benchmark

| Metric | Value |
|---|---|
| Records loaded | 41,741,361 |
| Mean load time | 38,865.59 ms (38.87 s) |
| Std deviation | 850.85 ms |
| Throughput | 1,073,993 records/sec |
| sizeof(ViolationRecord) | 352 bytes |
| Peak RSS | ~22 GB |

### 4.2 Search Benchmarks: LinearSearch (Serial Scan)

LinearSearch scans all 41.7M records sequentially for every query.

| Workload | Description | Mean Time (ms) | Std Dev | Matched | Throughput (rec/s) |
|---|---|---|---|---|---|
| B1 | Date range (Jan 2024) | 562.19 | 14.76 | 1,524,456 | 74.2M |
| B2 | Date range (all 2024) | 571.58 | 5.44 | 16,745,498 | 73.0M |
| B3 | Violation code 46 (common) | 152.03 | 3.48 | 3,459,036 | 274.6M |
| B4 | Violation code 99 (rare) | 147.11 | 3.17 | 2,243 | 283.7M |
| B5 | Plate lookup | 438.20 | 18.74 | 1 | 95.3M |
| B6 | State=NY (very common) | 220.33 | 0.94 | 37,186,395 | 189.4M |
| B7 | State=FL (rare) | 148.71 | 2.34 | 104,597 | 280.7M |
| B8 | County=BK (Brooklyn) | 197.62 | 2.10 | 7,923,199 | 211.2M |
| B9 | Count by precinct (agg) | 196.29 | 4.67 | 119 precincts | 212.7M |
| B10 | Count by fiscal year (agg) | 598.86 | 36.79 | 3 years | 69.7M |

**Observations**:
- Small integer fields (violation_code, state, county) scan at 190-284M rec/s due to cache efficiency
- Date range and plate searches are slower (74-95M rec/s) because they access larger fields
- All queries are O(n) -- every record is examined regardless of selectivity

### 4.3 Search Benchmarks: IndexedSearch (Sorted Indices + Binary Search)

IndexedSearch builds sorted index arrays (one `uint32_t` per record per field) and uses `std::lower_bound`/`std::upper_bound` for O(log n) lookups.

| Workload | Description | Mean Time (ms) | Matched | Speedup vs Linear |
|---|---|---|---|---|
| B1 | Date range (Jan 2024) | 0.73 | 1,524,456 | **767x** |
| B2 | Date range (all 2024) | 10.04 | 16,745,498 | **57x** |
| B3 | Violation code 46 (common) | 0.61 | 3,459,036 | **249x** |
| B4 | Violation code 99 (rare) | 0.0079 | 2,243 | **18,621x** |
| B5 | Plate lookup (linear fallback) | 463.69 | 1 | 0.95x |
| B6 | State=NY (very common) | 18.49 | 37,186,395 | **12x** |
| B7 | State=FL (rare) | 0.66 | 104,597 | **225x** |
| B8 | County=BK (Brooklyn) | 5.45 | 7,923,199 | **36x** |
| B9 | Count by precinct (agg) | 201.68 | 119 precincts | 0.97x |
| B10 | Count by fiscal year (agg) | 622.08 | 3 years | 0.96x |

**Index build time**: ~24 seconds (one-time cost), ~834 MB additional memory (4 indices x 41.7M x 4 bytes).

**Key insight**: Speedup is inversely proportional to selectivity. Rare lookups (code 99: 2,243 matches out of 41.7M) see 18,621x speedup because binary search finds the small range in O(log n) = ~25 comparisons. Common lookups (state=NY: 89% of records) see only 12x speedup because most of the index range must still be copied. Aggregations and plate lookups show no speedup (plate falls back to linear scan; aggregations must visit every record regardless).

---

## 5. Memory Analysis

### 5.1 Before Refactor (Bulk Read + char[])

```
Component                         Size
─────────────────────────────────────────
CSV file buffer (bulk read)       11.15 GB   (freed after parsing)
Records: 41.7M x 352 bytes       13.68 GB
Index arrays (IndexedSearch)       0.83 GB   (optional)
─────────────────────────────────────────
Peak RSS during parsing:          ~22 GB
Steady-state after parsing:       ~14.5 GB
```

### 5.2 After Refactor (getline + TextPool)

```
Component                         Size
─────────────────────────────────────────
Line buffer (getline)             ~300 bytes  (one line at a time)
Records: 41.7M x 176 bytes        6.84 GB
TextPool: ~70 bytes/record         2.92 GB
Index arrays (IndexedSearch)       0.83 GB   (optional)
─────────────────────────────────────────
Expected Peak RSS:                ~10 GB
```

**Memory reduction**: 22 GB -> ~10 GB (**55% reduction**).

### 5.3 Why TextPool Uses Less Memory Than char[] Arrays

The 21 string fields in the old struct used 234 bytes per record due to fixed-size `char[]` arrays sized for the maximum possible value. Most actual values are much shorter:

- `violation_description[66]`: average ~20 chars, wastes ~46 bytes/record
- `street_name[20]`: average ~8 chars, wastes ~12 bytes/record
- `no_standing_stopping[10]`, `hydrant_violation[10]`, `double_parking[10]`: always empty (0 chars), waste 30 bytes/record combined

With TextPool, only the actual characters are stored. The overhead is 5 bytes per field (4-byte offset + 1-byte length) = 105 bytes for the references, plus ~70 bytes average actual text content = ~175 bytes total vs 234 bytes of padded char[] arrays.

---

## 6. I/O Strategy Comparison

### 6.1 Bulk Read (Original -- Not Baseline Appropriate)

```
1. read(fd, buffer, file_size)      -- one syscall, 11.15 GB into RAM
2. Scan buffer for line boundaries   -- pointer arithmetic
3. Parse each line in-place          -- FieldView points into buffer
4. Copy fields into ViolationRecord  -- memcpy into char[]
5. Free file buffer                  -- 11.15 GB freed
```

- **Pros**: Extremely fast (39s), minimal syscall overhead, zero-copy parsing
- **Cons**: Requires 2x file size in RAM, not representative of a baseline

### 6.2 Line-by-Line getline (Refactored -- Baseline)

```
Pass 1:
  while (getline(file, line)) line_count++   -- count for pre-allocation

Pass 2:
  records.reserve(line_count)
  pool.reserve(line_count * 70)
  while (getline(file, line)) {
      parse_line(line) -> FieldView[]        -- temporary, per-line
      populate_record(fields, rec, pool)     -- append strings to pool
      records.push_back(rec)
  }
```

- **Pros**: Constant memory overhead (~300 bytes line buffer), scalable to any file size, standard approach
- **Cons**: Slower (~1-2 min expected) due to per-line memory allocation in `getline`
- **Pre-allocation**: Two-pass approach (count lines first, then parse) avoids vector reallocation during loading

---

## 7. Search Engine Design

### 7.1 LinearSearch (Baseline)

Simple `for` loop over all records, checking the filter condition:

```cpp
for (size_t i = 0; i < recs.size(); ++i) {
    if (recs[i].violation_code == code)
        result.indices.push_back(i);
}
```

Time complexity: O(n) for all queries. Performance depends on struct size (cache line utilization) and field access pattern.

### 7.2 IndexedSearch (Optimized)

Builds sorted index arrays at load time:

```cpp
// Build: sort indices by field value
vector<uint32_t> code_index_ = {0, 1, 2, ..., n-1};
sort(code_index_, [&](a, b) { return recs[a].violation_code < recs[b].violation_code; });

// Query: binary search for range boundaries
auto lo = lower_bound(code_index_, target_code, comparator);
auto hi = upper_bound(code_index_, target_code, comparator);
// All matches are in [lo, hi)
```

Time complexity: O(log n + k) where k = number of matches. Build time: O(n log n) per index, one-time cost.

**Plate search**: Falls back to linear scan because plate_id has millions of unique values, making an index impractical for this phase.

---

## 8. Testing

| Test Suite | Tests | Status |
|---|---|---|
| test_parser (ViolationRecord, CsvParser, enums) | 70 | All pass |
| test_search (LinearSearch, IndexedSearch, cross-validation) | 30 | All pass |
| **Total** | **100** | **All pass** |

Cross-validation tests confirm that LinearSearch and IndexedSearch produce identical result counts for every query type.

---

## 9. Key Takeaways

1. **Fixed char[] arrays cause significant memory waste** -- padding for maximum field length wastes ~164 bytes per record across 41.7M records = ~6.8 GB wasted.

2. **String pool (arena) pattern eliminates padding waste** -- storing all text in one contiguous buffer with (offset, length) references reduces string storage from 234 bytes to ~175 bytes per record.

3. **Bulk file reads create double-buffering problems** -- holding the entire file in memory while also building the record array causes peak RSS = file_size + records_size.

4. **Line-by-line I/O trades speed for memory** -- `getline()` uses ~300 bytes vs 11.15 GB for bulk read, at the cost of slower I/O.

5. **Sorted indices provide dramatic speedups for selective queries** -- up to 18,621x for rare value lookups, but no benefit for aggregations or high-selectivity filters.

6. **Smaller structs improve linear scan performance** -- 176-byte structs fit ~23 records per 4KB cache line vs ~11 with 352-byte structs, potentially doubling throughput for numeric field scans.

7. **Two-pass file reading enables optimal pre-allocation** -- counting lines first allows `vector::reserve()` and `string::reserve()` to allocate exactly once, avoiding reallocation overhead during parsing.


Here's the full summary of the new benchmark results:

---

## Load Benchmark

| Metric | Old (Bulk Read) | New (getline + TextPool) | Change |
|---|---|---|---|
| Load time (mean) | 38.87 s | 97.70 s | 2.5x slower (expected baseline) |
| Throughput | 1,073,993 rec/s | 427,238 rec/s | 2.5x slower |
| sizeof(ViolationRecord) | 352 bytes | **176 bytes** | **50% smaller** |
| In-memory structs | 13.68 GB | 6.84 GB | 50% smaller |
| TextPool | N/A | 3.21 GB | — |
| Peak RSS (with searches) | ~22 GB | **~11 GB** | **50% reduction** |
| Breakdown: Pass 1 (count lines) | N/A | ~36s | — |
| Breakdown: Pass 2 (parse) | N/A | ~62s | — |

Loading takes ~1.6 minutes now — that's the proper "basic baseline" the professor wanted. The two-pass approach spends ~36s counting lines and ~62s parsing. Peak RSS dropped from 22 GB to 11 GB because we no longer hold the 11 GB file buffer in memory.

---

## Linear Search — The Big Win

The smaller struct size (176 vs 352 bytes) gave **massive** search speedups due to better cache utilization:

| Workload | Old (ms) | New (ms) | **Speedup** |
|---|---|---|---|
| B1 Date narrow (Jan 2024) | 562.19 | 342.08 | **1.64x faster** |
| B2 Date wide (all 2024) | 571.58 | 352.88 | **1.62x faster** |
| B3 Code 46 (common) | 152.03 | 121.45 | **1.25x faster** |
| B4 Code 99 (rare) | 147.11 | 115.54 | **1.27x faster** |
| B5 Plate lookup | 438.20 | 214.97 | **2.04x faster** |
| B6 State=NY (common) | 220.33 | 221.77 | ~same |
| B7 State=FL (rare) | 148.71 | 122.06 | **1.22x faster** |
| B8 County=BK | 197.62 | 197.74 | ~same |
| B9 Precinct agg | 196.29 | 168.38 | **1.17x faster** |
| B10 FY agg | 598.86 | 369.58 | **1.62x faster** |

**Why**: A 176-byte struct fits ~23 records per 4KB cache line vs ~11 with a 352-byte struct. This means nearly 2x more records are in L1/L2 cache during each scan, directly translating to faster searches. The plate search (B5) got the biggest boost at **2x faster** despite now requiring a pointer chase into the TextPool.

---

## Indexed Search

Binary-search queries are unchanged (they only touch a few records), but full-scan fallbacks benefit from the smaller struct:

| Workload | Old (ms) | New (ms) | Notes |
|---|---|---|---|
| B1-B4 (binary search) | 0.01–0.73 | 0.01–0.74 | ~same (expected) |
| B5 Plate (linear fallback) | 463.69 | 226.41 | **2.05x faster** |
| B9 Precinct agg (full scan) | 201.68 | 167.41 | **1.20x faster** |
| B10 FY agg (full scan) | 622.08 | 367.05 | **1.69x faster** |

Index build time: **9.43s**, index memory: **637 MB**.

---

## Key Takeaway

We traded load speed (39s → 98s) for a **50% memory reduction** (22 GB → 11 GB) and got **1.2–2x faster searches for free** from the smaller struct's cache effects. This is exactly the baseline the professor wanted — a straightforward approach that Phase 2 (OpenMP) and Phase 3 (SoA) can clearly improve upon.