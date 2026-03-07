#include "search/parallel_search.hpp"
#include "core/text_ref.hpp"
#include "data/text_pool.hpp"
#include <omp.h>
#include <chrono>
#include <cstring>
#include <array>
#include <vector>

namespace parking::search {

// ── Helper: start/stop timer ────────────────────────────────────────────────

using Clock = std::chrono::high_resolution_clock;

static inline auto now() { return Clock::now(); }

static inline double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ── Helper: merge thread-local vectors into result ──────────────────────────

static void merge_thread_results(
    std::vector<std::vector<size_t>>& local_results,
    SearchResult& result)
{
    // Count total matches across all threads
    size_t total = 0;
    for (const auto& v : local_results) {
        total += v.size();
    }

    // Reserve once, merge sequentially (thread 0 first → ascending order)
    result.indices.reserve(total);
    for (auto& v : local_results) {
        result.indices.insert(result.indices.end(), v.begin(), v.end());
    }
}

// ── Filter queries (Pattern A: thread-local vector accumulation) ────────────

SearchResult ParallelSearch::search_by_date_range(
    const data::DataStore& store,
    uint32_t start_date, uint32_t end_date)
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
            uint32_t d = recs[i].issue_date;
            if (d >= start_date && d <= end_date) {
                local.push_back(i);
            }
        }
    }

    merge_thread_results(local_results, result);
    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult ParallelSearch::search_by_violation_code(
    const data::DataStore& store,
    uint16_t code)
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
            if (recs[i].violation_code == code) {
                local.push_back(i);
            }
        }
    }

    merge_thread_results(local_results, result);
    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult ParallelSearch::search_by_plate(
    const data::DataStore& store,
    const data::TextPool& pool,
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
            uint8_t len = recs[i].str_lengths[core::SF_PLATE_ID];
            if (len == plate_len) {
                const char* p = pool.get(recs[i].str_offsets[core::SF_PLATE_ID]);
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
    const data::DataStore& store,
    uint8_t state_enum)
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
            if (recs[i].registration_state == state_enum) {
                local.push_back(i);
            }
        }
    }

    merge_thread_results(local_results, result);
    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult ParallelSearch::search_by_county(
    const data::DataStore& store,
    uint8_t county_enum)
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
            if (recs[i].violation_county == county_enum) {
                local.push_back(i);
            }
        }
    }

    merge_thread_results(local_results, result);
    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

// ── Aggregation queries (Pattern B: thread-local array reduction) ───────────

AggregateResult ParallelSearch::count_by_precinct(
    const data::DataStore& store)
{
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    const size_t n = recs.size();
    result.total_scanned = n;

    static constexpr size_t MAX_PRECINCT = 200;
    int max_threads = omp_get_max_threads();

    // Thread-local count arrays — avoids any synchronization during the scan
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

    // Collect non-zero entries
    for (uint16_t p = 0; p < MAX_PRECINCT; ++p) {
        if (final_counts[p] > 0) {
            result.counts.emplace_back(p, final_counts[p]);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

AggregateResult ParallelSearch::count_by_fiscal_year(
    const data::DataStore& store)
{
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    const size_t n = recs.size();
    result.total_scanned = n;

    // Fiscal years range ~2015-2030 — use a small fixed array instead of hash map
    // Index = fiscal_year - 2000 (handles years 2000-2049)
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

} // namespace parking::search
