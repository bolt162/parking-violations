# Phase 3 Report: SoA Optimization

## Overview

**Goal:** Convert Array-of-Structures (AoS) to Structure-of-Arrays (SoA) for better cache efficiency during search operations.

**Dataset:** 41.7M NYC parking violation records

**Machines:**
- Development: MacBook Air 16GB RAM (Apple Silicon)
- Benchmarks: M4 Max 36GB RAM

## Changes Made

### Data Layout Change

Phase 2 stores each record as a struct (176 bytes):
```
records[i].issue_date    // accessing one field loads 176 bytes
```

Phase 3 stores each field as a separate array:
```
issue_dates[i]           // accessing one field loads 4 bytes
```

**Why:** Searches only use 6 fields. Loading 176 bytes when we need 4 wastes cache space.

## Results

### Load Time

| Phase | Time | Records/sec |
|-------|------|-------------|
| Phase 1 (serial) | 97.7s | 427K |
| Phase 2 (parallel) | TBD | TBD |
| Phase 3 (SoA serial) | TBD | TBD |
| Phase 3 (SoA parallel) | TBD | TBD |

### Search Time (B1-B10)

| Workload | Phase 2 | Phase 3 | Change |
|----------|---------|---------|--------|
| B1 date_narrow | TBD | TBD | TBD |
| B2 date_wide | TBD | TBD | TBD |
| B3 code_common | TBD | TBD | TBD |
| B4 code_rare | TBD | TBD | TBD |
| B5 plate | TBD | TBD | TBD |
| B6 state_common | TBD | TBD | TBD |
| B7 state_rare | TBD | TBD | TBD |
| B8 county | TBD | TBD | TBD |
| B9 precinct_agg | TBD | TBD | TBD |
| B10 fy_agg | TBD | TBD | TBD |

### Memory Usage

| Phase | Peak RSS |
|-------|----------|
| Phase 2 | ~12 GB |
| Phase 3 | TBD |

## Observations

### Step 1: Setup
- Created phase3 directory structure
- Copied record.hpp (enums) from phase2
- Created CMakeLists.txt with OpenMP support

(More observations will be added as we implement each step)

## Conclusions

(To be written after benchmarks)
