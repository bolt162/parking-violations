#include "search.hpp"
#include <chrono>
#include <cstring>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <omp.h>

namespace parking {

// --- Timing helpers ---

using Clock = std::chrono::high_resolution_clock;

static inline auto now() { return Clock::now(); }

static inline double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// --- merge_thread_results: combine thread-local vectors into result ---

static void merge_thread_results(
    std::vector<std::vector<size_t>>& local_results,
    SearchResult& result)
{
    size_t total = 0;
    for (const auto& v : local_results) {
        total += v.size();
    }

    result.indices.reserve(total);
    for (auto& v : local_results) {
        result.indices.insert(result.indices.end(), v.begin(), v.end());
    }
}

// --- parallel_scan_filter: eliminates boilerplate in parallel filter queries ---
// Pred takes (const ViolationRecord&) and returns bool.
// Uses thread-local vector accumulation with static scheduling.

template <typename Pred>
static SearchResult parallel_scan_filter(const DataStore& store, Pred pred) {
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    const size_t n = recs.size();
    result.total_scanned = n;

    int max_threads = omp_get_max_threads();
    std::vector<std::vector<size_t>> local_results(max_threads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto& local = local_results[tid];

        #pragma omp for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            if (pred(recs[i])) {
                local.push_back(i);
            }
        }
    }

    merge_thread_results(local_results, result);
    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

// --- ParallelSearch ---

SearchResult ParallelSearch::search_by_date_range(
    const DataStore& store, uint32_t start_date, uint32_t end_date)
{
    return parallel_scan_filter(store, [=](const ViolationRecord& r) {
        return r.issue_date >= start_date && r.issue_date <= end_date;
    });
}

SearchResult ParallelSearch::search_by_violation_code(
    const DataStore& store, uint16_t code)
{
    return parallel_scan_filter(store, [=](const ViolationRecord& r) {
        return r.violation_code == code;
    });
}

SearchResult ParallelSearch::search_by_plate(
    const DataStore& store, const TextPool& pool,
    const char* plate_id, int plate_len)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    const size_t n = recs.size();
    result.total_scanned = n;

    int max_threads = omp_get_max_threads();
    std::vector<std::vector<size_t>> local_results(max_threads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto& local = local_results[tid];

        #pragma omp for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            uint8_t len = recs[i].str_lengths[SF_PLATE_ID];
            if (len == plate_len) {
                const char* p = pool.get(recs[i].str_offsets[SF_PLATE_ID]);
                if (std::memcmp(p, plate_id, len) == 0) {
                    local.push_back(i);
                }
            }
        }
    }

    merge_thread_results(local_results, result);
    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult ParallelSearch::search_by_state(
    const DataStore& store, uint8_t state_enum)
{
    return parallel_scan_filter(store, [=](const ViolationRecord& r) {
        return r.registration_state == state_enum;
    });
}

SearchResult ParallelSearch::search_by_county(
    const DataStore& store, uint8_t county_enum)
{
    return parallel_scan_filter(store, [=](const ViolationRecord& r) {
        return r.violation_county == county_enum;
    });
}

// Aggregation queries (Pattern B: thread-local array reduction)

AggregateResult ParallelSearch::count_by_precinct(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    const size_t n = recs.size();
    result.total_scanned = n;

    static constexpr size_t MAX_PRECINCT = 200;
    int max_threads = omp_get_max_threads();

    std::vector<std::array<size_t, MAX_PRECINCT>> thread_counts(max_threads);
    for (auto& tc : thread_counts) { tc.fill(0); }

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto& counts = thread_counts[tid];

        #pragma omp for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            uint16_t p = recs[i].violation_precinct;
            if (p < MAX_PRECINCT) {
                counts[p]++;
            }
        }
    }

    // Reduce: sum all thread arrays element-wise
    std::array<size_t, MAX_PRECINCT> final_counts{};
    for (const auto& tc : thread_counts) {
        for (size_t p = 0; p < MAX_PRECINCT; ++p) {
            final_counts[p] += tc[p];
        }
    }

    for (uint16_t p = 0; p < MAX_PRECINCT; ++p) {
        if (final_counts[p] > 0) {
            result.counts.emplace_back(p, final_counts[p]);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

AggregateResult ParallelSearch::count_by_fiscal_year(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    const size_t n = recs.size();
    result.total_scanned = n;

    // Fiscal years range ~2015-2030, use a small fixed array
    static constexpr int FY_BASE = 2000;
    static constexpr size_t FY_SLOTS = 50;
    int max_threads = omp_get_max_threads();

    std::vector<std::array<size_t, FY_SLOTS>> thread_counts(max_threads);
    for (auto& tc : thread_counts) { tc.fill(0); }

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        auto& counts = thread_counts[tid];

        #pragma omp for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            int idx = static_cast<int>(recs[i].fiscal_year) - FY_BASE;
            if (idx >= 0 && idx < static_cast<int>(FY_SLOTS)) {
                counts[idx]++;
            }
        }
    }

    // Reduce
    std::array<size_t, FY_SLOTS> final_counts{};
    for (const auto& tc : thread_counts) {
        for (size_t j = 0; j < FY_SLOTS; ++j) {
            final_counts[j] += tc[j];
        }
    }

    for (size_t j = 0; j < FY_SLOTS; ++j) {
        if (final_counts[j] > 0) {
            result.counts.emplace_back(
                static_cast<uint16_t>(j + FY_BASE), final_counts[j]);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

// --- IndexedSearch ---

void IndexedSearch::build_indices(const DataStore& store) {
    auto t0 = now();
    size_t n = store.size();
    const auto& recs = store.records();

    std::cout << "  Building sorted indices for " << n << " records..." << std::endl;

    auto make_index = [n]() -> std::vector<uint32_t> {
        std::vector<uint32_t> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        return idx;
    };

    // Pre-allocate all 4 index vectors
    date_index_   = make_index();
    code_index_   = make_index();
    state_index_  = make_index();
    county_index_ = make_index();

    // Sort all 4 indices concurrently using OpenMP parallel sections
    double date_ms = 0, code_ms = 0, state_ms = 0, county_ms = 0;

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            auto t = now();
            std::sort(date_index_.begin(), date_index_.end(),
                      [&recs](uint32_t a, uint32_t b) {
                          return recs[a].issue_date < recs[b].issue_date;
                      });
            date_ms = elapsed_ms(t, now());
        }

        #pragma omp section
        {
            auto t = now();
            std::sort(code_index_.begin(), code_index_.end(),
                      [&recs](uint32_t a, uint32_t b) {
                          return recs[a].violation_code < recs[b].violation_code;
                      });
            code_ms = elapsed_ms(t, now());
        }

        #pragma omp section
        {
            auto t = now();
            std::sort(state_index_.begin(), state_index_.end(),
                      [&recs](uint32_t a, uint32_t b) {
                          return recs[a].registration_state < recs[b].registration_state;
                      });
            state_ms = elapsed_ms(t, now());
        }

        #pragma omp section
        {
            auto t = now();
            std::sort(county_index_.begin(), county_index_.end(),
                      [&recs](uint32_t a, uint32_t b) {
                          return recs[a].violation_county < recs[b].violation_county;
                      });
            county_ms = elapsed_ms(t, now());
        }
    }

    std::cout << "    date_index:   " << date_ms << " ms" << std::endl;
    std::cout << "    code_index:   " << code_ms << " ms" << std::endl;
    std::cout << "    state_index:  " << state_ms << " ms" << std::endl;
    std::cout << "    county_index: " << county_ms << " ms" << std::endl;

    build_time_ms_ = elapsed_ms(t0, now());
    indices_built_ = true;

    double mem_mb = (4.0 * n * 4) / (1024.0 * 1024.0);
    std::cout << "  Indices built in " << (build_time_ms_ / 1000.0)
              << "s, memory: " << mem_mb << " MB" << std::endl;
}

// Binary-search-based queries

SearchResult IndexedSearch::search_by_date_range(
    const DataStore& store, uint32_t start_date, uint32_t end_date)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        date_index_.begin(), date_index_.end(), start_date,
        [&recs](uint32_t idx, uint32_t val) {
            return recs[idx].issue_date < val;
        });

    auto hi = std::upper_bound(
        date_index_.begin(), date_index_.end(), end_date,
        [&recs](uint32_t val, uint32_t idx) {
            return val < recs[idx].issue_date;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_violation_code(
    const DataStore& store, uint16_t code)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        code_index_.begin(), code_index_.end(), code,
        [&recs](uint32_t idx, uint16_t val) {
            return recs[idx].violation_code < val;
        });

    auto hi = std::upper_bound(
        code_index_.begin(), code_index_.end(), code,
        [&recs](uint16_t val, uint32_t idx) {
            return val < recs[idx].violation_code;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_plate(
    const DataStore& store, const TextPool& pool,
    const char* plate_id, int plate_len)
{
    // Plate has too many unique values for a practical index, fall back to scan
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        uint8_t len = recs[i].str_lengths[SF_PLATE_ID];
        if (len == plate_len) {
            const char* p = pool.get(recs[i].str_offsets[SF_PLATE_ID]);
            if (std::memcmp(p, plate_id, len) == 0) {
                result.indices.push_back(i);
            }
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_state(
    const DataStore& store, uint8_t state_enum)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        state_index_.begin(), state_index_.end(), state_enum,
        [&recs](uint32_t idx, uint8_t val) {
            return recs[idx].registration_state < val;
        });

    auto hi = std::upper_bound(
        state_index_.begin(), state_index_.end(), state_enum,
        [&recs](uint8_t val, uint32_t idx) {
            return val < recs[idx].registration_state;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_county(
    const DataStore& store, uint8_t county_enum)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        county_index_.begin(), county_index_.end(), county_enum,
        [&recs](uint32_t idx, uint8_t val) {
            return recs[idx].violation_county < val;
        });

    auto hi = std::upper_bound(
        county_index_.begin(), county_index_.end(), county_enum,
        [&recs](uint8_t val, uint32_t idx) {
            return val < recs[idx].violation_county;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

// Aggregation queries (still full-scan even with indices)

AggregateResult IndexedSearch::count_by_precinct(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    std::array<size_t, 200> counts{};
    for (size_t i = 0; i < recs.size(); ++i) {
        uint16_t p = recs[i].violation_precinct;
        if (p < counts.size()) counts[p]++;
    }

    for (uint16_t p = 0; p < counts.size(); ++p) {
        if (counts[p] > 0) result.counts.emplace_back(p, counts[p]);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

AggregateResult IndexedSearch::count_by_fiscal_year(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    std::unordered_map<uint16_t, size_t> counts;
    counts.reserve(4);
    for (size_t i = 0; i < recs.size(); ++i) {
        counts[recs[i].fiscal_year]++;
    }

    for (auto& [fy, cnt] : counts) {
        result.counts.emplace_back(fy, cnt);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

} // namespace parking
