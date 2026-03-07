#ifndef PARKING_PARALLEL_SEARCH_HPP
#define PARKING_PARALLEL_SEARCH_HPP

#include "search/search_engine.hpp"

namespace parking::search {

/// OpenMP-parallelized linear scan implementation of SearchEngine.
///
/// Uses thread-local accumulation patterns to avoid synchronization:
///   - Filter queries:  thread-local vectors merged after parallel scan
///   - Aggregations:    thread-local arrays reduced element-wise
///
/// Thread count controlled via omp_set_num_threads() or OMP_NUM_THREADS env.
class ParallelSearch : public SearchEngine {
public:
    ParallelSearch() = default;
    ~ParallelSearch() override = default;

    const char* name() const override { return "ParallelSearch"; }

    // ── Filter queries ────────────────────────────────────────────────

    SearchResult search_by_date_range(
        const data::DataStore& store,
        uint32_t start_date, uint32_t end_date) override;

    SearchResult search_by_violation_code(
        const data::DataStore& store,
        uint16_t code) override;

    SearchResult search_by_plate(
        const data::DataStore& store,
        const data::TextPool& pool,
        const char* plate_id, int plate_len) override;

    SearchResult search_by_state(
        const data::DataStore& store,
        uint8_t state_enum) override;

    SearchResult search_by_county(
        const data::DataStore& store,
        uint8_t county_enum) override;

    // ── Aggregation queries ───────────────────────────────────────────

    AggregateResult count_by_precinct(
        const data::DataStore& store) override;

    AggregateResult count_by_fiscal_year(
        const data::DataStore& store) override;
};

} // namespace parking::search

#endif // PARKING_PARALLEL_SEARCH_HPP
