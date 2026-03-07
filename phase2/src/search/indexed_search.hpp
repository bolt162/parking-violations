#ifndef PARKING_INDEXED_SEARCH_HPP
#define PARKING_INDEXED_SEARCH_HPP

#include "search/search_engine.hpp"
#include <vector>

namespace parking::search {

/// Optimized search using pre-built sorted index arrays.
///
/// After construction, builds a sorted index for each searchable field.
/// Range queries use std::lower_bound / std::upper_bound for O(log n)
/// lookup instead of O(n) linear scan.
///
/// Memory overhead: one uint32_t per record per indexed field.
/// For 41.7M records × 5 indices = ~834 MB extra.
///
/// Index build time: O(n log n) per field, one-time cost after loading.
class IndexedSearch : public SearchEngine {
public:
    IndexedSearch() = default;
    ~IndexedSearch() override = default;

    const char* name() const override { return "IndexedSearch"; }

    /// Build all sorted indices from the DataStore. Must be called once
    /// after loading data, before any queries.
    void build_indices(const data::DataStore& store);

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

    AggregateResult count_by_precinct(
        const data::DataStore& store) override;

    AggregateResult count_by_fiscal_year(
        const data::DataStore& store) override;

    /// Time spent building indices, in milliseconds.
    double build_time_ms() const { return build_time_ms_; }

private:
    // Each index is a vector of record indices, sorted by the field value.
    // E.g., date_index_[0] is the index of the record with the smallest issue_date.

    std::vector<uint32_t> date_index_;       // sorted by issue_date
    std::vector<uint32_t> code_index_;       // sorted by violation_code
    std::vector<uint32_t> state_index_;      // sorted by registration_state
    std::vector<uint32_t> county_index_;     // sorted by violation_county

    double build_time_ms_ = 0.0;
    bool indices_built_ = false;
};

} // namespace parking::search

#endif // PARKING_INDEXED_SEARCH_HPP
