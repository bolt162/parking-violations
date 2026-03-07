#ifndef PARKING_SEARCH_ENGINE_HPP
#define PARKING_SEARCH_ENGINE_HPP

#include <cstdint>
#include "data/data_store.hpp"
#include "search/search_result.hpp"

namespace parking::search {

/// Abstract search interface.
///
/// Defines the query API that all search implementations must provide.
/// This enables polymorphic swapping between:
///   - LinearSearch    (Phase 1 baseline — serial brute force)
///   - IndexedSearch   (Phase 1 optimized — sorted indices + binary search)
///   - ParallelSearch  (Phase 2 — OpenMP parallelized linear scan)
class SearchEngine {
public:
    SearchEngine() = default;
    virtual ~SearchEngine() = default;

    /// Name of this search implementation (for benchmarking output).
    virtual const char* name() const = 0;

    // ── Filter queries (return matching record indices) ─────────────────

    /// Records where issue_date is in [start_date, end_date].
    virtual SearchResult search_by_date_range(
        const data::DataStore& store,
        uint32_t start_date, uint32_t end_date) = 0;

    /// Records matching an exact violation_code.
    virtual SearchResult search_by_violation_code(
        const data::DataStore& store,
        uint16_t code) = 0;

    /// Records matching an exact plate_id (string comparison).
    /// Requires TextPool reference since plate_id is stored in the pool.
    virtual SearchResult search_by_plate(
        const data::DataStore& store,
        const data::TextPool& pool,
        const char* plate_id, int plate_len) = 0;

    /// Records matching a registration_state enum value.
    virtual SearchResult search_by_state(
        const data::DataStore& store,
        uint8_t state_enum) = 0;

    /// Records matching a violation_county enum value.
    virtual SearchResult search_by_county(
        const data::DataStore& store,
        uint8_t county_enum) = 0;

    // ── Aggregation queries (return category counts) ────────────────────

    /// Count violations per precinct.
    virtual AggregateResult count_by_precinct(
        const data::DataStore& store) = 0;

    /// Count violations per fiscal year.
    virtual AggregateResult count_by_fiscal_year(
        const data::DataStore& store) = 0;
};

} // namespace parking::search

#endif // PARKING_SEARCH_ENGINE_HPP
