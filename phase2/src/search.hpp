#ifndef PARKING_SEARCH_HPP
#define PARKING_SEARCH_HPP

#include <cstdint>
#include <vector>
#include "parking.hpp"

namespace parking {

// Abstract search interface for polymorphic swapping between
// ParallelSearch (OpenMP linear scan) and IndexedSearch (sorted + binary search).
class SearchEngine {
public:
    SearchEngine() = default;
    virtual ~SearchEngine() = default;

    virtual const char* name() const = 0;

    // Filter queries -- return matching record indices
    virtual SearchResult search_by_date_range(
        const DataStore& store, uint32_t start_date, uint32_t end_date) = 0;

    virtual SearchResult search_by_violation_code(
        const DataStore& store, uint16_t code) = 0;

    virtual SearchResult search_by_plate(
        const DataStore& store, const TextPool& pool,
        const char* plate_id, int plate_len) = 0;

    virtual SearchResult search_by_state(
        const DataStore& store, uint8_t state_enum) = 0;

    virtual SearchResult search_by_county(
        const DataStore& store, uint8_t county_enum) = 0;

    // Aggregation queries -- return category counts
    virtual AggregateResult count_by_precinct(const DataStore& store) = 0;
    virtual AggregateResult count_by_fiscal_year(const DataStore& store) = 0;
};

// OpenMP-parallelized linear scan over all records.
// Uses thread-local accumulation patterns to avoid synchronization:
//   Filter queries:  thread-local vectors merged after parallel scan
//   Aggregations:    thread-local arrays reduced element-wise
// Thread count controlled via omp_set_num_threads() or OMP_NUM_THREADS env.
class ParallelSearch : public SearchEngine {
public:
    const char* name() const override { return "ParallelSearch"; }

    SearchResult search_by_date_range(
        const DataStore& store, uint32_t start_date, uint32_t end_date) override;
    SearchResult search_by_violation_code(
        const DataStore& store, uint16_t code) override;
    SearchResult search_by_plate(
        const DataStore& store, const TextPool& pool,
        const char* plate_id, int plate_len) override;
    SearchResult search_by_state(
        const DataStore& store, uint8_t state_enum) override;
    SearchResult search_by_county(
        const DataStore& store, uint8_t county_enum) override;

    AggregateResult count_by_precinct(const DataStore& store) override;
    AggregateResult count_by_fiscal_year(const DataStore& store) override;
};

// Pre-built sorted indices with O(log n) binary search lookups.
// Memory overhead: one uint32_t per record per indexed field.
// Index build uses OpenMP parallel sections for concurrent sorting.
class IndexedSearch : public SearchEngine {
public:
    const char* name() const override { return "IndexedSearch"; }

    // Build sorted indices from loaded data. Must be called once before queries.
    void build_indices(const DataStore& store);

    SearchResult search_by_date_range(
        const DataStore& store, uint32_t start_date, uint32_t end_date) override;
    SearchResult search_by_violation_code(
        const DataStore& store, uint16_t code) override;
    SearchResult search_by_plate(
        const DataStore& store, const TextPool& pool,
        const char* plate_id, int plate_len) override;
    SearchResult search_by_state(
        const DataStore& store, uint8_t state_enum) override;
    SearchResult search_by_county(
        const DataStore& store, uint8_t county_enum) override;

    AggregateResult count_by_precinct(const DataStore& store) override;
    AggregateResult count_by_fiscal_year(const DataStore& store) override;

    double build_time_ms() const { return build_time_ms_; }

private:
    std::vector<uint32_t> date_index_;
    std::vector<uint32_t> code_index_;
    std::vector<uint32_t> state_index_;
    std::vector<uint32_t> county_index_;

    double build_time_ms_ = 0.0;
    bool indices_built_ = false;
};

} // namespace parking

#endif // PARKING_SEARCH_HPP
