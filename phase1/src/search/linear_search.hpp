#ifndef PARKING_LINEAR_SEARCH_HPP
#define PARKING_LINEAR_SEARCH_HPP

#include "search/search_engine.hpp"

namespace parking::search {

/// Serial linear scan implementation of SearchEngine.
///
/// Every query iterates over ALL records in the DataStore.
/// This is the Phase 1 baseline; Phase 2 will parallelize these loops
/// with OpenMP by creating a ParallelSearch subclass.
class LinearSearch : public SearchEngine {
public:
    LinearSearch() = default;
    ~LinearSearch() override = default;

    const char* name() const override { return "LinearSearch"; }

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
};

} // namespace parking::search

#endif // PARKING_LINEAR_SEARCH_HPP
